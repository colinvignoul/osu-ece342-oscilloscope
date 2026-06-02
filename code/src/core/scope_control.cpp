#include "scope_control.hpp"

#include <limits>

namespace picoscope {
namespace {

template <typename T>
T clamp_value(T value, T low, T high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

std::uint8_t clamp_table_index(std::int32_t index, std::size_t count)
{
    const std::int32_t last = static_cast<std::int32_t>(count - 1u);
    return static_cast<std::uint8_t>(clamp_value<std::int32_t>(index, 0, last));
}

void request_redraw(ScopeUpdate &update)
{
    update.redraw = true;
}

void request_capture_reset(ScopeUpdate &update)
{
    update.redraw = true;
    update.reset_capture = true;
}

Channel channel_from_switch(bool active)
{
    return active ? Channel::Ch2 : Channel::Ch1;
}

ControlAxis control_axis_from_switch(bool active)
{
    return active ? ControlAxis::Horizontal : ControlAxis::Vertical;
}

bool apply_horizontal_position_delta(ChannelSettings &channel, std::int16_t delta)
{
    if (delta == 0) {
        return false;
    }

    const std::int32_t requested =
        static_cast<std::int32_t>(channel.horizontal_offset_columns) +
        static_cast<std::int32_t>(delta) *
            config::kPositionEncoderPixelsPerTransition;
    const std::int16_t clamped = static_cast<std::int16_t>(
        clamp_value<std::int32_t>(requested,
                                  std::numeric_limits<std::int16_t>::min(),
                                  std::numeric_limits<std::int16_t>::max()));
    if (clamped == channel.horizontal_offset_columns) {
        return false;
    }
    channel.horizontal_offset_columns = clamped;
    return true;
}

bool apply_vertical_position_delta(ChannelSettings &channel, std::int16_t delta)
{
    if (delta == 0) {
        return false;
    }

    channel.vertical_offset_divs +=
        static_cast<float>(delta) * config::kVerticalOffsetDivsPerTransition;
    return true;
}

bool apply_volts_scale_delta(ChannelSettings &channel, std::int16_t delta)
{
    if (delta == 0) {
        return false;
    }

    const std::int32_t requested =
        static_cast<std::int32_t>(channel.volts_scale_index) + delta;
    const std::uint8_t clamped = clamp_table_index(requested, config::kVoltsScaleCount);
    if (clamped == channel.volts_scale_index) {
        return false;
    }
    channel.volts_scale_index = clamped;
    return true;
}

} // namespace

float raw_to_volts(std::uint16_t raw, const Calibration &calibration)
{
    const std::int32_t centered =
        static_cast<std::int32_t>(raw) - static_cast<std::int32_t>(calibration.zero_count);
    return static_cast<float>(centered) * calibration.volts_per_count;
}

int raw_to_screen_y(std::uint16_t raw, const ChannelSettings &settings)
{
    const float volts = raw_to_volts(raw, settings.calibration);
    const float division_value = volts / volts_per_div(settings);
    const float shifted_division = division_value + settings.vertical_offset_divs;
    // Display coordinates grow downward, so positive voltage moves up from the
    // vertical centerline.
    const float y = static_cast<float>(config::kDisplayHeight) * 0.5f -
                    shifted_division * static_cast<float>(config::kPixelsPerDivisionY);
    return static_cast<int>(y + (y >= 0.0f ? 0.5f : -0.5f));
}

ScopeSettings default_scope_settings()
{
    ScopeSettings settings = {};
    settings.channels[1].color = config::kColorCh2;
    return settings;
}

ScopeUpdate apply_input_events(ScopeSettings &settings, const InputEvents &events)
{
    ScopeUpdate update = {};

    const Channel selected_channel = channel_from_switch(events.channel_switch_active);
    if (settings.active_channel != selected_channel ||
        settings.trigger.source != selected_channel) {
        settings.active_channel = selected_channel;
        settings.trigger.source = settings.active_channel;
        request_capture_reset(update);
    }

    const ControlAxis selected_axis = control_axis_from_switch(events.horizontal_switch_active);
    if (settings.control_axis != selected_axis) {
        settings.control_axis = selected_axis;
        request_redraw(update);
    }

    if (events.trigger_delta != 0) {
        const std::int32_t requested =
            static_cast<std::int32_t>(settings.trigger.level_count) +
            static_cast<std::int32_t>(events.trigger_delta) *
                config::kTriggerEncoderCountsPerTransition;
        const std::uint16_t clamped = static_cast<std::uint16_t>(
            clamp_value<std::int32_t>(requested, 0, config::kAdcMaxCount));
        if (clamped != settings.trigger.level_count) {
            settings.trigger.level_count = clamped;
            request_capture_reset(update);
        }
    }

    ChannelSettings &channel = settings.channels[channel_index(settings.active_channel)];
    if (settings.control_axis == ControlAxis::Vertical) {
        if (apply_vertical_position_delta(channel, events.shift_delta)) {
            request_redraw(update);
        }
        if (apply_volts_scale_delta(channel, events.scale_delta)) {
            request_redraw(update);
        }
    } else {
        if (apply_horizontal_position_delta(channel, events.shift_delta)) {
            request_redraw(update);
        }
        if (events.scale_delta != 0) {
            const std::int32_t requested =
                static_cast<std::int32_t>(settings.timebase_index) +
                events.scale_delta;
            const std::uint8_t clamped =
                clamp_table_index(requested, config::kTimebaseCount);
            if (clamped != settings.timebase_index) {
                settings.timebase_index = clamped;
                request_capture_reset(update);
            }
        }
    }

    return update;
}

const char *channel_label(Channel channel)
{
    return channel == Channel::Ch1 ? "CH1" : "CH2";
}

const char *trigger_edge_label(TriggerEdge edge)
{
    return edge == TriggerEdge::Rising ? "RISE" : "FALL";
}

const char *volts_scale_label(const ChannelSettings &channel)
{
    return config::kVoltsScales[channel.volts_scale_index].label;
}

float volts_per_div(const ChannelSettings &channel)
{
    return config::kVoltsScales[channel.volts_scale_index].volts_per_div;
}

const char *timebase_label(const ScopeSettings &settings)
{
    return config::kTimebases[settings.timebase_index].label;
}

TimebaseRatio timebase_nominal_ratio(const ScopeSettings &settings)
{
    const config::Timebase &timebase = config::kTimebases[settings.timebase_index];
    return TimebaseRatio{timebase.nominal_pair_numerator,
                         timebase.nominal_pair_denominator};
}

std::uint16_t timebase_decimation(const ScopeSettings &settings)
{
    const TimebaseRatio nominal = timebase_nominal_ratio(settings);
    if (nominal.numerator <= nominal.denominator) {
        return 1u;
    }
    const std::uint32_t nominal_pairs = nominal.numerator / nominal.denominator;
    return nominal_pairs > config::kMaxHistoryTimebaseDecimation
               ? config::kMaxHistoryTimebaseDecimation
               : static_cast<std::uint16_t>(nominal_pairs);
}

bool timebase_uses_interpolation(const ScopeSettings &settings)
{
    const TimebaseRatio nominal = timebase_nominal_ratio(settings);
    return nominal.numerator < nominal.denominator;
}

std::uint16_t timebase_interpolation_factor(const ScopeSettings &settings)
{
    const TimebaseRatio nominal = timebase_nominal_ratio(settings);
    if (nominal.numerator >= nominal.denominator) {
        return 1u;
    }
    return static_cast<std::uint16_t>(nominal.denominator / nominal.numerator);
}

std::uint32_t timebase_frame_pair_count(const ScopeSettings &settings)
{
    const TimebaseRatio nominal = timebase_nominal_ratio(settings);
    if (nominal.numerator >= nominal.denominator) {
        return static_cast<std::uint32_t>(config::kDisplayWidth) *
               timebase_decimation(settings);
    }

    const std::uint32_t last_column = config::kDisplayWidth - 1u;
    return (last_column * nominal.numerator + nominal.denominator - 1u) /
               nominal.denominator +
           1u;
}

std::uint32_t timebase_pretrigger_pair_count(const ScopeSettings &settings)
{
    const TimebaseRatio nominal = timebase_nominal_ratio(settings);
    if (nominal.numerator >= nominal.denominator) {
        return static_cast<std::uint32_t>(config::kDefaultTriggerColumn) *
               timebase_decimation(settings);
    }

    return (static_cast<std::uint32_t>(config::kDefaultTriggerColumn) *
                nominal.numerator +
            nominal.denominator - 1u) /
           nominal.denominator;
}

std::uint32_t timebase_posttrigger_pair_count(const ScopeSettings &settings)
{
    return timebase_frame_pair_count(settings) -
           timebase_pretrigger_pair_count(settings);
}

std::uint32_t timebase_sample_pairs_per_second(const ScopeSettings &settings)
{
    const TimebaseRatio nominal = timebase_nominal_ratio(settings);
    const std::uint16_t capture = timebase_decimation(settings);
    const std::uint64_t requested =
        static_cast<std::uint64_t>(config::kAdcSamplesPerSecondPerChannel) *
        capture * nominal.denominator;
    const std::uint32_t adjusted = static_cast<std::uint32_t>(
        (requested + nominal.numerator / 2u) / nominal.numerator);
    return adjusted > config::kAdcSamplesPerSecondPerChannel
               ? config::kAdcSamplesPerSecondPerChannel
               : adjusted;
}

float timebase_adc_clkdiv(const ScopeSettings &settings)
{
    const std::uint32_t aggregate_rate =
        timebase_sample_pairs_per_second(settings) * 2u;
    if (aggregate_rate >= config::kAdcAggregateSamplesPerSecond) {
        return 0.0f;
    }

    return static_cast<float>(config::kAdcClockHz) /
               static_cast<float>(aggregate_rate) -
           1.0f;
}

} // namespace picoscope
