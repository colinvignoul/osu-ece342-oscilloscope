// Purpose: Implements IRQ-driven GPIO sampling for rotary encoders and
// polling for active-high switches.
// Interface: EncoderManager produces InputEvents from three configured
// encoders plus two configured switches.
// Constraints: Encoder GPIOs are pull-up inputs, switch active level comes
// from config, switches are sampled as levels, and queued encoder deltas are
// drained atomically from poll().
// Ownership: EncoderManager owns decoder and switch state; GPIO hardware remains
// managed through Pico SDK calls.

#include "encoder_manager.hpp"

#include "config.hpp"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/time.h"

#include <limits>

namespace picoscope {
namespace {

// Takes a GPIO pin number, reads its digital level, and returns true for high.
bool read_pin(std::uint8_t pin)
{
    return gpio_get(pin) != 0;
}

// Takes a GPIO pin number, compares it with the configured active level, and
// returns true when active.
bool read_switch_active(std::uint8_t pin)
{
    return read_pin(pin) == config::kSwitchActiveLevel;
}

// Takes a signed 32-bit delta, clamps it to the InputEvents field width, and
// returns a safe 16-bit value.
std::int16_t clamp_delta(std::int32_t delta)
{
    if (delta > std::numeric_limits<std::int16_t>::max()) {
        return std::numeric_limits<std::int16_t>::max();
    }
    if (delta < std::numeric_limits<std::int16_t>::min()) {
        return std::numeric_limits<std::int16_t>::min();
    }
    return static_cast<std::int16_t>(delta);
}

} // namespace

EncoderManager *EncoderManager::active_instance_ = nullptr;

// Takes no inputs, configures all encoder and switch GPIOs, seeds state, and
// returns nothing.
void EncoderManager::init()
{
    active_instance_ = this;

    init_encoder(shift_, config::kShiftEncA, config::kShiftEncB);
    init_encoder(scale_, config::kScaleEncA, config::kScaleEncB);
    init_encoder(trigger_, config::kTriggerEncA, config::kTriggerEncB);
    init_switch(channel_switch_, config::kChannelSwitch);
    init_switch(horizontal_switch_, config::kHorizontalSwitch);
}

// Takes no inputs, polls switches, drains queued encoder state, and returns
// the resulting input events.
InputEvents EncoderManager::poll()
{
    InputEvents events = {};
    drain_pending_deltas(events);
    events.channel_switch_active = sample_switch(channel_switch_);
    events.horizontal_switch_active = sample_switch(horizontal_switch_);
    return events;
}

// Takes encoder storage plus A/B pins, configures pull-up GPIO inputs and IRQs,
// seeds decoder state, and returns nothing.
void EncoderManager::init_encoder(EncoderState &encoder,
                                  std::uint8_t pin_a,
                                  std::uint8_t pin_b)
{
    encoder.pin_a = pin_a;
    encoder.pin_b = pin_b;
    encoder.last_interrupt_us = 0;

    gpio_init(pin_a);
    gpio_set_dir(pin_a, GPIO_IN);
    gpio_pull_up(pin_a);

    gpio_init(pin_b);
    gpio_set_dir(pin_b, GPIO_IN);
    gpio_pull_up(pin_b);

    encoder.decoder.reset(read_pin(pin_a), read_pin(pin_b));

    constexpr std::uint32_t kEncoderIrqMask =
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL;
    gpio_set_irq_enabled_with_callback(pin_a, kEncoderIrqMask, true,
                                       &EncoderManager::gpio_callback);
    gpio_set_irq_enabled_with_callback(pin_b, kEncoderIrqMask, true,
                                       &EncoderManager::gpio_callback);
}

// Takes switch storage plus a GPIO pin and configures the switch input
void EncoderManager::init_switch(SwitchState &switch_state, std::uint8_t pin)
{
    switch_state.pin = pin;

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    if (config::kSwitchActiveLevel) {
        gpio_pull_down(pin);
    } else {
        gpio_pull_up(pin);
    }
}

// Takes one switch state, samples its GPIO level, and returns true when active.
bool EncoderManager::sample_switch(const SwitchState &switch_state) const
{
    return read_switch_active(switch_state.pin);
}

// Takes no inputs, atomically drains queued rotary movement into an InputEvents
// value and clears the pending counters.
void EncoderManager::drain_pending_deltas(InputEvents &events)
{
    const std::uint32_t interrupts = save_and_disable_interrupts();

    events.trigger_delta = clamp_delta(pending_trigger_delta_);
    events.shift_delta = clamp_delta(pending_shift_delta_);
    events.scale_delta = clamp_delta(pending_scale_delta_);

    pending_trigger_delta_ = 0;
    pending_shift_delta_ = 0;
    pending_scale_delta_ = 0;

    restore_interrupts(interrupts);
}

// Takes a GPIO number from the shared IRQ callback, updates the matching
// encoder decoder, and queues any resulting movement.
void EncoderManager::handle_gpio_irq(std::uint8_t gpio)
{
    EncoderState *encoder = encoder_for_gpio(gpio);
    if (encoder == nullptr) {
        return;
    }

    const std::uint32_t now = time_us_32();
    if ((now - encoder->last_interrupt_us) < config::kEncoderIrqDebounceUs) {
        return;
    }
    encoder->last_interrupt_us = now;

    const std::int8_t delta =
        encoder->decoder.update(read_pin(encoder->pin_a), read_pin(encoder->pin_b));
    if (delta != 0) {
        queue_encoder_delta(*encoder, delta);
    }
}

// Takes an encoder and a per-transition delta, queues it for the next poll, and
// returns nothing.
void EncoderManager::queue_encoder_delta(EncoderState &encoder, std::int8_t delta)
{
    if (&encoder == &trigger_) {
        pending_trigger_delta_ += delta;
        return;
    }

    if (&encoder == &shift_) {
        pending_shift_delta_ += delta;
        return;
    }

    if (&encoder == &scale_) {
        pending_scale_delta_ += delta;
    }
}

// Takes a GPIO number, finds the encoder that owns it, and returns that encoder
// state or nullptr.
EncoderManager::EncoderState *EncoderManager::encoder_for_gpio(std::uint8_t gpio)
{
    if (gpio == trigger_.pin_a || gpio == trigger_.pin_b) {
        return &trigger_;
    }
    if (gpio == shift_.pin_a || gpio == shift_.pin_b) {
        return &shift_;
    }
    if (gpio == scale_.pin_a || gpio == scale_.pin_b) {
        return &scale_;
    }
    return nullptr;
}

// Takes a GPIO IRQ from the Pico SDK, forwards it to the active manager, and
// returns nothing.
void EncoderManager::gpio_callback(unsigned int gpio, std::uint32_t events)
{
    (void)events;
    if (active_instance_ == nullptr) {
        return;
    }
    active_instance_->handle_gpio_irq(static_cast<std::uint8_t>(gpio));
}

} // namespace picoscope
