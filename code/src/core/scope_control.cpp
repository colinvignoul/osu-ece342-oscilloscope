#include "scope_control.hpp"

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

std::uint8_t clamp_table_index(std::int16_t index, std::size_t count)
{
    const std::int16_t last = static_cast<std::int16_t>(count - 1u);
    return static_cast<std::uint8_t>(clamp_value<std::int16_t>(index, 0, last));
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

    if (events.trigger_delta != 0) {
        const std::int32_t requested =
            static_cast<std::int32_t>(settings.trigger.level_count) +
            static_cast<std::int32_t>(events.trigger_delta) *
                config::kTriggerEncoderCountsPerDetent;
        const std::uint16_t clamped = static_cast<std::uint16_t>(
            clamp_value<std::int32_t>(requested, 0, config::kAdcMaxCount));
        if (clamped != settings.trigger.level_count) {
            settings.trigger.level_count = clamped;
            request_capture_reset(update);
        }
    }

    if (events.voltage_delta != 0) {
        ChannelSettings &channel = settings.channels[channel_index(settings.active_channel)];
        const std::int16_t requested =
            static_cast<std::int16_t>(channel.volts_scale_index) + events.voltage_delta;
        const std::uint8_t clamped = clamp_table_index(requested, config::kVoltsScaleCount);
        if (clamped != channel.volts_scale_index) {
            channel.volts_scale_index = clamped;
            request_redraw(update);
        }
    }

    if (events.channel_button_pressed) {
        settings.active_channel = other_channel(settings.active_channel);
        settings.trigger.source = settings.active_channel;
        request_capture_reset(update);
    }

    if (events.time_delta != 0) {
        const std::int16_t requested =
            static_cast<std::int16_t>(settings.timebase_index) + events.time_delta;
        const std::uint8_t clamped = clamp_table_index(requested, config::kTimebaseCount);
        if (clamped != settings.timebase_index) {
            settings.timebase_index = clamped;
            request_capture_reset(update);
        }
    }

    if (events.run_button_pressed) {
        settings.running = !settings.running;
        request_capture_reset(update);
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

std::uint16_t timebase_nominal_decimation(const ScopeSettings &settings)
{
    return config::kTimebases[settings.timebase_index].nominal_decimation;
}

std::uint16_t timebase_decimation(const ScopeSettings &settings)
{
    const std::uint16_t nominal = timebase_nominal_decimation(settings);
    return nominal > config::kMaxHistoryTimebaseDecimation
               ? config::kMaxHistoryTimebaseDecimation
               : nominal;
}

std::uint32_t timebase_sample_pairs_per_second(const ScopeSettings &settings)
{
    const std::uint16_t nominal = timebase_nominal_decimation(settings);
    const std::uint16_t capture = timebase_decimation(settings);
    return static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(config::kAdcSamplesPerSecondPerChannel) *
             capture +
         nominal / 2u) /
        nominal);
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
