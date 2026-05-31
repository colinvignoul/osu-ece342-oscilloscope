#pragma once

#include <cstdint>

#include "config.hpp"
#include "ili9341.hpp"

namespace picoscope {

// Owns display DMA state and borrows the lower-level ILI9341 command driver.
class DisplayDma {
public:
    explicit DisplayDma(Ili9341 &display);

    void init();

    // Takes a renderer with render_strip(strip_index, buffer),
    // renders/transfers all strips synchronously, and returns nothing.
    template <typename Renderer>
    void render_frame(const Renderer &renderer);

    void flush_strip_async(std::uint8_t strip_index, std::uint8_t buffer_index);

    void wait();

    config::Rgb565 *strip_buffer(std::uint8_t buffer_index);

private:
    static constexpr std::uint32_t kStripPixels =
        static_cast<std::uint32_t>(config::kDisplayWidth) * config::kStripRows;

    Ili9341 &display_;
    config::Rgb565 strip_buffers_[2][kStripPixels] = {};
    int dma_channel_ = -1;
    bool transfer_in_flight_ = false;
};

template <typename Renderer>
void DisplayDma::render_frame(const Renderer &renderer)
{
    std::uint8_t buffer_index = 0;
    // Ping-pong strip buffers let rendering for the next strip overlap
    // the DMA transfer of the current band.
    renderer.render_strip(0, strip_buffers_[buffer_index]);
    flush_strip_async(0, buffer_index);
    buffer_index ^= 1u;

    for (std::uint8_t strip = 1; strip < config::kNumStrips; ++strip) {
        renderer.render_strip(strip, strip_buffers_[buffer_index]);
        wait();
        flush_strip_async(strip, buffer_index);
        buffer_index ^= 1u;
    }
    wait();
}

} // namespace picoscope
