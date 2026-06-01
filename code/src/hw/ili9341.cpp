#include "ili9341.hpp"

#include "config.hpp"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

namespace picoscope {
namespace {

constexpr std::uint8_t kSleepOut = 0x11;
constexpr std::uint8_t kDisplayOn = 0x29;
constexpr std::uint8_t kColumnAddressSet = 0x2A;
constexpr std::uint8_t kPageAddressSet = 0x2B;
constexpr std::uint8_t kMemoryWrite = 0x2C;
constexpr std::uint8_t kMemoryAccessControl = 0x36;
constexpr std::uint8_t kPixelFormatSet = 0x3A;

struct InitCommand {
    std::uint8_t command;
    const std::uint8_t *data;
    std::size_t data_count;
};

constexpr std::uint8_t kCmdEf[] = {0x03, 0x80, 0x02};
constexpr std::uint8_t kPowerB[] = {0x00, 0xC1, 0x30};
constexpr std::uint8_t kPowerOn[] = {0x64, 0x03, 0x12, 0x81};
constexpr std::uint8_t kTimingA[] = {0x85, 0x00, 0x78};
constexpr std::uint8_t kPowerA[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
constexpr std::uint8_t kPumpRatio[] = {0x20};
constexpr std::uint8_t kTimingB[] = {0x00, 0x00};
constexpr std::uint8_t kPower1[] = {0x23};
constexpr std::uint8_t kPower2[] = {0x10};
constexpr std::uint8_t kVcom1[] = {0x3E, 0x28};
constexpr std::uint8_t kVcom2[] = {0x86};
constexpr std::uint8_t kMadctl[] = {0x28};
constexpr std::uint8_t kColmod[] = {0x55};
constexpr std::uint8_t kFrameRate[] = {0x00, 0x18};
constexpr std::uint8_t kDisplayFunction[] = {0x08, 0x82, 0x27};
constexpr std::uint8_t kGammaDisable[] = {0x00};
constexpr std::uint8_t kGammaCurve[] = {0x01};
constexpr std::uint8_t kPositiveGamma[] = {
    0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
    0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
};
constexpr std::uint8_t kNegativeGamma[] = {
    0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
    0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
};

constexpr InitCommand kInitCommands[] = {
    {0xEF, kCmdEf, sizeof(kCmdEf)},
    {0xCF, kPowerB, sizeof(kPowerB)},
    {0xED, kPowerOn, sizeof(kPowerOn)},
    {0xE8, kTimingA, sizeof(kTimingA)},
    {0xCB, kPowerA, sizeof(kPowerA)},
    {0xF7, kPumpRatio, sizeof(kPumpRatio)},
    {0xEA, kTimingB, sizeof(kTimingB)},
    {0xC0, kPower1, sizeof(kPower1)},
    {0xC1, kPower2, sizeof(kPower2)},
    {0xC5, kVcom1, sizeof(kVcom1)},
    {0xC7, kVcom2, sizeof(kVcom2)},
    {kMemoryAccessControl, kMadctl, sizeof(kMadctl)},
    {kPixelFormatSet, kColmod, sizeof(kColmod)},
    {0xB1, kFrameRate, sizeof(kFrameRate)},
    {0xB6, kDisplayFunction, sizeof(kDisplayFunction)},
    {0xF2, kGammaDisable, sizeof(kGammaDisable)},
    {0x26, kGammaCurve, sizeof(kGammaCurve)},
    {0xE0, kPositiveGamma, sizeof(kPositiveGamma)},
    {0xE1, kNegativeGamma, sizeof(kNegativeGamma)},
};

void cs_low()
{
    gpio_put(config::kLcdPinCs, 0);
}

void cs_high()
{
    gpio_put(config::kLcdPinCs, 1);
}

void dc_command()
{
    gpio_put(config::kLcdPinDc, 0);
}

void dc_data()
{
    gpio_put(config::kLcdPinDc, 1);
}

} // namespace

void Ili9341::init()
{
    spi_init(spi0, config::kLcdSpiBaudHz);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(config::kLcdPinSck, GPIO_FUNC_SPI);
    gpio_set_function(config::kLcdPinMosi, GPIO_FUNC_SPI);

    gpio_init(config::kLcdPinCs);
    gpio_set_dir(config::kLcdPinCs, GPIO_OUT);
    cs_high();

    gpio_init(config::kLcdPinDc);
    gpio_set_dir(config::kLcdPinDc, GPIO_OUT);
    dc_data();

    gpio_init(config::kLcdPinRst);
    gpio_set_dir(config::kLcdPinRst, GPIO_OUT);
    gpio_put(config::kLcdPinRst, 1);

    hardware_reset();

    // Common ILI9341 power, timing, orientation, and gamma bring-up sequence
    // for the 320x240 modules used by this project.
    for (const InitCommand &command : kInitCommands) {
        write_command_data(command.command, command.data, command.data_count);
    }

    write_command_data(kSleepOut, nullptr, 0);
    sleep_ms(120);
    write_command_data(kDisplayOn, nullptr, 0);
    sleep_ms(20);
}

void Ili9341::set_window(std::uint16_t x0,
                         std::uint16_t y0,
                         std::uint16_t x1,
                         std::uint16_t y1)
{
    // ILI9341 address-window commands expect big-endian 16-bit coordinates.
    const std::uint8_t column_data[4] = {
        static_cast<std::uint8_t>(x0 >> 8u),
        static_cast<std::uint8_t>(x0 & 0xFFu),
        static_cast<std::uint8_t>(x1 >> 8u),
        static_cast<std::uint8_t>(x1 & 0xFFu),
    };
    write_command_data(kColumnAddressSet, column_data, sizeof(column_data));

    const std::uint8_t page_data[4] = {
        static_cast<std::uint8_t>(y0 >> 8u),
        static_cast<std::uint8_t>(y0 & 0xFFu),
        static_cast<std::uint8_t>(y1 >> 8u),
        static_cast<std::uint8_t>(y1 & 0xFFu),
    };
    write_command_data(kPageAddressSet, page_data, sizeof(page_data));
}

void Ili9341::begin_pixels()
{
    write_command_data(kMemoryWrite, nullptr, 0);
    cs_low();
    dc_data();
}

void Ili9341::end_pixels()
{
    cs_high();
}

void Ili9341::hardware_reset()
{
    gpio_put(config::kLcdPinRst, 1);
    sleep_ms(5);
    gpio_put(config::kLcdPinRst, 0);
    sleep_ms(10);
    gpio_put(config::kLcdPinRst, 1);
    sleep_ms(120);
}

void Ili9341::write_command_data(std::uint8_t command,
                                 const std::uint8_t *data,
                                 std::size_t data_count)
{
    cs_low();
    dc_command();
    spi_write_blocking(spi0, &command, 1);
    if (data != nullptr && data_count > 0) {
        dc_data();
        spi_write_blocking(spi0, data, data_count);
    }
    // Do not release CS until the last command/data byte has left the shifter.
    while (spi_is_busy(spi0)) {
        tight_loop_contents();
    }
    cs_high();
}

} // namespace picoscope
