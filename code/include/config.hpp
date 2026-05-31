// Purpose: Defines compile-time hardware, display, ADC, color, and UI scale
// constants shared across the firmware.
// Interface: Exposes immutable values and small lookup tables under
// picoscope::config; callers read these values and never take ownership.
// Constraints: Values are tuned for a Raspberry Pi Pico/RP2040, two ADC
// channels, and a 320x240 ILI9341 panel using RGB565 pixels.
// Ownership: This file owns central configuration only; runtime state lives in
// ScopeSettings and the hardware/rendering classes.



// TODO separate into "non-adjustable" and "adjutable" sections



#pragma once

#include <cstddef>
#include <cstdint>

namespace picoscope::config {

constexpr std::uint16_t kDisplayWidth = 320; // Display width in pixels
constexpr std::uint16_t kDisplayHeight = 240; // Display height in pixels

constexpr std::uint8_t kGridColumns = 10; // Number of horizontal (time) divisions
constexpr std::uint8_t kGridRows = 8; // Number of vertical (voltage) divisions
constexpr std::uint8_t kStripRows = 24; // Render strip height in pixels
constexpr std::uint8_t kNumStrips = kDisplayHeight / kStripRows; // Number of strips to render one full frame
constexpr std::uint16_t kPixelsPerDivisionX = kDisplayWidth / kGridColumns;
constexpr std::uint16_t kPixelsPerDivisionY = kDisplayHeight / kGridRows;

// Define pins for SPI connection with display
constexpr std::uint8_t kLcdPinCs = 17;
constexpr std::uint8_t kLcdPinSck = 18;
constexpr std::uint8_t kLcdPinMosi = 19;
constexpr std::uint8_t kLcdPinDc = 20;
constexpr std::uint8_t kLcdPinRst = 21;
constexpr std::uint8_t kLcdPinLite = 22;
constexpr std::uint32_t kLcdSpiBaudHz = 62500000u; // Display SPI baudrate


constexpr std::uint8_t kAdcPinCh1 = 26;
constexpr std::uint8_t kAdcPinCh2 = 27;
constexpr std::uint8_t kAdcInputCh1 = 0;
constexpr std::uint8_t kAdcInputCh2 = 1;
constexpr std::uint16_t kAdcMaxCount = 4095; // 12-bit ADC = 4096 possible values
constexpr std::uint16_t kAdcMidscaleCount = 2048; 
constexpr std::uint32_t kAdcInputFullScaleMillivolts = 3300u;
constexpr std::uint32_t kAdcBiasMillivolts = 1250u;
constexpr float kAdcInputFullScaleVolts =
    static_cast<float>(kAdcInputFullScaleMillivolts) / 1000.0f;
constexpr float kAdcBiasVolts = static_cast<float>(kAdcBiasMillivolts) / 1000.0f;
constexpr float kAnalogFrontendGain = 0.5f;
// constexpr float kAdcSignalMaxPeakToPeakVolts = 2.5f;
// constexpr float kMeasuredSignalMaxPeakToPeakVolts = kAdcSignalMaxPeakToPeakVolts / kAnalogFrontendGain;
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

// kDefaultTriggerColumn determines how much pre-trigger waveform is drawn
// by defining the display column on which to center the waveform's 
// triggering samples.
// The default here is centered (50% pre-trigger, 50% post-trigger).
constexpr std::uint16_t kDefaultTriggerColumn = kDisplayWidth / 2u; 

// These settings have to do with auto-trigger behavior
// and ensuring transients don't cause traces to jitter.
constexpr std::uint32_t kAutoTriggerFramePeriods = 1u;
constexpr std::uint32_t kAutoTriggerMinimumWaitUs = 50000u;
constexpr std::uint16_t kTriggerHysteresisCounts = 16u;
constexpr std::uint8_t kTriggerArmDwellSamples = 3u;
constexpr std::uint16_t kTriggerOppositeEdgeHoldoffColumns = 2u;
constexpr std::uint16_t kTriggerOppositeEdgeHoldoffMinimumPairs = 16u;

constexpr std::uint8_t kTimeEncA = 0;
constexpr std::uint8_t kTimeEncB = 1;
constexpr std::uint8_t kVoltageEncA = 6;
constexpr std::uint8_t kVoltageEncB = 2;
constexpr std::uint8_t kTriggerEncA = 3;
constexpr std::uint8_t kTriggerEncB = 4;
constexpr std::uint8_t kChannelButton = 5;
constexpr std::uint8_t kRunButton = 7;
constexpr bool kStandaloneButtonPressedLevel = true;

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

// Horizontal scale: nominal sample-pair decimation plus its UI label.
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
    {50, "6.4ms"},   // Dynamic ADC downsampling begins at 6.4 ms/div;
    {100, "12.8ms"}, // ADC sample rate continues to decrease with increasing
    {200, "25.6ms"}, // timebase in order to maintain display refresh rate
    {500, "64ms"},
    {1000, "128ms"},
};
constexpr std::size_t kTimebaseCount = sizeof(kTimebases) / sizeof(kTimebases[0]);
constexpr std::uint8_t kDefaultTimebaseIndex = 5;

constexpr std::int16_t kTriggerEncoderCountsPerDetent = 8;
constexpr std::uint8_t kEncoderTransitionsPerDetent = 4;
constexpr std::uint32_t kEncoderIrqDebounceUs = 100;
constexpr std::uint32_t kButtonDebounceUs = 5000;

} // namespace picoscope::config
