// Purpose: Implements a table-driven quadrature decoder for mechanical rotary
// encoder AB signals.
// Interface: Call reset() with initial pin levels, then update() with sampled
// levels to receive per-detent movement.
// Constraints: The transition table ignores invalid two-bit jumps, and movement
// emits only after a complete four-edge cycle returns to the rest state.
// Ownership: The decoder owns no GPIO resources; callers supply sampled states.

#include "rotary_decoder.hpp"

namespace picoscope {
namespace {

// Index is previous AB state in the high bits and current AB state in the low
// bits. Invalid two-bit jumps contribute 0, which filters bounce/skipped steps.
constexpr std::int8_t kTransitionTable[16] = {
     0,  1, -1,  0,
    -1,  0,  0,  1,
     1,  0,  0, -1,
     0, -1,  1,  0
};
constexpr std::int8_t kTransitionsPerDetent = 4;

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
    rest_state_ = previous_state_;
    partial_delta_ = 0;
}

// Takes current A/B levels and returns -1, 0, or 1 when a full detent
// completes.
std::int8_t QuadratureDecoder::update(bool a, bool b)
{
    const std::uint8_t state = encode_state(a, b);
    if (state == previous_state_) {
        return 0;
    }

    const std::uint8_t transition =
        static_cast<std::uint8_t>((previous_state_ << 2u) | state);
    previous_state_ = state;

    const std::int8_t delta = kTransitionTable[transition];
    if (delta == 0) {
        partial_delta_ = 0;
        return 0;
    }

    partial_delta_ = static_cast<std::int8_t>(partial_delta_ + delta);
    if (state != rest_state_) {
        return 0;
    }

    const std::int8_t completed_delta = partial_delta_;
    partial_delta_ = 0;
    if (completed_delta == kTransitionsPerDetent) {
        return 1;
    }
    if (completed_delta == -kTransitionsPerDetent) {
        return -1;
    }
    return 0;
}

} // namespace picoscope
