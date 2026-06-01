#pragma once

#include <cstdint>

#include "config.hpp"
#include "scope_types.hpp"

namespace picoscope {

// Reads a frame/settings pair and writes caller buffers.
class StripRenderer {
public:
    // Constructor
    StripRenderer(const ScopeFrame &frame, const ScopeSettings &settings);

    // Takes a strip index and RGB565 buffer and renders that strip in place
    void render_strip(std::uint8_t strip_index, config::Rgb565 *buffer) const;

private:
    void fill(config::Rgb565 *buffer, config::Rgb565 color) const;

    void draw_pixel(config::Rgb565 *buffer,
                    int x,
                    int y,
                    config::Rgb565 color) const;

    void draw_hline(config::Rgb565 *buffer,
                    int x,
                    int y,
                    int width,
                    config::Rgb565 color) const;

    void draw_vline(config::Rgb565 *buffer,
                    int x,
                    int y,
                    int height,
                    config::Rgb565 color) const;

    void draw_line(config::Rgb565 *buffer,
                   int x0,
                   int y0,
                   int x1,
                   int y1,
                   config::Rgb565 color) const;

    void draw_grid(std::uint8_t strip_index, config::Rgb565 *buffer) const;

    void draw_trigger_marker(std::uint8_t strip_index, config::Rgb565 *buffer) const;

    void draw_trace(std::uint8_t strip_index,
                    config::Rgb565 *buffer,
                    std::uint8_t channel_index) const;

    int draw_text(config::Rgb565 *buffer,
                  int x,
                  int y,
                  const char *text,
                  config::Rgb565 color) const;

    void draw_char(config::Rgb565 *buffer,
                   int x,
                   int y,
                   char c,
                   config::Rgb565 color) const;

    void draw_status(config::Rgb565 *buffer) const;

    const ScopeFrame &frame_;
    const ScopeSettings &settings_;
};

} // namespace picoscope
