// Purpose: Declares control-layer helpers that mutate scope settings in
// response to encoder/switch input and expose labels for the renderer.
// Interface: Functions operate on ScopeSettings/ChannelSettings by reference or
// const reference; label functions return static strings owned by config tables.
// Constraints: Return values distinguish redraw-only updates from capture resets
// so the main loop can stop/restart sampling only when necessary.
// Ownership: This layer owns no hardware resources or frame buffers.

#pragma once

#include "scope_types.hpp"

namespace picoscope {

// Describes which work is required after applying one batch of input events.
struct ScopeUpdate {
    bool redraw = false;
    bool reset_capture = false;
};

// Takes mutable settings and a batch of input events, applies all requested
// control changes, and returns the required redraw/capture work.
ScopeUpdate apply_input_events(ScopeSettings &settings, const InputEvents &events);

// Takes a channel enum, resolves its short UI label, and returns a static
// null-terminated string.
const char *channel_label(Channel channel);

// Takes a trigger edge enum, resolves its UI label, and returns a static
// null-terminated string.
const char *trigger_edge_label(TriggerEdge edge);

// Takes channel settings, looks up the selected volts-per-division label, and
// returns a static null-terminated string.
const char *volts_scale_label(const ChannelSettings &channel);

// Takes channel settings, looks up the selected scale, and returns volts per
// vertical division.
float volts_per_div(const ChannelSettings &channel);

// Takes scope settings, looks up the selected timebase label, and returns a
// static null-terminated string.
const char *timebase_label(const ScopeSettings &settings);

// Nominal horizontal sample-pair ratio at the maximum ADC rate.
struct TimebaseRatio {
    std::uint16_t numerator = 1;
    std::uint16_t denominator = 1;
};

// Takes scope settings, looks up the selected nominal timebase, and returns the
// sample-pair ratio per display column at the maximum ADC rate.
TimebaseRatio timebase_nominal_ratio(const ScopeSettings &settings);

// Takes scope settings, looks up the selected timebase, and returns sample
// pairs to aggregate per display column at the adjusted ADC rate.
std::uint16_t timebase_decimation(const ScopeSettings &settings);

// Takes scope settings and returns true when display columns are interpolated
// between raw sample pairs.
bool timebase_uses_interpolation(const ScopeSettings &settings);

// Takes scope settings and returns the number of display columns synthesized
// per raw sample-pair interval. Non-interpolated timebases return 1.
std::uint16_t timebase_interpolation_factor(const ScopeSettings &settings);

// Takes scope settings and returns raw CH1/CH2 pairs needed for one frame.
std::uint32_t timebase_frame_pair_count(const ScopeSettings &settings);

// Takes scope settings and returns raw CH1/CH2 pairs before the trigger column.
std::uint32_t timebase_pretrigger_pair_count(const ScopeSettings &settings);

// Takes scope settings and returns raw CH1/CH2 pairs from trigger through frame end.
std::uint32_t timebase_posttrigger_pair_count(const ScopeSettings &settings);

// Takes scope settings and returns the effective CH1/CH2 sample-pair rate after
// ADC-rate adjustment.
std::uint32_t timebase_sample_pairs_per_second(const ScopeSettings &settings);

// Takes scope settings and returns the clkdiv value to pass to adc_set_clkdiv().
float timebase_adc_clkdiv(const ScopeSettings &settings);

} // namespace picoscope
