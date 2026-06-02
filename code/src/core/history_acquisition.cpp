// Purpose: Implements history-backed trigger scanning, pre-trigger capture, and
// auto-trigger lookback rendering from ADC history snapshots.
// Interface: HistoryAcquisition consumes sampler snapshots and fills an
// AcquisitionEngine frame when a trigger or lookback frame is available.
// Constraints: Uses fixed sampler/acquisition storage only and performs no
// dynamic allocation.
// Ownership: This file mutates only HistoryAcquisition state and the supplied
// AcquisitionEngine.

#include "history_acquisition.hpp"

#include "acquisition_engine.hpp"
#include "scope_control.hpp"
#include "trigger_logic.hpp"

namespace picoscope {

// Takes an optional next pair sequence, resets trigger scan state, and returns
// nothing.
void HistoryAcquisition::reset(std::uint64_t next_pair_sequence)
{
    state_ = {};
    state_.initialized = true;
    state_.next_scan_pair_sequence = next_pair_sequence;
}

// Takes current settings and returns true when one full raw frame fits in the
// stable ADC history ring.
bool HistoryAcquisition::supported(const ScopeSettings &settings) const
{
    return frame_pair_count(settings) <= history_capacity_pairs();
}

// Takes current settings, computes raw CH1/CH2 pairs per display frame, and
// returns that count.
std::uint32_t HistoryAcquisition::frame_pair_count(const ScopeSettings &settings) const
{
    return timebase_frame_pair_count(settings);
}

// Takes current settings, computes the configured pre-trigger raw pair count,
// and returns that count.
std::uint32_t HistoryAcquisition::pretrigger_pair_count(
    const ScopeSettings &settings) const
{
    return timebase_pretrigger_pair_count(settings);
}

// Takes current settings, computes the configured post-trigger raw pair count,
// and returns that count.
std::uint32_t HistoryAcquisition::posttrigger_pair_count(
    const ScopeSettings &settings) const
{
    return timebase_posttrigger_pair_count(settings);
}

// Takes no inputs and returns the stable history capacity after guard pairs.
std::uint32_t HistoryAcquisition::history_capacity_pairs() const
{
    return config::kAdcHistoryRingPairs - config::kAdcHistoryGuardPairs;
}

// Takes current settings, computes the auto-trigger wait in raw sample pairs,
// and returns that count.
std::uint32_t HistoryAcquisition::auto_trigger_pair_count(
    const ScopeSettings &settings) const
{
    return auto_trigger_timeout_pairs(frame_pair_count(settings),
                                      timebase_sample_pairs_per_second(settings));
}

// Takes a history window description, captures it into the acquisition frame,
// and returns true when a complete frame is ready to render.
bool HistoryAcquisition::capture_history_window(const AdcHistorySnapshot &snapshot,
                                                std::uint64_t start_pair_sequence,
                                                std::uint32_t pair_count,
                                                AcquisitionEngine &acquisition,
                                                const ScopeSettings &settings,
                                                const TriggerEvent &trigger_event)
{
    AdcHistoryRange range = {};
    if (!make_history_range(snapshot, start_pair_sequence, pair_count, range)) {
        return false;
    }
    acquisition.capture_history_range(range, settings, trigger_event);
    return acquisition.frame_ready();
}

// Takes the latest history snapshot, builds a frame ending at the newest stable
// pair, and returns true when a lookback frame is ready.
bool HistoryAcquisition::capture_lookback_frame(const AdcHistorySnapshot &snapshot,
                                                AcquisitionEngine &acquisition,
                                                const ScopeSettings &settings)
{
    const std::uint32_t pairs = frame_pair_count(settings);
    if (!snapshot.has_complete_pair ||
        snapshot.latest_complete_pair_sequence + 1u < pairs) {
        return false;
    }

    const std::uint64_t start_pair =
        snapshot.latest_complete_pair_sequence + 1u - pairs;
    return capture_history_window(snapshot,
                                  start_pair,
                                  pairs,
                                  acquisition,
                                  settings,
                                  make_origin_event(FrameOrigin::Auto, settings.trigger));
}

// Takes a pending trigger and snapshot, waits for post-trigger samples, captures
// the configured pre/post window when available, and returns true on a frame.
bool HistoryAcquisition::capture_trigger_frame_if_ready(
    const AdcHistorySnapshot &snapshot,
    AcquisitionEngine &acquisition,
    const ScopeSettings &settings)
{
    const std::uint32_t pretrigger_pairs = pretrigger_pair_count(settings);
    const std::uint32_t posttrigger_pairs = posttrigger_pair_count(settings);
    const std::uint32_t pairs = frame_pair_count(settings);
    if (state_.trigger_pair_sequence < pretrigger_pairs) {
        reset(snapshot.latest_complete_pair_sequence + 1u);
        return false;
    }

    const std::uint64_t start_pair = state_.trigger_pair_sequence - pretrigger_pairs;
    const std::uint64_t required_latest =
        state_.trigger_pair_sequence + posttrigger_pairs - 1u;
    if (snapshot.oldest_available_pair_sequence > start_pair) {
        reset(snapshot.latest_complete_pair_sequence + 1u);
        return false;
    }
    if (snapshot.latest_complete_pair_sequence < required_latest) {
        return false;
    }

    const bool captured =
        capture_history_window(snapshot,
                               start_pair,
                               pairs,
                               acquisition,
                               settings,
                               state_.pending_trigger_event);
    if (captured) {
        reset(snapshot.latest_complete_pair_sequence + 1u);
    }
    return captured;
}

// Takes a history snapshot, updates trigger/auto-trigger state, captures a
// frame when possible, and returns true when the renderer should run.
bool HistoryAcquisition::process(const AdcHistorySnapshot &snapshot,
                                 AcquisitionEngine &acquisition,
                                 const ScopeSettings &settings)
{
    if (!snapshot.has_complete_pair) {
        return false;
    }

    if (!state_.initialized ||
        state_.next_scan_pair_sequence < snapshot.oldest_available_pair_sequence) {
        reset(snapshot.oldest_available_pair_sequence);
    }

    if (state_.waiting_for_post_trigger) {
        return capture_trigger_frame_if_ready(snapshot, acquisition, settings);
    }

    const std::uint8_t trigger_channel = channel_index(settings.trigger.source);
    std::uint64_t sequence = state_.next_scan_pair_sequence;
    if (sequence > snapshot.latest_complete_pair_sequence + 1u) {
        sequence = snapshot.latest_complete_pair_sequence + 1u;
    }

    const std::uint32_t pretrigger_pairs = pretrigger_pair_count(settings);
    const std::uint8_t trigger_arm_samples = trigger_arm_dwell_samples(settings);
    const std::uint32_t trigger_holdoff_pairs =
        trigger_opposite_edge_holdoff_pairs(settings);
    while (sequence <= snapshot.latest_complete_pair_sequence) {
        std::uint16_t words[2] = {};
        if (!read_history_pair(snapshot, sequence, words)) {
            break;
        }

        const bool valid = adc_word_valid(words[trigger_channel]);
        const std::uint16_t raw = adc_word_value(words[trigger_channel]);
        const bool previous_valid = state_.previous_trigger_valid;
        const std::uint16_t previous_count = state_.previous_trigger_count;
        bool holdoff_active = state_.trigger_opposite_holdoff_pairs > 0u;
        bool crossed = false;
        if (valid) {
            if (previous_valid &&
                trigger_opposite_edge_crossed(previous_count, raw, settings.trigger)) {
                state_.trigger_opposite_holdoff_pairs =
                    trigger_holdoff_pairs;
                holdoff_active = true;
            }

            const bool raw_crossed =
                schmitt_trigger_crossed(state_.trigger_armed,
                                        state_.trigger_arm_sample_count,
                                        raw,
                                        settings.trigger,
                                        trigger_arm_samples);
            crossed = sequence >= snapshot.oldest_available_pair_sequence + pretrigger_pairs &&
                      !holdoff_active &&
                      raw_crossed;
        }

        if (crossed) {
            state_.pending_trigger_event = make_trigger_event(settings.trigger,
                                                              previous_valid,
                                                              previous_count,
                                                              raw);
            state_.waiting_for_post_trigger = true;
            state_.trigger_pair_sequence = sequence;
            state_.next_scan_pair_sequence = sequence + 1u;
            state_.auto_wait_pairs = 0;
            return capture_trigger_frame_if_ready(snapshot, acquisition, settings);
        }

        if (valid) {
            if (state_.trigger_opposite_holdoff_pairs > 0u) {
                --state_.trigger_opposite_holdoff_pairs;
            }
            state_.previous_trigger_valid = true;
            state_.previous_trigger_count = raw;
        }
        ++state_.auto_wait_pairs;
        state_.next_scan_pair_sequence = sequence + 1u;
        ++sequence;
    }

    if (state_.auto_wait_pairs >= auto_trigger_pair_count(settings) &&
        capture_lookback_frame(snapshot, acquisition, settings)) {
        reset(snapshot.latest_complete_pair_sequence + 1u);
        return true;
    }
    return false;
}

} // namespace picoscope
