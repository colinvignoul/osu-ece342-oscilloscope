// Purpose: Declares the shared data model for channels, triggers, input
// events, calibration, and rendered acquisition frames.
// Interface: Provides simple value types passed by reference between control,
// acquisition, rendering, and hardware layers.
// Constraints: Arrays are fixed for two channels and one 320-column frame so
// the firmware can run without dynamic allocation.
// Ownership: Struct instances are owned by their creators; this header only
// defines layout and small conversion helpers.

#pragma once

#include <cstdint>

#include "config.hpp"

namespace picoscope {

// Logical input channels map directly to channel arrays throughout the firmware.
enum class Channel : std::uint8_t {
    Ch1 = 0,
    Ch2 = 1,
};

// Trigger edge selection controls how consecutive ADC samples are compared.
enum class TriggerEdge : std::uint8_t {
    Rising = 0,
    Falling = 1,
};

// Axis edit mode selects whether the vertical/horizontal encoders shift
// position or change scale.
enum class AxisEditMode : std::uint8_t {
    Shift = 0,
    Scale = 1,
};

// Frame origin tells diagnostics whether the capture came from a real trigger
// crossing, an auto-refresh, or no completed acquisition yet.
enum class FrameOrigin : std::uint8_t {
    None = 0,
    Trigger = 1,
    Auto = 2,
};

// Direction of the raw trigger-channel sample step that produced a trigger.
enum class TriggerSampleDirection : std::uint8_t {
    Unknown = 0,
    Rising = 1,
    Falling = 2,
    Flat = 3,
};

// ADC calibration converts raw counts into measured-input volts relative to a
// channel zero count. Defaults account for the analog front-end gain and bias.
struct Calibration {
    std::uint16_t zero_count = config::kAdcBiasCount;
    float volts_per_count = config::kDefaultVoltsPerCount;
};

// Per-channel display and calibration settings owned by ScopeSettings.
struct ChannelSettings {
    bool enabled = true;
    config::Rgb565 color = config::kColorCh1;
    std::uint8_t volts_scale_index = config::kDefaultVoltsScaleIndex;
    std::int16_t horizontal_offset_columns = 0;
    float vertical_offset_divs = 0.0f;
    Calibration calibration = {};
};

// Trigger contract: source channel, edge polarity, and threshold in raw counts.
struct TriggerSettings {
    Channel source = Channel::Ch1;
    TriggerEdge edge = TriggerEdge::Rising;
    std::uint16_t level_count = config::kAdcBiasCount;
};

// Diagnostic record for the trigger event that started a rendered frame.
struct TriggerEvent {
    FrameOrigin origin = FrameOrigin::None;
    TriggerEdge configured_edge = TriggerEdge::Rising;
    TriggerSampleDirection sample_direction = TriggerSampleDirection::Unknown;
    std::uint16_t previous_count = 0;
    std::uint16_t current_count = 0;
    bool has_previous_count = false;
};

constexpr TriggerSampleDirection trigger_sample_direction(std::uint16_t previous,
                                                          std::uint16_t current)
{
    if (current > previous) {
        return TriggerSampleDirection::Rising;
    }
    if (current < previous) {
        return TriggerSampleDirection::Falling;
    }
    return TriggerSampleDirection::Flat;
}

constexpr TriggerEvent make_trigger_event(const TriggerSettings &trigger,
                                          bool has_previous_count,
                                          std::uint16_t previous_count,
                                          std::uint16_t current_count)
{
    return TriggerEvent{
        FrameOrigin::Trigger,
        trigger.edge,
        has_previous_count ? trigger_sample_direction(previous_count, current_count)
                           : TriggerSampleDirection::Unknown,
        has_previous_count ? previous_count : static_cast<std::uint16_t>(0u),
        current_count,
        has_previous_count,
    };
}

constexpr TriggerEvent make_origin_event(FrameOrigin origin,
                                         const TriggerSettings &trigger)
{
    return TriggerEvent{
        origin,
        trigger.edge,
        TriggerSampleDirection::Unknown,
        0u,
        0u,
        false,
    };
}

// Complete mutable control state shared by input, acquisition, and rendering.
struct ScopeSettings {
    ChannelSettings channels[2] = {};
    TriggerSettings trigger = {};
    Channel active_channel = Channel::Ch1;
    AxisEditMode axis_edit_mode = AxisEditMode::Shift;
    std::uint8_t timebase_index = config::kDefaultTimebaseIndex;
    bool running = true;
};

// Input snapshot returned by one EncoderManager poll.
struct InputEvents {
    std::int16_t trigger_delta = 0;
    std::int16_t vertical_delta = 0;
    std::int16_t horizontal_delta = 0;
    bool channel_switch_active = false;
    bool shift_scale_switch_active = false;
};

// One display column keeps the min/max seen during its decimation window rather
// than a single point, preserving short pulses in the rendered trace.
struct ColumnSampleBucket {
    std::uint16_t min_count[2] = {0, 0};
    std::uint16_t max_count[2] = {0, 0};
    bool valid[2] = {false, false};
};

// A completed or in-progress frame is owned by AcquisitionEngine and borrowed
// read-only by StripRenderer.
struct ScopeFrame {
    ColumnSampleBucket columns[config::kDisplayWidth] = {};
    bool triggered = false;
    bool complete = false;
    TriggerEvent trigger_event = {};
    // Incremented only when a full frame completes.
    std::uint32_t sequence = 0;
};

// Takes a Channel enum, maps it to the zero-based channel array index, and
// returns that index.
constexpr std::uint8_t channel_index(Channel channel)
{
    return channel == Channel::Ch1 ? 0u : 1u;
}

// Takes a Channel enum, maps it to the opposite logical channel, and returns
// the other Channel value.
constexpr Channel other_channel(Channel channel)
{
    return channel == Channel::Ch1 ? Channel::Ch2 : Channel::Ch1;
}

// Takes a raw ADC count plus calibration, converts it to signed measured-input
// volts relative to calibration.zero_count, and returns that voltage.
float raw_to_volts(std::uint16_t raw, const Calibration &calibration);

// Takes a raw ADC count and channel display settings, maps it to absolute
// screen Y coordinates, and returns the nearest integer pixel row.
int raw_to_screen_y(std::uint16_t raw, const ChannelSettings &settings);

// Takes no inputs, constructs the firmware's startup channel/trigger/timebase
// configuration, and returns it by value.
ScopeSettings default_scope_settings();

} // namespace picoscope
