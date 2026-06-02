// Purpose: Declares shared trigger-edge helpers used by streaming and
// history-backed acquisition paths.
// Interface: Pass previous/current raw ADC counts plus TriggerSettings for a
// raw crossing result, or pass persistent arm state plus current sample for
// Schmitt-qualified trigger detection.
// Constraints: Samples are raw 12-bit ADC counts; caller owns previous-sample
// validity/history state.
// Ownership: This header owns no state.

#pragma once

#include <cstdint>

#include "scope_control.hpp"
#include "scope_types.hpp"

namespace picoscope {

// Takes consecutive raw trigger-channel samples plus trigger settings, compares
// them against the configured edge/threshold, and returns true on a crossing.
constexpr bool trigger_crossed(std::uint16_t previous,
                               std::uint16_t current,
                               const TriggerSettings &trigger)
{
    if (trigger.edge == TriggerEdge::Rising) {
        return previous < trigger.level_count && current >= trigger.level_count;
    }
    return previous > trigger.level_count && current <= trigger.level_count;
}

// Takes trigger settings and returns the lower side of the hysteresis band,
// saturated at the ADC's lower limit.
constexpr std::uint16_t trigger_lower_threshold(const TriggerSettings &trigger)
{
    constexpr std::uint16_t kHalfBand = config::kTriggerHysteresisCounts / 2u;
    return trigger.level_count > kHalfBand
               ? static_cast<std::uint16_t>(trigger.level_count - kHalfBand)
               : 0u;
}

// Takes trigger settings and returns the upper side of the hysteresis band,
// saturated at the ADC's upper limit.
constexpr std::uint16_t trigger_upper_threshold(const TriggerSettings &trigger)
{
    constexpr std::uint16_t kHalfBand = config::kTriggerHysteresisCounts / 2u;
    return trigger.level_count <
                   static_cast<std::uint16_t>(config::kAdcMaxCount - kHalfBand)
               ? static_cast<std::uint16_t>(trigger.level_count + kHalfBand)
               : config::kAdcMaxCount;
}

// Takes consecutive raw trigger-channel samples plus trigger settings and
// returns true when the signal crosses the opposite edge's hysteresis band.
constexpr bool trigger_opposite_edge_crossed(std::uint16_t previous,
                                             std::uint16_t current,
                                             const TriggerSettings &trigger)
{
    const std::uint16_t lower = trigger_lower_threshold(trigger);
    const std::uint16_t upper = trigger_upper_threshold(trigger);
    if (trigger.edge == TriggerEdge::Rising) {
        return previous >= upper && current <= lower;
    }
    return previous <= lower && current >= upper;
}

// Takes scope settings and returns true when the selected timebase should use
// the lightweight trigger qualifier for high-frequency response.
inline bool trigger_uses_fast_filter(const ScopeSettings &settings)
{
    const TimebaseRatio nominal = timebase_nominal_ratio(settings);
    return nominal.numerator <= nominal.denominator * 2u;
}

// Takes scope settings and returns the required number of arming-side samples
// before a Schmitt-qualified crossing can fire.
inline std::uint8_t trigger_arm_dwell_samples(const ScopeSettings &settings)
{
    return trigger_uses_fast_filter(settings)
               ? config::kFastTriggerArmDwellSamples
               : config::kTriggerArmDwellSamples;
}

// Takes persistent arm state plus one valid trigger-channel sample, requires a
// short dwell on the arming side, and returns true only on a qualified crossing.
inline bool schmitt_trigger_crossed(bool &armed,
                                    std::uint8_t &arm_sample_count,
                                    std::uint16_t current,
                                    const TriggerSettings &trigger,
                                    std::uint8_t arm_dwell_samples)
{
    const std::uint16_t lower = trigger_lower_threshold(trigger);
    const std::uint16_t upper = trigger_upper_threshold(trigger);

    if (trigger.edge == TriggerEdge::Rising) {
        if (!armed) {
            if (current <= lower) {
                if (arm_sample_count < arm_dwell_samples) {
                    ++arm_sample_count;
                }
                if (arm_sample_count >= arm_dwell_samples) {
                    armed = true;
                }
            } else {
                arm_sample_count = 0;
            }
            return false;
        }
        if (current >= upper) {
            armed = false;
            arm_sample_count = 0;
            return true;
        }
        return false;
    }

    if (!armed) {
        if (current >= upper) {
            if (arm_sample_count < arm_dwell_samples) {
                ++arm_sample_count;
            }
            if (arm_sample_count >= arm_dwell_samples) {
                armed = true;
            }
        } else {
            arm_sample_count = 0;
        }
        return false;
    }
    if (current <= lower) {
        armed = false;
        arm_sample_count = 0;
        return true;
    }
    return false;
}

// Takes persistent arm state plus one valid trigger-channel sample and applies
// the default trigger dwell.
inline bool schmitt_trigger_crossed(bool &armed,
                                    std::uint8_t &arm_sample_count,
                                    std::uint16_t current,
                                    const TriggerSettings &trigger)
{
    return schmitt_trigger_crossed(armed,
                                   arm_sample_count,
                                   current,
                                   trigger,
                                   config::kTriggerArmDwellSamples);
}

// Takes the raw pair count for one frame plus the active sample-pair rate and
// returns the auto-trigger timeout in sample pairs, preserving at least one
// frame period plus a minimum wait.
constexpr std::uint32_t auto_trigger_timeout_pairs(
    std::uint32_t frame_pair_count,
    std::uint32_t sample_pairs_per_second = config::kAdcSamplesPerSecondPerChannel)
{
    constexpr std::uint64_t kMicrosPerSecond = 1000000ull;
    const std::uint32_t frame_period_pairs =
        frame_pair_count * config::kAutoTriggerFramePeriods;
    const std::uint32_t minimum_pairs = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(sample_pairs_per_second) *
             config::kAutoTriggerMinimumWaitUs +
         kMicrosPerSecond - 1u) /
        kMicrosPerSecond);
    return frame_period_pairs > minimum_pairs ? frame_period_pairs : minimum_pairs;
}

// Takes the active display-column decimation and returns the number of raw
// pairs to suppress triggers after seeing the opposite edge.
constexpr std::uint32_t trigger_opposite_edge_holdoff_pairs(std::uint16_t decimation)
{
    const std::uint32_t column_pairs =
        static_cast<std::uint32_t>(config::kTriggerOppositeEdgeHoldoffColumns) *
        decimation;
    return column_pairs > config::kTriggerOppositeEdgeHoldoffMinimumPairs
               ? column_pairs
               : config::kTriggerOppositeEdgeHoldoffMinimumPairs;
}

// Takes scope settings and returns the opposite-edge trigger suppression period
// for the selected timebase.
inline std::uint32_t trigger_opposite_edge_holdoff_pairs(
    const ScopeSettings &settings)
{
    if (trigger_uses_fast_filter(settings)) {
        return config::kFastTriggerOppositeEdgeHoldoffPairs;
    }
    return trigger_opposite_edge_holdoff_pairs(timebase_decimation(settings));
}

} // namespace picoscope
