// Purpose: Implements a table-driven quadrature decoder for mechanical rotary
// encoder AB signals.
// Interface: Call reset() with initial pin levels, then update() with sampled
// levels to receive per-transition movement.
// Constraints: The transition table ignores invalid two-bit jumps.
// Ownership: The decoder owns no GPIO resources; callers supply sampled states.

#include "rotary_decoder.hpp"

namespace picoscope {
namespace {

// Index is previous AB state in the high bits and current AB state in the low
// bits. Invalid two-bit jumps contribute 0, which filters bounce/skipped steps.
constexpr std::int8_t kTransitionTable[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0,
};

// Takes raw A/B pin levels, packs them into a two-bit state, and returns that
// encoded state.
std::uint8_t encode_state(bool a, bool b)
{
    return static_cast<std::uint8_t>((a ? 2u : 0u) | (b ? 1u : 0u));
}

} // namespace

// Takes current A/B levels and seeds previous state
void QuadratureDecoder::reset(bool a, bool b)
{
    previous_state_ = encode_state(a, b);
}

// Takes current A/B levels and returns -1, 0, or 1 for this transition.
std::int8_t QuadratureDecoder::update(bool a, bool b)
{
    const std::uint8_t state = encode_state(a, b);
    const std::uint8_t transition =
        static_cast<std::uint8_t>((previous_state_ << 2u) | state);
    previous_state_ = state;

    return kTransitionTable[transition];
}

} // namespace picoscope
