// Purpose: Declares input support for three rotary encoders and two standalone
// push buttons.
// Interface: init() configures GPIOs/encoder IRQs, then poll() returns
// debounced button presses and drained one-detent rotary deltas as InputEvents.
// Constraints: Encoder A/B inputs use pull-ups and GPIO interrupts; standalone
// buttons use the configured pressed level and are time-debounced by polling.
// Ownership: EncoderManager owns per-encoder decoder/debounce state only; GPIO
// hardware remains globally managed by the Pico SDK.

#pragma once

#include <cstdint>

#include "rotary_decoder.hpp"
#include "scope_types.hpp"

namespace picoscope {

// Owns decoder/debounce state for the trigger, voltage, time, and standalone
// button controls.
class EncoderManager {
public:
    // Takes no inputs, initializes all encoder/button GPIOs, IRQs, and decoder
    // state, and returns nothing.
    void init();

    // Takes no inputs, samples all encoders/buttons once, and returns edge-like
    // input events for this poll.
    InputEvents poll();

private:
    struct ButtonState {
        std::uint8_t pin = 0;
        bool raw_pressed = false;
        bool stable_pressed = false;
        std::uint64_t last_change_us = 0;
    };

    struct EncoderState {
        std::uint8_t pin_a = 0;
        std::uint8_t pin_b = 0;
        QuadratureDecoder decoder = {};
        volatile std::int32_t pending_delta = 0;
        std::uint32_t last_interrupt_us = 0;
    };

    // Takes an encoder state plus GPIO pins, configures pull-up inputs and IRQs,
    // seeds decoder state, and returns nothing.
    void init_encoder(EncoderState &encoder,
                      std::uint8_t pin_a,
                      std::uint8_t pin_b);

    // Takes button storage plus a GPIO pin, configures the standalone button
    // input, seeds debounce state, and returns nothing.
    void init_button(ButtonState &button, std::uint8_t pin);

    // Takes one encoder state, atomically drains queued detents since the
    // previous poll, and returns the signed delta.
    std::int16_t drain_encoder_delta(EncoderState &encoder);

    // Takes one button state, updates debounce state from GPIO level, and
    // returns true only for a newly stable press.
    bool poll_button_pressed(ButtonState &button);

    // Takes a GPIO number from the shared IRQ callback, updates the matching
    // encoder decoder, and queues any resulting detent delta.
    void handle_encoder_irq(std::uint8_t gpio);

    // Takes a GPIO number, finds the encoder that owns it, and returns that
    // encoder state or nullptr.
    EncoderState *encoder_for_gpio(std::uint8_t gpio);

    // Takes a GPIO IRQ from the Pico SDK, forwards it to the active manager,
    // and returns nothing.
    static void gpio_callback(unsigned int gpio, std::uint32_t events);

    EncoderState trigger_ = {};
    EncoderState voltage_ = {};
    EncoderState time_ = {};
    ButtonState channel_button_ = {};
    ButtonState run_button_ = {};

    static EncoderManager *active_instance_;
};

} // namespace picoscope
