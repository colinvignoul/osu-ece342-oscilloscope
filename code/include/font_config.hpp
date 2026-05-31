// Purpose: Declares the fixed bitmap font used by the strip renderer.
// Interface: Call glyph_for() to resolve a character into 5x7 column data.
// Constraints: Unsupported characters resolve to a blank glyph.
// Ownership: Font data is immutable and owned by the implementation file.

#pragma once

#include <cstdint>

namespace picoscope::font {

constexpr int kGlyphWidth = 5;
constexpr int kGlyphHeight = 7;

// Takes a character, resolves the built-in 5x7 glyph columns, and returns a
// pointer to static glyph data or the blank glyph.
const std::uint8_t *glyph_for(char c);

} // namespace picoscope::font
