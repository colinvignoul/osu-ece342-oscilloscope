// Purpose: Declares the cross-core render pipeline that publishes scope frames
// from core 0 and renders them on core 1.
// Interface: init() starts the display core; publish() queues the newest
// frame/settings snapshot without blocking the acquisition loop.
// Constraints: Uses fixed render slots, a Pico spin lock, and latest-frame-wins
// mailbox semantics.
// Ownership: RenderPipeline owns display hardware drivers, render slots, and the
// core-1 render loop state.

#pragma once

#include <cstdint>

#include "display_dma.hpp"
#include "ili9341.hpp"
#include "scope_types.hpp"

#include "hardware/sync.h"

namespace picoscope {

class RenderPipeline {
public:
    RenderPipeline();

    RenderPipeline(const RenderPipeline &) = delete;
    RenderPipeline &operator=(const RenderPipeline &) = delete;

    // Takes no inputs, initializes the render mailbox and launches core 1.
    void init();

    // Takes a frame/settings snapshot, publishes it for core 1 without blocking,
    // and returns true when a render slot was queued.
    bool publish(const ScopeFrame &frame, const ScopeSettings &settings);

private:
    static constexpr std::uint8_t kRenderSlotCount = 3;

    enum class RenderSlotState : std::uint8_t {
        Free,
        Writing,
        Queued,
        Rendering,
    };

    struct PublishedFrame {
        ScopeFrame frame = {};
        ScopeSettings settings = {};
        std::uint32_t sequence = 0;
    };

    // Takes no inputs, enters the render mailbox lock, and returns the saved IRQ
    // state needed when unlocking.
    std::uint32_t lock_mailbox();

    // Takes a saved IRQ state from lock_mailbox(), releases the render mailbox
    // lock, and returns nothing.
    void unlock_mailbox(std::uint32_t irq_state);

    // Takes no inputs, marks stale queued frames free, and returns a writable
    // slot index or -1 if every slot is currently owned.
    int reserve_slot();

    // Takes no inputs, claims the newest queued frame for rendering, frees stale
    // queued frames, and returns the slot index or -1 when none are queued.
    int claim_next_slot();

    // Takes a render slot index, marks it reusable after core 1 finishes drawing,
    // and returns nothing.
    void release_slot(int slot);

    // Takes no inputs, runs forever on core 1, and owns all display/SPI rendering.
    void render_forever();

    // Takes no inputs, forwards the Pico core-1 entry callback to active_pipeline_.
    static void core1_entry();

    Ili9341 display_lcd_ = {};
    DisplayDma display_dma_;
    PublishedFrame render_slots_[kRenderSlotCount] = {};
    RenderSlotState render_slot_states_[kRenderSlotCount] = {};
    spin_lock_t *render_lock_ = nullptr;
    std::uint32_t publish_sequence_ = 0;

    static RenderPipeline *active_pipeline_;
};

} // namespace picoscope
