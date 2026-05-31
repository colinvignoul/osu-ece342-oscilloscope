#include "strip_renderer.hpp"

#include "font_config.hpp"
#include "scope_control.hpp"

namespace picoscope {
namespace {

#if defined(__GNUC__) || defined(__clang__)
#define PICOSCOPE_NOINLINE __attribute__((noinline))
#else
#define PICOSCOPE_NOINLINE
#endif

constexpr int kGlyphAdvance = 6;

int abs_int(int value)
{
    return value < 0 ? -value : value;
}

void swap_int(int &a, int &b)
{
    const int tmp = a;
    a = b;
    b = tmp;
}

bool has_column_value(const ColumnSampleBucket &bucket, std::uint8_t channel)
{
    return bucket.valid[channel];
}

char frame_origin_marker(const ScopeFrame &frame)
{
    if (!frame.complete) {
        return '-';
    }
    if (frame.trigger_event.origin == FrameOrigin::Trigger) {
        return 'T';
    }
    if (frame.trigger_event.origin == FrameOrigin::Auto) {
        return 'A';
    }
    return frame.triggered ? 'T' : 'A';
}

char trigger_direction_marker(const ScopeFrame &frame)
{
    if (frame.trigger_event.origin != FrameOrigin::Trigger ||
        !frame.trigger_event.has_previous_count) {
        return '\0';
    }
    if (frame.trigger_event.sample_direction == TriggerSampleDirection::Rising) {
        return '+';
    }
    if (frame.trigger_event.sample_direction == TriggerSampleDirection::Falling) {
        return '-';
    }
    if (frame.trigger_event.sample_direction == TriggerSampleDirection::Flat) {
        return '.';
    }
    return '\0';
}

} // namespace


StripRenderer::StripRenderer(const ScopeFrame &frame, const ScopeSettings &settings)
    : frame_(frame)
    , settings_(settings)
{
}

void StripRenderer::render_strip(std::uint8_t strip_index, config::Rgb565 *buffer) const
{
    fill(buffer, config::kColorBackground);
    draw_grid(strip_index, buffer);
    draw_trigger_marker(strip_index, buffer);
    draw_trace(strip_index, buffer, 0);
    draw_trace(strip_index, buffer, 1);
    if (strip_index == 0) {
        draw_status(buffer);
    }
}

void StripRenderer::fill(config::Rgb565 *buffer, config::Rgb565 color) const
{
    const std::uint32_t count =
        static_cast<std::uint32_t>(config::kDisplayWidth) * config::kStripRows;
    for (std::uint32_t i = 0; i < count; ++i) {
        buffer[i] = color;
    }
}

void StripRenderer::draw_pixel(config::Rgb565 *buffer,
                               int x,
                               int y,
                               config::Rgb565 color) const
{
    if (x < 0 || x >= config::kDisplayWidth || y < 0 || y >= config::kStripRows) {
        return;
    }
    buffer[static_cast<std::uint32_t>(y) * config::kDisplayWidth +
           static_cast<std::uint32_t>(x)] = color;
}

void StripRenderer::draw_hline(config::Rgb565 *buffer,
                               int x,
                               int y,
                               int width,
                               config::Rgb565 color) const
{
    if (width <= 0 || y < 0 || y >= config::kStripRows) {
        return;
    }
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (x >= config::kDisplayWidth) {
        return;
    }
    if (x + width > config::kDisplayWidth) {
        width = config::kDisplayWidth - x;
    }
    if (width <= 0) {
        return;
    }

    config::Rgb565 *row = buffer + static_cast<std::uint32_t>(y) * config::kDisplayWidth +
                          static_cast<std::uint32_t>(x);
    for (int dx = 0; dx < width; ++dx) {
        row[dx] = color;
    }
}

void StripRenderer::draw_vline(config::Rgb565 *buffer,
                               int x,
                               int y,
                               int height,
                               config::Rgb565 color) const
{
    if (height <= 0 || x < 0 || x >= config::kDisplayWidth) {
        return;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (y >= config::kStripRows) {
        return;
    }
    if (y + height > config::kStripRows) {
        height = config::kStripRows - y;
    }
    if (height <= 0) {
        return;
    }

    std::uint32_t offset = static_cast<std::uint32_t>(y) * config::kDisplayWidth +
                           static_cast<std::uint32_t>(x);
    for (int dy = 0; dy < height; ++dy) {
        buffer[offset] = color;
        offset += config::kDisplayWidth;
    }
}

void StripRenderer::draw_line(config::Rgb565 *buffer,
                              int x0,
                              int y0,
                              int x1,
                              int y1,
                              config::Rgb565 color) const
{
    // Integer Bresenham keeps trace connections cheap on the RP2040.
    const int dx = abs_int(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -abs_int(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        draw_pixel(buffer, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void StripRenderer::draw_grid(std::uint8_t strip_index, config::Rgb565 *buffer) const
{
    // Grid positions live in full-screen coordinates, then get clipped into
    // this strip's local buffer.
    const int strip_top = static_cast<int>(strip_index) * config::kStripRows;

    for (std::uint8_t col = 0; col <= config::kGridColumns; ++col) {
        const int x = col == config::kGridColumns
                          ? config::kDisplayWidth - 1
                          : col * config::kPixelsPerDivisionX;
        const config::Rgb565 color =
            col == config::kGridColumns / 2 ? config::kColorGridMajor : config::kColorGrid;
        draw_vline(buffer, x, 0, config::kStripRows, color);
    }

    for (std::uint8_t row = 0; row <= config::kGridRows; ++row) {
        const int y = row == config::kGridRows
                          ? config::kDisplayHeight - 1
                          : row * config::kPixelsPerDivisionY;
        if (y >= strip_top && y < strip_top + config::kStripRows) {
            const config::Rgb565 color =
                row == config::kGridRows / 2 ? config::kColorGridMajor : config::kColorGrid;
            draw_hline(buffer, 0, y - strip_top, config::kDisplayWidth, color);
        }
    }
}

void StripRenderer::draw_trigger_marker(std::uint8_t strip_index,
                                        config::Rgb565 *buffer) const
{
    const std::uint8_t source = channel_index(settings_.trigger.source);
    const int y_abs =
        raw_to_screen_y(settings_.trigger.level_count, settings_.channels[source]);
    const int strip_top = static_cast<int>(strip_index) * config::kStripRows;
    const int y = y_abs - strip_top;

    if (y < 0 || y >= config::kStripRows) {
        return;
    }
    for (int x = 0; x < config::kDisplayWidth; x += 8) {
        draw_hline(buffer, x, y, 4, config::kColorTrigger);
    }
}

void StripRenderer::draw_trace(std::uint8_t strip_index,
                               config::Rgb565 *buffer,
                               std::uint8_t ch) const
{
    if (!settings_.channels[ch].enabled) {
        return;
    }

    const int strip_top = static_cast<int>(strip_index) * config::kStripRows;
    bool have_previous = false;
    int previous_x = 0;
    int previous_y = 0;
    const int horizontal_offset = settings_.channels[ch].horizontal_offset_columns;

    for (std::uint16_t source_x = 0; source_x < config::kDisplayWidth; ++source_x) {
        const ColumnSampleBucket &bucket = frame_.columns[source_x];
        if (!has_column_value(bucket, ch)) {
            have_previous = false;
            continue;
        }
        const int x = static_cast<int>(source_x) + horizontal_offset;

        int y_top = raw_to_screen_y(bucket.max_count[ch], settings_.channels[ch]);
        int y_bottom = raw_to_screen_y(bucket.min_count[ch], settings_.channels[ch]);
        if (y_top > y_bottom) {
            swap_int(y_top, y_bottom);
        }

        // Draw the bucket's min/max span so fast excursions remain visible even
        // when several samples collapse into one display column.
        draw_vline(buffer,
                   x,
                   y_top - strip_top,
                   y_bottom - y_top + 1,
                   settings_.channels[ch].color);

        const int y_mid = (y_top + y_bottom) / 2;
        if (have_previous) {
            draw_line(buffer,
                      previous_x,
                      previous_y - strip_top,
                      x,
                      y_mid - strip_top,
                      settings_.channels[ch].color);
        }
        previous_x = x;
        previous_y = y_mid;
        have_previous = true;
    }
}

PICOSCOPE_NOINLINE int StripRenderer::draw_text(config::Rgb565 *buffer,
                                                int x,
                                                int y,
                                                const char *text,
                                                config::Rgb565 color) const
{
    if (text == nullptr) {
        return x;
    }
    for (const char *p = text; *p != '\0'; ++p) {
        draw_char(buffer, x, y, *p, color);
        x += kGlyphAdvance;
    }
    return x;
}

PICOSCOPE_NOINLINE void StripRenderer::draw_char(config::Rgb565 *buffer,
                                                 int x,
                                                 int y,
                                                 char c,
                                                 config::Rgb565 color) const
{
    const std::uint8_t *columns = font::glyph_for(c);
    for (int col = 0; col < font::kGlyphWidth; ++col) {
        const std::uint8_t bits = columns[col];
        for (int row = 0; row < font::kGlyphHeight; ++row) {
            if ((bits & (1u << row)) != 0u) {
                draw_pixel(buffer, x + col, y + row, color);
            }
        }
    }
}

void StripRenderer::draw_status(config::Rgb565 *buffer) const
{
    for (int y = 0; y < 10; ++y) {
        draw_hline(buffer, 0, y, config::kDisplayWidth, config::kColorBackground);
    }

    int x = 2;
    constexpr int y = 1;
    x = draw_text(buffer, x, y, channel_label(Channel::Ch1), config::kColorStatus);
    x = draw_text(buffer, x, y, " ", config::kColorStatus);
    x = draw_text(buffer, x, y, volts_scale_label(settings_.channels[0]), config::kColorStatus);
    x = draw_text(buffer, x, y, "/D ", config::kColorStatus);
    x = draw_text(buffer, x, y, channel_label(Channel::Ch2), config::kColorStatus);
    x = draw_text(buffer, x, y, " ", config::kColorStatus);
    x = draw_text(buffer, x, y, volts_scale_label(settings_.channels[1]), config::kColorStatus);
    x = draw_text(buffer, x, y, "/D ", config::kColorStatus);
    x = draw_text(buffer, x, y, timebase_label(settings_), config::kColorStatus);
    x = draw_text(buffer, x, y, "/D T", config::kColorStatus);
    x = draw_text(buffer, x, y, channel_label(settings_.trigger.source), config::kColorStatus);
    x = draw_text(buffer, x, y, " ", config::kColorStatus);
    draw_char(buffer,
              x,
              y,
              settings_.trigger.edge == TriggerEdge::Rising ? 'R' : 'F',
              config::kColorStatus);
    x += kGlyphAdvance;
    x = draw_text(buffer, x, y, " ", config::kColorStatus);
    draw_char(buffer, x, y, frame_origin_marker(frame_), config::kColorStatus);
    x += kGlyphAdvance;
    const char direction_marker = trigger_direction_marker(frame_);
    if (direction_marker != '\0') {
        draw_char(buffer, x, y, direction_marker, config::kColorStatus);
        x += kGlyphAdvance;
    }
    x = draw_text(buffer, x, y, " ", config::kColorStatus);
    draw_text(buffer, x, y, settings_.running ? "RUN" : "HOLD", config::kColorStatus);
}

} // namespace picoscope

#undef PICOSCOPE_NOINLINE
