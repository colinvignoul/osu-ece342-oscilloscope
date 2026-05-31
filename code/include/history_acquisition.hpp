// Purpose: Declares the history-backed acquisition policy for pre-trigger and
// lookback rendering from the ADC history ring.
// Interface: Feed sampler snapshots through process(); it updates trigger scan
// state and fills AcquisitionEngine frames when enough history is available.
// Constraints: Owns only lightweight scan state and performs no allocation.
// Ownership: HistoryAcquisition owns trigger-scan state; ADC storage remains
// sampler-owned and rendered frames remain AcquisitionEngine-owned.

#pragma once

#include <cstdint>

#include "adc_history.hpp"
#include "scope_types.hpp"

namespace picoscope {

class AcquisitionEngine;

class HistoryAcquisition {
public:
    // Takes no inputs, constructs a history acquisition state machine ready to
    // seed itself from the first snapshot.
    HistoryAcquisition() = default;

    // Takes an optional next sequence, clears scan state, and returns nothing.
    void reset(std::uint64_t next_pair_sequence = 0);

    // Takes current settings and returns true when one raw frame fits in the
    // stable ADC history ring.
    bool supported(const ScopeSettings &settings) const;

    // Takes a history snapshot plus acquisition engine/settings, scans for
    // trigger or auto-trigger lookback frames, and returns true when a frame is
    // ready to render.
    bool process(const AdcHistorySnapshot &snapshot,
                 AcquisitionEngine &acquisition,
                 const ScopeSettings &settings);

    // Takes current settings, computes raw CH1/CH2 pairs per display frame, and
    // returns that count.
    std::uint32_t frame_pair_count(const ScopeSettings &settings) const;

    // Takes current settings, computes pre-trigger raw pair count, and returns
    // that count.
    std::uint32_t pretrigger_pair_count(const ScopeSettings &settings) const;

    // Takes current settings, computes post-trigger raw pair count, and returns
    // that count.
    std::uint32_t posttrigger_pair_count(const ScopeSettings &settings) const;

private:
    struct State {
        bool initialized = false;
        bool waiting_for_post_trigger = false;
        bool trigger_armed = false;
        bool previous_trigger_valid = false;
        std::uint8_t trigger_arm_sample_count = 0;
        std::uint64_t next_scan_pair_sequence = 0;
        std::uint64_t trigger_pair_sequence = 0;
        std::uint32_t auto_wait_pairs = 0;
        std::uint32_t trigger_opposite_holdoff_pairs = 0;
        std::uint16_t previous_trigger_count = 0;
        TriggerEvent pending_trigger_event = {};
    };

    // Takes no inputs and returns the stable history capacity after guard pairs.
    std::uint32_t history_capacity_pairs() const;

    // Takes current settings, computes auto-trigger wait in raw pairs, and
    // returns that count.
    std::uint32_t auto_trigger_pair_count(const ScopeSettings &settings) const;

    // Takes a history window, captures it into acquisition, and returns true
    // when the frame is complete.
    bool capture_history_window(const AdcHistorySnapshot &snapshot,
                                std::uint64_t start_pair_sequence,
                                std::uint32_t pair_count,
                                AcquisitionEngine &acquisition,
                                const ScopeSettings &settings,
                                const TriggerEvent &trigger_event);

    // Takes the latest snapshot, builds a frame ending at newest stable pair,
    // and returns true when acquisition has a frame ready.
    bool capture_lookback_frame(const AdcHistorySnapshot &snapshot,
                                AcquisitionEngine &acquisition,
                                const ScopeSettings &settings);

    // Takes a pending trigger and snapshot, waits for post-trigger samples,
    // captures when ready, and returns true on a completed frame.
    bool capture_trigger_frame_if_ready(const AdcHistorySnapshot &snapshot,
                                        AcquisitionEngine &acquisition,
                                        const ScopeSettings &settings);

    State state_ = {};
};

} // namespace picoscope
