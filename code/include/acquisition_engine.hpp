// Purpose: Declares the acquisition engine that turns interleaved ADC DMA
// blocks or history ranges into display-ready min/max columns.
// Interface: Callers feed CH1/CH2 sample words with process_interleaved(), read
// the stable ScopeFrame by reference, or capture a history range immediately.
// Constraints: The engine expects CH1 then CH2 word ordering, skips ADC error
// words, performs trigger detection, and never allocates memory dynamically.
// Ownership: AcquisitionEngine owns its ScopeFrame and internal bucket state;
// input sample buffers remain owned by the sampler/caller.

#pragma once

#include <cstdint>

#include "adc_history.hpp"
#include "scope_control.hpp"
#include "scope_types.hpp"

namespace picoscope {

// Owns trigger state, decimation buckets, and one display-width ScopeFrame.
class AcquisitionEngine {
public:
    // Takes no inputs, initializes an empty frame and decimation bucket, and
    // returns a ready-to-use engine instance.
    AcquisitionEngine();

    // Takes current settings and a clear-frame flag, resets acquisition state,
    // and returns nothing.
    void reset(const ScopeSettings &settings, bool clear_frame = true);

    // Takes interleaved ADC words, their count, and current settings; consumes
    // complete channel pairs into the frame and returns nothing.
    void process_interleaved(const std::uint16_t *samples,
                             std::uint32_t word_count,
                             const ScopeSettings &settings);

    // Takes interleaved ADC words and settings, starts capture immediately
    // without waiting for a trigger, and returns nothing.
    void capture_interleaved(const std::uint16_t *samples,
                             std::uint32_t word_count,
                             const ScopeSettings &settings,
                             bool triggered);

    // Takes one wrapped history range, captures it immediately into the frame,
    // and returns nothing.
    void capture_history_range(const AdcHistoryRange &range,
                               const ScopeSettings &settings,
                               bool triggered);

    // Takes one wrapped history range plus trigger diagnostics, captures it
    // immediately into the frame, and returns nothing.
    void capture_history_range(const AdcHistoryRange &range,
                               const ScopeSettings &settings,
                               const TriggerEvent &trigger_event);

    // Takes no inputs and returns a const reference to the engine-owned frame.
    const ScopeFrame &frame() const { return frame_; }

    // Takes no inputs and returns true when a complete frame is ready to render.
    bool frame_ready() const { return frame_ready_; }

    // Takes current settings, releases the current ready frame, prepares the
    // next trigger wait, and returns nothing.
    void acknowledge_frame(const ScopeSettings &settings);

private:
    enum class Mode : std::uint8_t {
        WaitingForTrigger,
        Capturing,
    };

    // Takes trigger diagnostics, clears frame/bucket state, optionally copies
    // streaming pre-trigger columns, starts filling a new frame, and returns
    // nothing.
    void begin_capture(const TriggerEvent &trigger_event, bool copy_pretrigger);

    // Takes current settings, caches the active timebase policy, and returns
    // nothing.
    void configure_timebase(const ScopeSettings &settings);

    // Takes no inputs and returns true when the active timebase synthesizes
    // display columns between raw sample pairs.
    bool interpolation_active() const;

    // Takes one raw CH1/CH2 pair plus validity flags, folds it into the active
    // decimation bucket, and returns nothing.
    void capture_pair(const std::uint16_t raw[2], const bool valid[2]);

    // Takes one raw CH1/CH2 pair plus validity flags, interpolates display
    // columns for a fractional timebase, and returns nothing.
    void capture_interpolated_pair(const std::uint16_t raw[2],
                                   const bool valid[2]);

    // Takes one raw CH1/CH2 pair while waiting for a trigger, folds it into the
    // rolling pre-trigger column ring, and returns nothing.
    void track_pretrigger_pair(const std::uint16_t raw[2], const bool valid[2]);

    // Takes one raw CH1/CH2 pair while waiting for a trigger, interpolates
    // rolling pre-trigger columns, and optionally excludes the exact endpoint.
    void track_interpolated_pretrigger_pair(const std::uint16_t raw[2],
                                            const bool valid[2],
                                            bool include_endpoint);

    // Takes no inputs and returns true when enough fractional pre-trigger
    // columns can be synthesized through the current raw endpoint.
    bool interpolated_pretrigger_ready_for_endpoint() const;

    // Takes interleaved ADC words and consumes complete CH1/CH2 pairs into the
    // current capture, returning nothing.
    void capture_words(const std::uint16_t *samples, std::uint32_t word_count);

    // Takes one display column bucket, appends it to the output frame, and
    // marks the frame ready when full.
    void append_frame_column(const ColumnSampleBucket &bucket);

    // Takes one display column bucket and appends it to the rolling
    // pre-trigger ring.
    void append_pretrigger_column(const ColumnSampleBucket &bucket);

    // Takes no inputs, writes the current min/max bucket into the next display
    // column, and returns nothing.
    void commit_bucket();

    // Takes no inputs, writes the current min/max bucket into the rolling
    // pre-trigger column ring, and returns nothing.
    void commit_pretrigger_bucket();

    // Takes no inputs, copies the rolling pre-trigger ring into the frame so the
    // next captured sample lands at the configured trigger column.
    void copy_pretrigger_columns_to_frame();

    // Takes no inputs, resets the current min/max aggregation bucket, and
    // returns nothing.
    void clear_bucket();

    // Takes no inputs, clears the raw-pair interpolation endpoint.
    void clear_interpolation_state();

    // Takes no inputs, clears rolling pre-trigger display columns and indices.
    void clear_pretrigger_columns();

    // Takes no inputs, clears all frame columns and completion flags, and
    // returns nothing.
    void clear_frame();

    ScopeFrame frame_ = {};
    Mode mode_ = Mode::WaitingForTrigger;
    bool frame_ready_ = false;
    std::uint16_t active_decimation_ = 1;
    std::uint16_t active_interpolation_factor_ = 1;
    std::uint16_t bucket_count_ = 0;
    std::uint16_t column_index_ = 0;
    std::uint16_t bucket_min_[2] = {0, 0};
    std::uint16_t bucket_max_[2] = {0, 0};
    bool bucket_valid_[2] = {false, false};
    std::uint16_t interpolation_previous_raw_[2] = {0, 0};
    bool interpolation_previous_valid_[2] = {false, false};
    bool interpolation_has_previous_ = false;
    ColumnSampleBucket pretrigger_columns_[config::kDefaultTriggerColumn] = {};
    std::uint16_t pretrigger_write_index_ = 0;
    std::uint16_t pretrigger_count_ = 0;
    bool trigger_armed_ = false;
    std::uint8_t trigger_arm_sample_count_ = 0;
    bool previous_trigger_valid_ = false;
    std::uint16_t previous_trigger_count_ = 0;
    std::uint32_t trigger_wait_pairs_ = 0;
    std::uint32_t trigger_opposite_holdoff_pairs_ = 0;
};

} // namespace picoscope
