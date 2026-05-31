// Purpose: Declares the low-level ILI9341 SPI display driver.
// Interface: init() configures the panel, set_window() selects a drawing area,
// and begin_pixels()/end_pixels() bracket raw RGB565 pixel writes by DMA/SPI.
// Constraints: Command traffic uses 8-bit SPI words, while DisplayDma switches
// to 16-bit words for pixel payloads before returning to command mode.
// Ownership: Ili9341 owns no buffers; it controls configured GPIO/SPI hardware.

#pragma once

#include <cstddef>
#include <cstdint>

namespace picoscope {

// Low-level display command driver; pixel buffers are owned by callers.
class Ili9341 {
public:
    // Takes no inputs, initializes SPI/GPIO and sends the panel bring-up
    // sequence, and returns nothing.
    void init();

    // Takes inclusive display coordinates, programs the ILI9341 address window,
    // and returns nothing.
    void set_window(std::uint16_t x0,
                    std::uint16_t y0,
                    std::uint16_t x1,
                    std::uint16_t y1);

    // Takes no inputs, sends Memory Write and leaves the panel ready for pixel
    // data, and returns nothing.
    void begin_pixels();

    // Takes no inputs, ends the current pixel payload by releasing chip select,
    // and returns nothing.
    void end_pixels();

private:
    // Takes no inputs, toggles the hardware reset pin with required delays, and
    // returns nothing.
    void hardware_reset();

    // Takes one command plus optional payload bytes, writes them synchronously,
    // and returns nothing.
    void write_command_data(std::uint8_t command,
                            const std::uint8_t *data,
                            std::size_t data_count);
};

} // namespace picoscope
