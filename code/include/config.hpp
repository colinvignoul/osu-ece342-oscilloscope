// Purpose: Defines compile-time hardware, display, ADC, color, and UI scale
// constants shared across the firmware.
// Interface: Exposes immutable values and small lookup tables under
// picoscope::config; callers read these values and never take ownership.
// Constraints: Values are set for a Raspberry Pi Pico/RP2040, two ADC
// channels, and a 320x240 ILI9341 panel using RGB565 pixels.
// Ownership: This file owns central configuration only; runtime state lives in
// ScopeSettings and the hardware/rendering classes.

#pragma once

#include <cstddef>
#include <cstdint>

namespace picoscope::config {

// Display config
constexpr std::uint16_t kDisplayWidth = 320;
constexpr std::uint16_t kDisplayHeight = 240;
constexpr std::uint8_t kGridColumns = 10;
constexpr std::uint8_t kGridRows = 8;
constexpr std::uint8_t kStripRows = 24;
constexpr std::uint8_t kNumStrips = kDisplayHeight / kStripRows;
constexpr std::uint16_t kPixelsPerDivisionX = kDisplayWidth / kGridColumns;
constexpr std::uint16_t kPixelsPerDivisionY = kDisplayHeight / kGridRows;
constexpr std::uint32_t kLcdSpiBaudHz = 62500000u;

// Pico GPIO config 
constexpr std::uint8_t kShiftEncA = 0;
constexpr std::uint8_t kShiftEncB = 1;
constexpr std::uint8_t kScaleEncA = 6;
constexpr std::uint8_t kScaleEncB = 2;
constexpr std::uint8_t kTriggerEncA = 3;
constexpr std::uint8_t kTriggerEncB = 4;
constexpr std::uint8_t kChannelSwitch = 5;
constexpr std::uint8_t kHorizontalSwitch = 7;
constexpr std::uint8_t kRunHoldSwitch = 8;
constexpr std::uint8_t kLcdPinCs = 17;
constexpr std::uint8_t kLcdPinSck = 18;
constexpr std::uint8_t kLcdPinMosi = 19;
constexpr std::uint8_t kLcdPinDc = 20;
constexpr std::uint8_t kLcdPinRst = 21;
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
constexpr std::uint32_t kAdcAggregateSamplesPerSecond = 500000u;
constexpr std::uint32_t kAdcSamplesPerSecondPerChannel = kAdcAggregateSamplesPerSecond / 2u;
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
    static_cast<std::uint16_t>((kAdcHistoryRingPairs - kAdcHistoryGuardPairs) / kDisplayWidth);
static_assert(kMaxHistoryTimebaseDecimation > 0u,
              "ADC history ring must hold at least one display frame.");

// Set where the trigger event should be on the display
constexpr std::uint16_t kDefaultTriggerColumn = kDisplayWidth / 2u;

// Auto-trigger settings
constexpr std::uint32_t kAutoTriggerFramePeriods = 1u; // this should stay at 1 for now
constexpr std::uint32_t kAutoTriggerMinimumWaitUs = 50000u;

// Trigger sensitivity settings
constexpr std::uint16_t kTriggerHysteresisCounts = 4u;
constexpr std::uint8_t kTriggerArmDwellSamples = 2u;
constexpr std::uint16_t kTriggerOppositeEdgeHoldoffColumns = 2u;
constexpr std::uint16_t kTriggerOppositeEdgeHoldoffMinimumPairs = 4u;
constexpr std::uint8_t kFastTriggerArmDwellSamples = 1u;
constexpr std::uint32_t kFastTriggerOppositeEdgeHoldoffPairs = 1u;


constexpr bool kSwitchActiveLevel = true;

// Define colors for use in rendering
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

// Vertical scale: numeric volts-per-division plus its UI label.
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
};
constexpr std::size_t kVoltsScaleCount = sizeof(kVoltsScales) / sizeof(kVoltsScales[0]);
constexpr std::uint8_t kDefaultVoltsScaleIndex = 2;

// Horizontal scale: nominal sample pairs per pixel plus its UI label.
struct Timebase {
    std::uint16_t nominal_pair_numerator;
    std::uint16_t nominal_pair_denominator;
    const char *label;
};

// Nominal pair ratio is sample pairs per pixel at the maximum ADC rate.
// Labels are time per horizontal division.
constexpr Timebase kTimebases[] = {
    {1, 4, "32us"},
    {1, 2, "64us"},
    {1, 1, "128us"},
    {2, 1, "256us"},
    {5, 1, "640us"},
    {10, 1, "1.28ms"},
    {20, 1, "2.56ms"},
    {50, 1, "6.4ms"},
    {100, 1, "12.8ms"},
    //{200, 1, "25.6ms"},
    //{500, 1, "64ms"},
    //{1000, 1, "128ms"},
};
constexpr std::size_t kTimebaseCount = sizeof(kTimebases) / sizeof(kTimebases[0]);
constexpr std::uint8_t kDefaultTimebaseIndex = 5;

constexpr std::int16_t kHorizontalOffsetColumnsPerDetent = 1;
constexpr std::uint32_t kEncoderDetentDuplicateGuardUs = 1000;

} // namespace picoscope::config
