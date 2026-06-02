// Purpose: Declares input support for trigger, shift, and scale rotary encoders
// plus active-high channel and horizontal-axis switches.
// Interface: init() configures GPIOs/encoder IRQs, then poll() returns
// switch levels and drained rotary detent deltas as InputEvents.
// Constraints: Encoder A/B inputs use pull-ups and GPIO interrupts; switches use
// pull-down inputs and are sampled by polling.
// Ownership: EncoderManager owns per-encoder decoder and switch state only; GPIO
// hardware remains globally managed by the Pico SDK.

#pragma once

#include <cstdint>

#include "rotary_decoder.hpp"
#include "scope_types.hpp"

namespace picoscope {

// Owns decoder and switch state for the trigger, shift, scale, active-channel,
// and vertical/horizontal controls.
class EncoderManager {
public:
    // Takes no inputs, initializes all encoder/switch GPIOs, IRQs, and decoder
    // state, and returns nothing.
    void init();

    // Takes no inputs, samples switches, drains queued encoder deltas, and
    // returns input events for this poll.
    InputEvents poll();

private:
    struct SwitchState {
        std::uint8_t pin = 0;
    };

    struct EncoderState {
        std::uint8_t pin_a = 0;
        std::uint8_t pin_b = 0;
        QuadratureDecoder decoder = {};
        std::uint32_t last_detent_us = 0;
    };

    // Takes an encoder state plus GPIO pins, configures pull-up inputs and IRQs,
    // seeds decoder state, and returns nothing.
    void init_encoder(EncoderState &encoder,
                      std::uint8_t pin_a,
                      std::uint8_t pin_b);

    // Takes switch storage plus a GPIO pin, configures the switch input, and
    // returns nothing.
    void init_switch(SwitchState &switch_state, std::uint8_t pin);

    // Takes one switch state, samples its GPIO level, and returns true when
    // active.
    bool sample_switch(const SwitchState &switch_state) const;

    // Takes no inputs, atomically drains queued rotary movement into an
    // InputEvents value.
    void drain_pending_deltas(InputEvents &events);

    // Takes a GPIO number from the shared IRQ callback, updates the matching
    // encoder decoder, and queues any resulting movement.
    void handle_gpio_irq(std::uint8_t gpio);

    // Takes an encoder and a per-detent delta, queues it for the next poll, and
    // returns nothing.
    void queue_encoder_delta(EncoderState &encoder, std::int8_t delta);

    // Takes a GPIO number, finds the encoder that owns it, and returns that
    // encoder state or nullptr.
    EncoderState *encoder_for_gpio(std::uint8_t gpio);

    // Takes a GPIO IRQ from the Pico SDK, forwards it to the active manager,
    // and returns nothing.
    static void gpio_callback(unsigned int gpio, std::uint32_t events);

    EncoderState trigger_ = {};
    EncoderState shift_ = {};
    EncoderState scale_ = {};
    SwitchState channel_switch_ = {};
    SwitchState horizontal_switch_ = {};

    volatile std::int32_t pending_trigger_delta_ = 0;
    volatile std::int32_t pending_shift_delta_ = 0;
    volatile std::int32_t pending_scale_delta_ = 0;

    static EncoderManager *active_instance_;
};

} // namespace picoscope
