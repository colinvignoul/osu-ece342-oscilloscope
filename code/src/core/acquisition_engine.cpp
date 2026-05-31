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

} // namespace

// Takes no inputs, clears frame and bucket state, and returns a ready engine.
AcquisitionEngine::AcquisitionEngine()
{
    clear_frame();
    clear_bucket();
    clear_pretrigger_columns();
}

// Takes current scope settings and a clear-frame flag, resets trigger/capture
// state for the selected timebase, and returns nothing.
void AcquisitionEngine::reset(const ScopeSettings &settings, bool clear_frame_contents)
{
    active_decimation_ = timebase_decimation(settings);
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
    clear_pretrigger_columns();
    if (clear_frame_contents) {
        clear_frame();
    }
}

// Takes borrowed interleaved sample words, their word count, and current
// settings; detects triggers and fills frame columns, returning nothing.
void AcquisitionEngine::process_interleaved(const std::uint16_t *samples,
                                            std::uint32_t word_count,
                                            const ScopeSettings &settings)
{
    if (samples == nullptr || word_count < 2u || !settings.running || frame_ready_) {
        return;
    }

    const std::uint16_t requested_decimation = timebase_decimation(settings);
    if (requested_decimation != active_decimation_) {
        reset(settings, false);
    }

    const std::uint8_t trigger_channel = channel_index(settings.trigger.source);
    const std::uint32_t frame_pairs =
        static_cast<std::uint32_t>(active_decimation_) * config::kDisplayWidth;
    const std::uint32_t auto_trigger_pairs =
        auto_trigger_timeout_pairs(frame_pairs,
                                   timebase_sample_pairs_per_second(settings));

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
                        trigger_opposite_edge_holdoff_pairs(active_decimation_);
                    holdoff_active = true;
                }
                const bool raw_crossed = schmitt_trigger_crossed(trigger_armed_,
                                                                 trigger_arm_sample_count_,
                                                                 raw[trigger_channel],
                                                                 settings.trigger);
                crossed = pretrigger_count_ >= config::kDefaultTriggerColumn &&
                          !holdoff_active &&
                          raw_crossed;
            }

            ++trigger_wait_pairs_;
            if (crossed || trigger_wait_pairs_ >= auto_trigger_pairs) {
                const TriggerEvent trigger_event =
                    crossed ? make_trigger_event(settings.trigger,
                                                 previous_valid,
                                                 previous_count,
                                                 raw[trigger_channel])
                            : make_origin_event(FrameOrigin::Auto, settings.trigger);
                begin_capture(trigger_event, true);
                capture_pair(raw, valid);
            } else {
                track_pretrigger_pair(raw, valid);
            }
            if (valid[trigger_channel]) {
                if (trigger_opposite_holdoff_pairs_ > 0u) {
                    --trigger_opposite_holdoff_pairs_;
                }
                previous_trigger_valid_ = true;
                previous_trigger_count_ = raw[trigger_channel];
            }
        } else {
            capture_pair(raw, valid);
        }

        if (frame_ready_) {
            break;
        }
    }
}

// Takes interleaved words and settings, starts capture immediately, and returns
// with frame_ready() true only when enough words filled the display frame.
void AcquisitionEngine::capture_interleaved(const std::uint16_t *samples,
                                            std::uint32_t word_count,
                                            const ScopeSettings &settings,
                                            bool triggered)
{
    if (samples == nullptr || word_count < 2u || !settings.running) {
        return;
    }

    active_decimation_ = timebase_decimation(settings);
    begin_capture(capture_origin_event(triggered, settings.trigger), false);
    capture_words(samples, word_count);
}

// Takes a wrapped history range, captures each contiguous span in chronological
// order, and returns nothing.
void AcquisitionEngine::capture_history_range(const AdcHistoryRange &range,
                                              const ScopeSettings &settings,
                                              bool triggered)
{
    capture_history_range(range,
                          settings,
                          capture_origin_event(triggered, settings.trigger));
}

// Takes a wrapped history range plus trigger diagnostics, captures each
// contiguous span in chronological order, and returns nothing.
void AcquisitionEngine::capture_history_range(const AdcHistoryRange &range,
                                              const ScopeSettings &settings,
                                              const TriggerEvent &trigger_event)
{
    if (!settings.running || range.span_count == 0u) {
        return;
    }

    active_decimation_ = timebase_decimation(settings);
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
    active_decimation_ = timebase_decimation(settings);
    clear_bucket();
    clear_pretrigger_columns();
}

// Takes trigger diagnostics, clears output state, enters capture mode, and
// returns nothing.
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
}

// Takes one raw CH1/CH2 pair plus validity flags, folds valid samples into the
// min/max bucket, commits full buckets, and returns nothing.
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

// Takes one raw CH1/CH2 pair plus validity flags, folds valid samples into the
// rolling pre-trigger bucket, commits full buckets, and returns nothing.
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
        capture_pair(raw, valid);
        if (frame_ready_) {
            break;
        }
    }
}

// Takes no inputs, copies the current bucket into the next frame column, marks
// completion when the frame is full, and returns nothing.
void AcquisitionEngine::commit_bucket()
{
    if (column_index_ >= config::kDisplayWidth) {
        frame_.complete = true;
        frame_ready_ = true;
        mode_ = Mode::WaitingForTrigger;
        return;
    }

    ColumnSampleBucket &column = frame_.columns[column_index_];
    for (std::uint8_t ch = 0; ch < 2u; ++ch) {
        column.valid[ch] = bucket_valid_[ch];
        column.min_count[ch] = bucket_min_[ch];
        column.max_count[ch] = bucket_max_[ch];
    }

    ++column_index_;
    clear_bucket();

    if (column_index_ >= config::kDisplayWidth) {
        frame_.complete = true;
        frame_ready_ = true;
        ++frame_.sequence;
        mode_ = Mode::WaitingForTrigger;
    }
}

// Takes no inputs, copies the current bucket into the rolling pre-trigger ring,
// marks the ring populated up to its capacity, and returns nothing.
void AcquisitionEngine::commit_pretrigger_bucket()
{
    if (config::kDefaultTriggerColumn == 0u) {
        clear_bucket();
        return;
    }

    ColumnSampleBucket &column = pretrigger_columns_[pretrigger_write_index_];
    for (std::uint8_t ch = 0; ch < 2u; ++ch) {
        column.valid[ch] = bucket_valid_[ch];
        column.min_count[ch] = bucket_min_[ch];
        column.max_count[ch] = bucket_max_[ch];
    }

    ++pretrigger_write_index_;
    if (pretrigger_write_index_ >= config::kDefaultTriggerColumn) {
        pretrigger_write_index_ = 0;
    }
    if (pretrigger_count_ < config::kDefaultTriggerColumn) {
        ++pretrigger_count_;
    }
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

// Takes no inputs, resets min/max/valid state for the active decimation bucket,
// and returns nothing.
void AcquisitionEngine::clear_bucket()
{
    for (std::uint8_t ch = 0; ch < 2u; ++ch) {
        bucket_min_[ch] = config::kAdcMaxCount;
        bucket_max_[ch] = 0;
        bucket_valid_[ch] = false;
    }
    bucket_count_ = 0;
}

// Takes no inputs, clears rolling pre-trigger display columns and indices, and
// returns nothing.
void AcquisitionEngine::clear_pretrigger_columns()
{
    for (std::uint16_t x = 0; x < config::kDefaultTriggerColumn; ++x) {
        pretrigger_columns_[x] = {};
    }
    pretrigger_write_index_ = 0;
    pretrigger_count_ = 0;
}

// Takes no inputs, clears every display column and frame status flag, and
// returns nothing.
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
