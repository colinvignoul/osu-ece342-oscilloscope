// Purpose: Implements strip-based RGB565 display transfers using SPI DMA.
// Interface: Call render_frame() with a renderer object, or manually flush
// strips and wait for the in-flight transfer to complete.
// Constraints: Pixel transfers temporarily use 16-bit SPI mode and must keep CS
// asserted until both DMA and the SPI shifter finish.
// Ownership: DisplayDma owns the strip buffers and DMA channel, while borrowing
// the Ili9341 command driver.

#include "display_dma.hpp"

#include "hardware/dma.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

namespace picoscope {

// Takes a display driver reference, stores it for command/pixel coordination,
// and returns a DisplayDma object.
DisplayDma::DisplayDma(Ili9341 &display)
    : display_(display)
{
}

// Takes no inputs, claims the display DMA channel, and returns nothing.
void DisplayDma::init()
{
    dma_channel_ = dma_claim_unused_channel(true);
}

// Takes a strip index and buffer index, configures display window and SPI DMA
// for that strip, starts transfer, and returns before completion.
void DisplayDma::flush_strip_async(std::uint8_t strip_index, std::uint8_t buffer_index)
{
    if (transfer_in_flight_) {
        wait();
    }

    const std::uint16_t y0 =
        static_cast<std::uint16_t>(strip_index * config::kStripRows);
    const std::uint16_t y1 =
        static_cast<std::uint16_t>(y0 + config::kStripRows - 1u);

    display_.set_window(0, y0, config::kDisplayWidth - 1u, y1);
    // Pixel payloads use 16-bit SPI transfers; wait() restores 8-bit mode for
    // command writes before the display is touched again.
    spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    display_.begin_pixels();

    dma_channel_config dma_config = dma_channel_get_default_config(dma_channel_);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, spi_get_dreq(spi0, true));

    dma_channel_configure(dma_channel_,
                          &dma_config,
                          &spi_get_hw(spi0)->dr,
                          strip_buffers_[buffer_index & 1u],
                          kStripPixels,
                          true);
    transfer_in_flight_ = true;
}

// Takes no inputs, blocks until the active DMA/SPI pixel transfer completes,
// restores command SPI mode, and returns nothing.
void DisplayDma::wait()
{
    if (!transfer_in_flight_) {
        return;
    }

    dma_channel_wait_for_finish_blocking(dma_channel_);
    // DMA completion only means the SPI FIFO was fed; keep CS asserted until
    // the SPI shifter has sent the final pixel.
    while (spi_is_busy(spi0)) {
        tight_loop_contents();
    }
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    display_.end_pixels();
    transfer_in_flight_ = false;
}

// Takes a buffer index, selects one of the two strip buffers, and returns a
// mutable pointer to it.
config::Rgb565 *DisplayDma::strip_buffer(std::uint8_t buffer_index)
{
    return strip_buffers_[buffer_index & 1u];
}

} // namespace picoscope
