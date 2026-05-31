// Purpose: Declares input support for trigger, vertical, and horizontal rotary
// encoders plus two active-high buttons.
// Interface: init() configures GPIOs/encoder IRQs, then poll() returns
// debounced button presses and drained rotary deltas as InputEvents.
// Constraints: Encoder A/B inputs use pull-ups and GPIO interrupts; buttons use
// the configured pressed level and are time-debounced by polling.
// Ownership: EncoderManager owns per-encoder decoder and button state only; GPIO
// hardware remains globally managed by the Pico SDK.

#pragma once

#include <cstdint>

#include "rotary_decoder.hpp"
#include "scope_types.hpp"

namespace picoscope {

// Owns decoder and button state for the trigger, vertical-axis, horizontal-axis,
// active-channel, and shift/scale controls.
class EncoderManager {
public:
    // Takes no inputs, initializes all encoder/button GPIOs, IRQs, and decoder
    // state, and returns nothing.
    void init();

    // Takes no inputs, samples buttons, drains queued encoder deltas, and
    // returns input events for this poll.
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
        std::uint32_t last_interrupt_us = 0;
    };

    // Takes an encoder state plus GPIO pins, configures pull-up inputs and IRQs,
    // seeds decoder state, and returns nothing.
    void init_encoder(EncoderState &encoder,
                      std::uint8_t pin_a,
                      std::uint8_t pin_b);

    // Takes button storage plus a GPIO pin, configures the button input, seeds
    // debounce state, and returns nothing.
    void init_button(ButtonState &button, std::uint8_t pin);

    // Takes one button state, updates debounce state from GPIO level, and
    // returns true only for a newly stable press.
    bool poll_button_pressed(ButtonState &button);

    // Takes no inputs, atomically drains queued rotary movement into an
    // InputEvents value.
    void drain_pending_deltas(InputEvents &events);

    // Takes a GPIO number from the shared IRQ callback, updates the matching
    // encoder decoder, and queues any resulting movement.
    void handle_gpio_irq(std::uint8_t gpio);

    // Takes an encoder and a per-transition delta, queues it for the next poll,
    // and returns nothing.
    void queue_encoder_delta(EncoderState &encoder, std::int8_t delta);

    // Takes a GPIO number, finds the encoder that owns it, and returns that
    // encoder state or nullptr.
    EncoderState *encoder_for_gpio(std::uint8_t gpio);

    // Takes a GPIO IRQ from the Pico SDK, forwards it to the active manager,
    // and returns nothing.
    static void gpio_callback(unsigned int gpio, std::uint32_t events);

    EncoderState trigger_ = {};
    EncoderState vertical_ = {};
    EncoderState horizontal_ = {};
    ButtonState channel_button_ = {};
    ButtonState shift_scale_button_ = {};

    volatile std::int32_t pending_trigger_delta_ = 0;
    volatile std::int32_t pending_vertical_delta_ = 0;
    volatile std::int32_t pending_horizontal_delta_ = 0;

    static EncoderManager *active_instance_;
};

} // namespace picoscope
