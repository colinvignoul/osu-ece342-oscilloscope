// Purpose: Defines compile-time hardware, display, ADC, color, and UI scale
// constants shared across the firmware.
// Interface: Exposes immutable values and small lookup tables under
// picoscope::config; callers read these values and never take ownership.
// Constraints: Values are tuned for a Raspberry Pi Pico/RP2040, two ADC
// channels, and a 320x240 ILI9341 panel using RGB565 pixels.
// Ownership: This file owns central configuration only; runtime state lives in
// ScopeSettings and the hardware/rendering classes.

#pragma once

#include <cstddef>
#include <cstdint>

namespace picoscope::config {

constexpr std::uint16_t kDisplayWidth = 320;
constexpr std::uint16_t kDisplayHeight = 240;
constexpr std::uint8_t kGridColumns = 10;
constexpr std::uint8_t kGridRows = 8;
// The display is flushed in strips so double-buffered DMA storage stays small
// while avoiding excessive per-strip render/setup overhead.
constexpr std::uint8_t kStripRows = 24;
constexpr std::uint8_t kNumStrips = kDisplayHeight / kStripRows;
constexpr std::uint16_t kPixelsPerDivisionX = kDisplayWidth / kGridColumns;
constexpr std::uint16_t kPixelsPerDivisionY = kDisplayHeight / kGridRows;

constexpr std::uint8_t kLcdPinCs = 17;
constexpr std::uint8_t kLcdPinSck = 18;
constexpr std::uint8_t kLcdPinMosi = 19;
constexpr std::uint8_t kLcdPinDc = 20;
constexpr std::uint8_t kLcdPinRst = 21;
constexpr std::uint8_t kLcdPinLite = 22;
constexpr std::uint32_t kLcdSpiBaudHz = 62500000u;

constexpr std::uint8_t kAdcPinCh1 = 26;
constexpr std::uint8_t kAdcPinCh2 = 27;
constexpr std::uint8_t kAdcInputCh1 = 0;
constexpr std::uint8_t kAdcInputCh2 = 1;
constexpr std::uint16_t kAdcMaxCount = 4095;
constexpr std::uint16_t kAdcMidscaleCount = 2048;
constexpr std::uint32_t kAdcInputFullScaleMillivolts = 3300u;
constexpr std::uint32_t kAdcBiasMillivolts = 1250u;
constexpr float kAdcInputFullScaleVolts =
    static_cast<float>(kAdcInputFullScaleMillivolts) / 1000.0f;
constexpr float kAdcBiasVolts = static_cast<float>(kAdcBiasMillivolts) / 1000.0f;
constexpr float kAnalogFrontendGain = 0.5f;
constexpr float kAdcSignalMaxPeakToPeakVolts = 2.5f;
constexpr float kMeasuredSignalMaxPeakToPeakVolts =
    kAdcSignalMaxPeakToPeakVolts / kAnalogFrontendGain;
constexpr std::uint16_t kAdcBiasCount = static_cast<std::uint16_t>(
    (kAdcBiasMillivolts * kAdcMaxCount + kAdcInputFullScaleMillivolts / 2u) /
    kAdcInputFullScaleMillivolts);
static_assert(kAdcBiasCount == 1551u, "ADC 1.25 V bias count must match 3.3 V span.");
constexpr float kDefaultVoltsPerCount =
    (kAdcInputFullScaleVolts / static_cast<float>(kAdcMaxCount)) /
    kAnalogFrontendGain;
constexpr std::uint32_t kAdcClockHz = 48000000u;
constexpr std::uint32_t kAdcConversionCycles = 96u;
// Aggregate rate is shared by both round-robin channels.
constexpr std::uint32_t kAdcAggregateSamplesPerSecond = 500000u;
static_assert(kAdcClockHz / kAdcConversionCycles == kAdcAggregateSamplesPerSecond,
              "ADC aggregate sample rate must match the RP2040 conversion rate.");
constexpr std::uint32_t kAdcSamplesPerSecondPerChannel =
    kAdcAggregateSamplesPerSecond / 2u;
constexpr std::uint32_t kAdcDmaBlockWords = 4096u;
constexpr std::uint32_t kAdcHistoryRingBytes = 32768u;
constexpr std::uint32_t kAdcHistoryRingWords =
    kAdcHistoryRingBytes / sizeof(std::uint16_t);
constexpr std::uint32_t kAdcHistoryRingPairs = kAdcHistoryRingWords / 2u;
static_assert((kAdcHistoryRingPairs & (kAdcHistoryRingPairs - 1u)) == 0u,
              "ADC history ring pair count must be a power of two.");
constexpr std::uint32_t kAdcHistoryRingBits = 15u;
constexpr std::uint32_t kAdcHistoryGuardPairs = 2u;
constexpr std::uint32_t kAdcHistoryTransferWords = 0xFFFFFFFFu;
constexpr std::uint16_t kMaxHistoryTimebaseDecimation =
    static_cast<std::uint16_t>((kAdcHistoryRingPairs - kAdcHistoryGuardPairs) /
                               kDisplayWidth);
static_assert(kMaxHistoryTimebaseDecimation > 0u,
              "ADC history ring must hold at least one display frame.");

constexpr std::uint16_t kDefaultTriggerColumn = kDisplayWidth / 2u;

constexpr std::uint32_t kAutoTriggerFramePeriods = 1u;
constexpr std::uint32_t kAutoTriggerMinimumWaitUs = 50000u;
constexpr std::uint16_t kTriggerHysteresisCounts = 16u;
constexpr std::uint8_t kTriggerArmDwellSamples = 3u;
constexpr std::uint16_t kTriggerOppositeEdgeHoldoffColumns = 2u;
constexpr std::uint16_t kTriggerOppositeEdgeHoldoffMinimumPairs = 16u;

constexpr std::uint8_t kShiftEncA = 0;
constexpr std::uint8_t kShiftEncB = 1;
constexpr std::uint8_t kScaleEncA = 6;
constexpr std::uint8_t kScaleEncB = 2;
constexpr std::uint8_t kTriggerEncA = 3;
constexpr std::uint8_t kTriggerEncB = 4;
constexpr std::uint8_t kChannelSwitch = 5;
constexpr std::uint8_t kHorizontalSwitch = 7;
constexpr bool kSwitchActiveLevel = true;

// RGB565 pixels are stored in host memory as 16-bit words ready for display DMA.
using Rgb565 = std::uint16_t;
constexpr Rgb565 kColorBlack = 0x0000;
constexpr Rgb565 kColorWhite = 0xFFFF;
constexpr Rgb565 kColorGrid = 0x3186;
constexpr Rgb565 kColorGridMajor = 0x632C;
constexpr Rgb565 kColorCh1 = 0x07E0;
constexpr Rgb565 kColorCh2 = 0xFD20;
constexpr Rgb565 kColorTrigger = 0xF81F;
constexpr Rgb565 kColorStatus = 0xFFFF;
constexpr Rgb565 kColorBackground = 0x0000;

// One vertical scale option: numeric volts-per-division plus its UI label.
struct VoltsScale {
    float volts_per_div;
    const char *label;
};

constexpr VoltsScale kVoltsScales[] = {
    {0.05f, "50mV"},
    {0.10f, "100mV"},
    {0.20f, "200mV"},
    {0.50f, "500mV"},
    {1.00f, "1V"},
    {2.00f, "2V"},
    {5.00f, "5V"},
    {10.0f, "10V"},
};
constexpr std::size_t kVoltsScaleCount = sizeof(kVoltsScales) / sizeof(kVoltsScales[0]);
constexpr std::uint8_t kDefaultVoltsScaleIndex = 2;

// One horizontal scale option: nominal sample-pair decimation plus its UI label.
struct Timebase {
    std::uint16_t nominal_decimation;
    const char *label;
};

// Labels are time per horizontal division; nominal decimation is sample pairs
// per pixel at the maximum ADC rate.
constexpr Timebase kTimebases[] = {
    {1, "128us"},
    {2, "256us"},
    {5, "640us"},
    {10, "1.28ms"},
    {20, "2.56ms"},
    {50, "6.4ms"},
    {100, "12.8ms"},
    {200, "25.6ms"},
    {500, "64ms"},
    {1000, "128ms"},
};
constexpr std::size_t kTimebaseCount = sizeof(kTimebases) / sizeof(kTimebases[0]);
constexpr std::uint8_t kDefaultTimebaseIndex = 5;

constexpr std::int16_t kTriggerEncoderCountsPerTransition = 2;
constexpr std::int16_t kPositionEncoderPixelsPerTransition = 1;
constexpr float kVerticalOffsetDivsPerTransition =
    static_cast<float>(kPositionEncoderPixelsPerTransition) /
    static_cast<float>(kPixelsPerDivisionY);
constexpr std::uint32_t kEncoderIrqDebounceUs = 100;

} // namespace picoscope::config
