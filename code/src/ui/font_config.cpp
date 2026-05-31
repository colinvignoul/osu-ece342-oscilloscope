/**
 * Purpose: Defines the 5x7 bitmap font used by display text rendering.
 * Interface: font::glyph_for() maps supported characters to column data.
 * Constraints: Glyphs are column-major, bit 0 is the top row, and unsupported
 *  characters resolve to a blank glyph.
 * Ownership: This file owns the immutable font table.
 * 
 */

/**
 * Note: there is probably a more preferred way to implement a bitmap font LUT
 *  like storing it in flash or something but this works 
 */

#include "font_config.hpp"

namespace picoscope::font {

namespace {

// 5x7 column-major font;
// each hex is a no greater than 7-bit binary value where
// bit 0 is the top row of each column.
static constexpr std::uint8_t space_glyph[kGlyphWidth] = {0x00, 0x00, 0x00, 0x00, 0x00};
static constexpr std::uint8_t plus_glyph[kGlyphWidth] = {0x08, 0x08, 0x3E, 0x08, 0x08};
static constexpr std::uint8_t dash_glyph[kGlyphWidth] = {0x08, 0x08, 0x08, 0x08, 0x08};
static constexpr std::uint8_t dot_glyph[kGlyphWidth] = {0x00, 0x60, 0x60, 0x00, 0x00};
static constexpr std::uint8_t slash_glyph[kGlyphWidth] = {0x40, 0x20, 0x10, 0x08, 0x04};
static constexpr std::uint8_t zero_glyph[kGlyphWidth] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
static constexpr std::uint8_t one_glyph[kGlyphWidth] = {0x00, 0x42, 0x7F, 0x40, 0x00};
static constexpr std::uint8_t two_glyph[kGlyphWidth] = {0x42, 0x61, 0x51, 0x49, 0x46};
static constexpr std::uint8_t three_glyph[kGlyphWidth] = {0x21, 0x41, 0x45, 0x4B, 0x31};
static constexpr std::uint8_t four_glyph[kGlyphWidth] = {0x18, 0x14, 0x12, 0x7F, 0x10};
static constexpr std::uint8_t five_glyph[kGlyphWidth] = {0x27, 0x45, 0x45, 0x45, 0x39};
static constexpr std::uint8_t six_glyph[kGlyphWidth] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
static constexpr std::uint8_t seven_glyph[kGlyphWidth] = {0x01, 0x71, 0x09, 0x05, 0x03};
static constexpr std::uint8_t eight_glyph[kGlyphWidth] = {0x36, 0x49, 0x49, 0x49, 0x36};
static constexpr std::uint8_t nine_glyph[kGlyphWidth] = {0x06, 0x49, 0x49, 0x29, 0x1E};
static constexpr std::uint8_t a_glyph[kGlyphWidth] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
static constexpr std::uint8_t c_glyph[kGlyphWidth] = {0x3E, 0x41, 0x41, 0x41, 0x22};
static constexpr std::uint8_t d_glyph[kGlyphWidth] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
static constexpr std::uint8_t e_glyph[kGlyphWidth] = {0x7F, 0x49, 0x49, 0x49, 0x41};
static constexpr std::uint8_t f_glyph[kGlyphWidth] = {0x7F, 0x09, 0x09, 0x09, 0x01};
static constexpr std::uint8_t g_glyph[kGlyphWidth] = {0x3E, 0x41, 0x49, 0x49, 0x7A};
static constexpr std::uint8_t h_glyph[kGlyphWidth] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
static constexpr std::uint8_t i_glyph[kGlyphWidth] = {0x00, 0x41, 0x7F, 0x41, 0x00};
static constexpr std::uint8_t l_glyph[kGlyphWidth] = {0x7F, 0x40, 0x40, 0x40, 0x40};
static constexpr std::uint8_t m_glyph[kGlyphWidth] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
static constexpr std::uint8_t n_glyph[kGlyphWidth] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
static constexpr std::uint8_t o_glyph[kGlyphWidth] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
static constexpr std::uint8_t r_glyph[kGlyphWidth] = {0x7F, 0x09, 0x19, 0x29, 0x46};
static constexpr std::uint8_t s_glyph[kGlyphWidth] = {0x46, 0x49, 0x49, 0x49, 0x31};
static constexpr std::uint8_t t_glyph[kGlyphWidth] = {0x01, 0x01, 0x7F, 0x01, 0x01};
static constexpr std::uint8_t u_glyph[kGlyphWidth] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
static constexpr std::uint8_t v_glyph[kGlyphWidth] = {0x1F, 0x20, 0x40, 0x20, 0x1F};

} // namespace

const std::uint8_t *glyph_for(char c)
{
    switch (c) {
    case ' ': return space_glyph;
    case '+': return plus_glyph;
    case '-': return dash_glyph;
    case '.': return dot_glyph;
    case '/': return slash_glyph;
    case '0': return zero_glyph;
    case '1': return one_glyph;
    case '2': return two_glyph;
    case '3': return three_glyph;
    case '4': return four_glyph;
    case '5': return five_glyph;
    case '6': return six_glyph;
    case '7': return seven_glyph;
    case '8': return eight_glyph;
    case '9': return nine_glyph;
    case 'A': return a_glyph;
    case 'C': return c_glyph;
    case 'D': return d_glyph;
    case 'E': return e_glyph;
    case 'F': return f_glyph;
    case 'G': return g_glyph;
    case 'H': return h_glyph;
    case 'I': return i_glyph;
    case 'L': return l_glyph;
    case 'M': return m_glyph;
    case 'N': return n_glyph;
    case 'O': return o_glyph;
    case 'R': return r_glyph;
    case 'S': return s_glyph;
    case 'T': return t_glyph;
    case 'U': return u_glyph;
    case 'V': return v_glyph;
    case 'm': return m_glyph;
    case 's': return s_glyph;
    case 'u': return u_glyph;
    case 'v': return v_glyph;
    default: return space_glyph;
    }
}

} // namespace picoscope::font
