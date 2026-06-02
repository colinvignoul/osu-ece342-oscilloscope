// Purpose: Declares a small quadrature decoder for mechanical rotary encoders.
// Interface: reset() seeds the current AB rest state, then update() consumes
// sampled AB states and returns per-detent movement.
// Constraints: Invalid two-bit transitions are ignored.
// Ownership: QuadratureDecoder owns only its previous-state field; GPIO
// sampling is supplied by callers.

#pragma once

#include <cstdint>

namespace picoscope {

// Holds the previous AB state for one encoder.
class QuadratureDecoder {
public:
    // Takes current A/B levels, seeds decoder state without movement, and
    // returns nothing.
    void reset(bool a, bool b);

    // Takes current A/B levels and returns -1, 0, or 1 when a full detent
    // completes.
    std::int8_t update(bool a, bool b);

private:
    std::uint8_t previous_state_ = 0;
    std::uint8_t rest_state_ = 0;
    std::int8_t partial_delta_ = 0;
};

} // namespace picoscope
