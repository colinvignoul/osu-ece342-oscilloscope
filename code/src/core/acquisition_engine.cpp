// Purpose: Implements trigger detection and frame construction for interleaved
// two-channel ADC sample blocks or history ranges.
// Interface: AcquisitionEngine accepts borrowed sample spans and exposes a
// stable, engine-owned ScopeFrame once a full display-width capture completes.
// Constraints: Input words must be ordered CH1 then CH2, ADC error words are
// skipped, and all buffers are fixed-size for embedded use.
// Ownership: This file mutates only AcquisitionEngine-owned state; sample input
// remains owned by AdcSampler or the test caller.

#include "acquisition_engine.hpp"
#include "trigger_logic.hpp"

namespace picoscope {
namespace {

TriggerEvent capture_origin_event(bool triggered, const TriggerSettings &trigger)
{
    return make_origin_event(triggered ? FrameOrigin::Trigger : FrameOrigin::Auto,
                             trigger);
}

ColumnSampleBucket sample_column(const std::uint16_t raw[2], const bool valid[2])
{
    ColumnSampleBucket column = {};
    for (std::uint8_t ch = 0; ch < 2u; ++ch) {
        if (valid[ch]) {
            column.valid[ch] = true;
            column.min_count[ch] = raw[ch];
            column.max_count[ch] = raw[ch];
        }
    }
    return column;
}

ColumnSampleBucket interpolated_column(const std::uint16_t previous_raw[2],
                                       const bool previous_valid[2],
                                       const std::uint16_t current_raw[2],
                                       const bool current_valid[2],
                                       std::uint16_t phase,
                                       std::uint16_t phases)
{
    ColumnSampleBucket column = {};
    for (std::uint8_t ch = 0; ch < 2u; ++ch) {
        if (!previous_valid[ch] || !current_valid[ch]) {
            continue;
        }
        const std::uint32_t weighted =
            static_cast<std::uint32_t>(previous_raw[ch]) *
                static_cast<std::uint32_t>(phases - phase) +
            static_cast<std::uint32_t>(current_raw[ch]) *
                static_cast<std::uint32_t>(phase);
        const std::uint16_t value =
            static_cast<std::uint16_t>((weighted + phases / 2u) / phases);
        column.valid[ch] = true;
        column.min_count[ch] = value;
        column.max_count[ch] = value;
    }
    return column;
}

} // namespace

// Constructor clears frame and bucket state and returns a ready engine.
AcquisitionEngine::AcquisitionEngine()
{
    clear_frame();
    clear_bucket();
    clear_pretrigger_columns();
}

// Takes current scope settings and a clear-frame flag, resets trigger/capture
// state for the selected timebase.
void AcquisitionEngine::reset(const ScopeSettings &settings, bool clear_frame_contents)
{
    configure_timebase(settings);
    mode_ = Mode::WaitingForTrigger;
    frame_ready_ = false;
    bucket_count_ = 0;
    column_index_ = 0;
    trigger_wait_pairs_ = 0;
    trigger_armed_ = false;
    trigger_arm_sample_count_ = 0;
    previous_trigger_valid_ = false;
    previous_trigger_count_ = 0;
    trigger_opposite_holdoff_pairs_ = 0;
    clear_bucket();
    clear_interpolation_state();
    clear_pretrigger_columns();
    if (clear_frame_contents) {
        clear_frame();
    }
}

// Takes borrowed interleaved sample words, their word count, and current
// settings; detects triggers and fills frame columns. 
void AcquisitionEngine::process_interleaved(const std::uint16_t *samples,
                                            std::uint32_t word_count,
                                            const ScopeSettings &settings)
{
    if (samples == nullptr || word_count < 2u || !settings.running || frame_ready_) {
        return;
    }

    const std::uint16_t requested_decimation = timebase_decimation(settings);
    const std::uint16_t requested_interpolation =
        timebase_interpolation_factor(settings);
    if (requested_decimation != active_decimation_ ||
        requested_interpolation != active_interpolation_factor_) {
        reset(settings, false);
    }

    const std::uint8_t trigger_channel = channel_index(settings.trigger.source);
    const std::uint32_t frame_pairs =
        static_cast<std::uint32_t>(active_decimation_) * config::kDisplayWidth;
    const std::uint32_t auto_trigger_pairs =
        auto_trigger_timeout_pairs(frame_pairs,
                                   timebase_sample_pairs_per_second(settings));
    const std::uint8_t trigger_arm_samples = trigger_arm_dwell_samples(settings);
    const std::uint32_t trigger_holdoff_pairs =
        trigger_opposite_edge_holdoff_pairs(settings);

    // The sampler round-robins CH1 then CH2, so acquisition consumes complete
    // channel pairs. Auto-trigger keeps the display alive when the signal never
    // crosses the configured trigger level.
    for (std::uint32_t i = 0; i + 1u < word_count; i += 2u) {
        const std::uint16_t words[2] = {samples[i], samples[i + 1u]};
        const bool valid[2] = {adc_word_valid(words[0]), adc_word_valid(words[1])};
        const std::uint16_t raw[2] = {adc_word_value(words[0]), adc_word_value(words[1])};

        if (mode_ == Mode::WaitingForTrigger) {
            const bool previous_valid = previous_trigger_valid_;
            const std::uint16_t previous_count = previous_trigger_count_;
            bool crossed = false;
            bool holdoff_active = trigger_opposite_holdoff_pairs_ > 0u;
            if (valid[trigger_channel]) {
                if (previous_valid &&
                    trigger_opposite_edge_crossed(previous_count,
                                                  raw[trigger_channel],
                                                  settings.trigger)) {
                    trigger_opposite_holdoff_pairs_ =
                        trigger_holdoff_pairs;
                    holdoff_active = true;
                }
                const bool raw_crossed = schmitt_trigger_crossed(trigger_armed_,
                                                                 trigger_arm_sample_count_,
                                                                 raw[trigger_channel],
                                                                 settings.trigger,
                                                                 trigger_arm_samples);
                const bool pretrigger_ready =
                    interpolation_active()
                        ? interpolated_pretrigger_ready_for_endpoint()
                        : pretrigger_count_ >= config::kDefaultTriggerColumn;
                crossed = pretrigger_ready &&
                          !holdoff_active &&
                          raw_crossed;
            }

            ++trigger_wait_pairs_;
            if (crossed || trigger_wait_pairs_ >= auto_trigger_pairs) {
                if (interpolation_active()) {
                    track_interpolated_pretrigger_pair(raw, valid, false);
                }
                const TriggerEvent trigger_event =
                    crossed ? make_trigger_event(settings.trigger,
                                                 previous_valid,
                                                 previous_count,
                                                 raw[trigger_channel])
                            : make_origin_event(FrameOrigin::Auto, settings.trigger);
                begin_capture(trigger_event, true);
                if (interpolation_active()) {
                    capture_interpolated_pair(raw, valid);
                } else {
                    capture_pair(raw, valid);
                }
            } else {
                if (interpolation_active()) {
                    track_interpolated_pretrigger_pair(raw, valid, true);
                } else {
                    track_pretrigger_pair(raw, valid);
                }
            }
            if (valid[trigger_channel]) {
                if (trigger_opposite_holdoff_pairs_ > 0u) {
                    --trigger_opposite_holdoff_pairs_;
                }
                previous_trigger_valid_ = true;
                previous_trigger_count_ = raw[trigger_channel];
            }
        } else {
            if (interpolation_active()) {
                capture_interpolated_pair(raw, valid);
            } else {
                capture_pair(raw, valid);
            }
        }

        if (frame_ready_) {
            break;
        }
    }
}

// Takes interleaved words and settings, starts capture immediately, and returns
// with frame_ready true only when enough words filled the display frame.
void AcquisitionEngine::capture_interleaved(const std::uint16_t *samples,
                                            std::uint32_t word_count,
                                            const ScopeSettings &settings,
                                            bool triggered)
{
    if (samples == nullptr || word_count < 2u || !settings.running) {
        return;
    }

    configure_timebase(settings);
    begin_capture(capture_origin_event(triggered, settings.trigger), false);
    capture_words(samples, word_count);
}

// Takes a wrapped history range, captures each contiguous span in order.
void AcquisitionEngine::capture_history_range(const AdcHistoryRange &range,
                                              const ScopeSettings &settings,
                                              bool triggered)
{
    capture_history_range(range,
                          settings,
                          capture_origin_event(triggered, settings.trigger));
}

// Takes a wrapped history range plus trigger diagnostics, captures each
// contiguous span in chronological order.
void AcquisitionEngine::capture_history_range(const AdcHistoryRange &range,
                                              const ScopeSettings &settings,
                                              const TriggerEvent &trigger_event)
{
    if (!settings.running || range.span_count == 0u) {
        return;
    }

    configure_timebase(settings);
    begin_capture(trigger_event, false);
    for (std::uint8_t i = 0; i < range.span_count && i < 2u; ++i) {
        capture_words(range.spans[i].words, range.spans[i].word_count);
        if (frame_ready_) {
            break;
        }
    }
}

// Takes current scope settings, marks the rendered frame consumed, resets
// trigger wait state, and returns nothing.
void AcquisitionEngine::acknowledge_frame(const ScopeSettings &settings)
{
    frame_ready_ = false;
    mode_ = Mode::WaitingForTrigger;
    trigger_wait_pairs_ = 0;
    trigger_armed_ = false;
    trigger_arm_sample_count_ = 0;
    previous_trigger_valid_ = false;
    previous_trigger_count_ = 0;
    trigger_opposite_holdoff_pairs_ = 0;
    configure_timebase(settings);
    clear_bucket();
    clear_interpolation_state();
    clear_pretrigger_columns();
}

// Takes trigger diagnostics, clears output state, and enters capture mode.
void AcquisitionEngine::begin_capture(const TriggerEvent &trigger_event,
                                      bool copy_pretrigger)
{
    clear_frame();
    if (copy_pretrigger) {
        copy_pretrigger_columns_to_frame();
    }
    clear_bucket();
    column_index_ = copy_pretrigger ? config::kDefaultTriggerColumn : 0u;
    bucket_count_ = 0;
    frame_.triggered = trigger_event.origin == FrameOrigin::Trigger;
    frame_.trigger_event = trigger_event;
    frame_.complete = false;
    mode_ = Mode::Capturing;
    clear_interpolation_state();
}

// Takes current settings, caches active decimation/interpolation policy.
void AcquisitionEngine::configure_timebase(const ScopeSettings &settings)
{
    active_decimation_ = timebase_decimation(settings);
    active_interpolation_factor_ = timebase_interpolation_factor(settings);
}

// Takes no inputs and returns whether the active timebase interpolates columns.
bool AcquisitionEngine::interpolation_active() const
{
    return active_interpolation_factor_ > 1u;
}

// Takes one raw CH1/CH2 pair plus validity flags, folds valid samples into the
// min/max bucket, and commits full buckets.
void AcquisitionEngine::capture_pair(const std::uint16_t raw[2], const bool valid[2])
{
    // Each display column summarizes active_decimation_ sample pairs. Keeping
    // min and max preserves narrow spikes after horizontal compression.
    for (std::uint8_t ch = 0; ch < 2u; ++ch) {
        if (!valid[ch]) {
            continue;
        }
        if (!bucket_valid_[ch]) {
            bucket_min_[ch] = raw[ch];
            bucket_max_[ch] = raw[ch];
            bucket_valid_[ch] = true;
        } else {
            if (raw[ch] < bucket_min_[ch]) {
                bucket_min_[ch] = raw[ch];
            }
            if (raw[ch] > bucket_max_[ch]) {
                bucket_max_[ch] = raw[ch];
            }
        }
    }

    ++bucket_count_;
    if (bucket_count_ >= active_decimation_) {
        commit_bucket();
    }
}

// Takes one raw CH1/CH2 pair plus validity flags, emits exact/interpolated
// display columns for fractional timebases.
void AcquisitionEngine::capture_interpolated_pair(const std::uint16_t raw[2],
                                                  const bool valid[2])
{
    if (!interpolation_has_previous_) {
        append_frame_column(sample_column(raw, valid));
        for (std::uint8_t ch = 0; ch < 2u; ++ch) {
            interpolation_previous_raw_[ch] = raw[ch];
            interpolation_previous_valid_[ch] = valid[ch];
        }
        interpolation_has_previous_ = true;
        return;
    }

    for (std::uint16_t phase = 1u;
         phase <= active_interpolation_factor_ && !frame_ready_;
         ++phase) {
        const ColumnSampleBucket column =
            phase == active_interpolation_factor_
                ? sample_column(raw, valid)
                : interpolated_column(interpolation_previous_raw_,
                                      interpolation_previous_valid_,
                                      raw,
                                      valid,
                                      phase,
                                      active_interpolation_factor_);
        append_frame_column(column);
    }

    for (std::uint8_t ch = 0; ch < 2u; ++ch) {
        interpolation_previous_raw_[ch] = raw[ch];
        interpolation_previous_valid_[ch] = valid[ch];
    }
    interpolation_has_previous_ = true;
}

// Takes one raw CH1/CH2 pair plus validity flags, folds valid samples into the
// rolling pre-trigger bucket, and commits full buckets.
void AcquisitionEngine::track_pretrigger_pair(const std::uint16_t raw[2],
                                              const bool valid[2])
{
    for (std::uint8_t ch = 0; ch < 2u; ++ch) {
        if (!valid[ch]) {
            continue;
        }
        if (!bucket_valid_[ch]) {
            bucket_min_[ch] = raw[ch];
            bucket_max_[ch] = raw[ch];
            bucket_valid_[ch] = true;
        } else {
            if (raw[ch] < bucket_min_[ch]) {
                bucket_min_[ch] = raw[ch];
            }
            if (raw[ch] > bucket_max_[ch]) {
                bucket_max_[ch] = raw[ch];
            }
        }
    }

    ++bucket_count_;
    if (bucket_count_ >= active_decimation_) {
        commit_pretrigger_bucket();
    }
}

// Takes one raw CH1/CH2 pair while waiting for trigger, emits rolling
// pre-trigger interpolation columns. Excluding the endpoint lets the trigger
// sample itself become the first captured column.
void AcquisitionEngine::track_interpolated_pretrigger_pair(
    const std::uint16_t raw[2],
    const bool valid[2],
    bool include_endpoint)
{
    if (!interpolation_has_previous_) {
        if (include_endpoint) {
            append_pretrigger_column(sample_column(raw, valid));
        }
        for (std::uint8_t ch = 0; ch < 2u; ++ch) {
            interpolation_previous_raw_[ch] = raw[ch];
            interpolation_previous_valid_[ch] = valid[ch];
        }
        interpolation_has_previous_ = true;
        return;
    }

    const std::uint16_t last_phase =
        include_endpoint ? active_interpolation_factor_
                         : static_cast<std::uint16_t>(active_interpolation_factor_ - 1u);
    for (std::uint16_t phase = 1u; phase <= last_phase; ++phase) {
        const ColumnSampleBucket column =
            phase == active_interpolation_factor_
                ? sample_column(raw, valid)
                : interpolated_column(interpolation_previous_raw_,
                                      interpolation_previous_valid_,
                                      raw,
                                      valid,
                                      phase,
                                      active_interpolation_factor_);
        append_pretrigger_column(column);
    }

    for (std::uint8_t ch = 0; ch < 2u; ++ch) {
        interpolation_previous_raw_[ch] = raw[ch];
        interpolation_previous_valid_[ch] = valid[ch];
    }
    interpolation_has_previous_ = true;
}

// Takes no inputs and returns true when the current endpoint can provide enough
// fractional columns before the endpoint itself.
bool AcquisitionEngine::interpolated_pretrigger_ready_for_endpoint() const
{
    const std::uint16_t endpoint_leadin =
        interpolation_has_previous_ && active_interpolation_factor_ > 1u
            ? static_cast<std::uint16_t>(active_interpolation_factor_ - 1u)
            : 0u;
    return static_cast<std::uint32_t>(pretrigger_count_) + endpoint_leadin >=
           config::kDefaultTriggerColumn;
}

// Takes interleaved ADC words, strips FIFO metadata, and folds complete pairs
// into the active capture until the frame is full.
void AcquisitionEngine::capture_words(const std::uint16_t *samples,
                                      std::uint32_t word_count)
{
    if (samples == nullptr || word_count < 2u || frame_ready_) {
        return;
    }

    for (std::uint32_t i = 0; i + 1u < word_count; i += 2u) {
        const std::uint16_t words[2] = {samples[i], samples[i + 1u]};
        const bool valid[2] = {adc_word_valid(words[0]), adc_word_valid(words[1])};
        const std::uint16_t raw[2] = {adc_word_value(words[0]), adc_word_value(words[1])};
        if (interpolation_active()) {
            capture_interpolated_pair(raw, valid);
        } else {
            capture_pair(raw, valid);
        }
        if (frame_ready_) {
            break;
        }
    }
}

// Takes one display column bucket, appends it to the frame, and completes the
// frame when the display width is filled.
void AcquisitionEngine::append_frame_column(const ColumnSampleBucket &bucket)
{
    if (column_index_ >= config::kDisplayWidth) {
        frame_.complete = true;
        frame_ready_ = true;
        mode_ = Mode::WaitingForTrigger;
        return;
    }

    frame_.columns[column_index_] = bucket;
    ++column_index_;

    if (column_index_ >= config::kDisplayWidth) {
        frame_.complete = true;
        frame_ready_ = true;
        ++frame_.sequence;
        mode_ = Mode::WaitingForTrigger;
    }
}

// Takes one display column bucket and appends it to the rolling pre-trigger
// ring.
void AcquisitionEngine::append_pretrigger_column(const ColumnSampleBucket &bucket)
{
    if (config::kDefaultTriggerColumn == 0u) {
        return;
    }

    pretrigger_columns_[pretrigger_write_index_] = bucket;

    ++pretrigger_write_index_;
    if (pretrigger_write_index_ >= config::kDefaultTriggerColumn) {
        pretrigger_write_index_ = 0;
    }
    if (pretrigger_count_ < config::kDefaultTriggerColumn) {
        ++pretrigger_count_;
    }
}

// Takes no inputs, copies the current bucket into the next frame column, marks
// completion when the frame is full.
void AcquisitionEngine::commit_bucket()
{
    ColumnSampleBucket column = {};
    for (std::uint8_t ch = 0; ch < 2u; ++ch) {
        column.valid[ch] = bucket_valid_[ch];
        column.min_count[ch] = bucket_min_[ch];
        column.max_count[ch] = bucket_max_[ch];
    }

    append_frame_column(column);
    clear_bucket();
}

// Takes no inputs, copies the current bucket into the rolling pre-trigger ring,
// marks the ring populated up to its capacity.
void AcquisitionEngine::commit_pretrigger_bucket()
{
    ColumnSampleBucket column = {};
    for (std::uint8_t ch = 0; ch < 2u; ++ch) {
        column.valid[ch] = bucket_valid_[ch];
        column.min_count[ch] = bucket_min_[ch];
        column.max_count[ch] = bucket_max_[ch];
    }

    append_pretrigger_column(column);
    clear_bucket();
}

// Takes no inputs, copies the oldest-to-newest pre-trigger columns into the
// frame before a streaming capture starts.
void AcquisitionEngine::copy_pretrigger_columns_to_frame()
{
    const std::uint16_t missing_columns =
        static_cast<std::uint16_t>(config::kDefaultTriggerColumn - pretrigger_count_);
    for (std::uint16_t x = 0; x < missing_columns; ++x) {
        frame_.columns[x] = {};
    }

    for (std::uint16_t i = 0; i < pretrigger_count_; ++i) {
        const std::uint16_t ring_index =
            static_cast<std::uint16_t>((pretrigger_write_index_ +
                                        config::kDefaultTriggerColumn -
                                        pretrigger_count_ + i) %
                                       config::kDefaultTriggerColumn);
        frame_.columns[missing_columns + i] = pretrigger_columns_[ring_index];
    }
}

// Takes no inputs, resets min/max/valid state for the active decimation bucket.
void AcquisitionEngine::clear_bucket()
{
    for (std::uint8_t ch = 0; ch < 2u; ++ch) {
        bucket_min_[ch] = config::kAdcMaxCount;
        bucket_max_[ch] = 0;
        bucket_valid_[ch] = false;
    }
    bucket_count_ = 0;
}

// Takes no inputs, clears the previous raw pair used by interpolation.
void AcquisitionEngine::clear_interpolation_state()
{
    for (std::uint8_t ch = 0; ch < 2u; ++ch) {
        interpolation_previous_raw_[ch] = 0;
        interpolation_previous_valid_[ch] = false;
    }
    interpolation_has_previous_ = false;
}

// Takes no inputs, clears rolling pre-trigger display columns and indices.
void AcquisitionEngine::clear_pretrigger_columns()
{
    for (std::uint16_t x = 0; x < config::kDefaultTriggerColumn; ++x) {
        pretrigger_columns_[x] = {};
    }
    pretrigger_write_index_ = 0;
    pretrigger_count_ = 0;
}

// Takes no inputs, clears every display column and frame status flag.
void AcquisitionEngine::clear_frame()
{
    for (std::uint16_t x = 0; x < config::kDisplayWidth; ++x) {
        for (std::uint8_t ch = 0; ch < 2u; ++ch) {
            frame_.columns[x].min_count[ch] = 0;
            frame_.columns[x].max_count[ch] = 0;
            frame_.columns[x].valid[ch] = false;
        }
    }
    frame_.triggered = false;
    frame_.complete = false;
    frame_.trigger_event = {};
}

} // namespace picoscope
