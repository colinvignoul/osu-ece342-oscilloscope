// Purpose: Implements the cross-core render mailbox and display core loop.
// Interface: Core 0 publishes frame/settings snapshots; core 1 claims the newest
// snapshot and renders it through StripRenderer and DisplayDma.
// Constraints: Publishing never waits for display DMA, stale queued frames are
// dropped, and render slot state changes are protected by a Pico spin lock.
// Ownership: RenderPipeline owns all display hardware objects and render slots.

#include "render_pipeline.hpp"

#include "strip_renderer.hpp"

#include "pico/multicore.h"
#include "pico/stdlib.h"

namespace picoscope {

RenderPipeline *RenderPipeline::active_pipeline_ = nullptr;

// Takes no inputs, constructs a render pipeline with display DMA bound to the
// pipeline-owned ILI9341 command driver.
RenderPipeline::RenderPipeline()
    : display_dma_(display_lcd_)
{
}

// Takes no inputs, enters the render mailbox lock, and returns the saved IRQ
// state needed when unlocking.
std::uint32_t RenderPipeline::lock_mailbox()
{
    return spin_lock_blocking(render_lock_);
}

// Takes a saved IRQ state from lock_mailbox(), releases the render mailbox lock,
// and returns nothing.
void RenderPipeline::unlock_mailbox(std::uint32_t irq_state)
{
    spin_unlock(render_lock_, irq_state);
}

// Takes no inputs, marks stale queued frames free, and returns a writable slot
// index or -1 if every slot is currently owned.
int RenderPipeline::reserve_slot()
{
    int free_slot = -1;
    const std::uint32_t irq_state = lock_mailbox();
    for (std::uint8_t slot = 0; slot < kRenderSlotCount; ++slot) {
        if (render_slot_states_[slot] == RenderSlotState::Queued) {
            render_slot_states_[slot] = RenderSlotState::Free;
        }
        if (free_slot < 0 && render_slot_states_[slot] == RenderSlotState::Free) {
            free_slot = slot;
        }
    }
    if (free_slot >= 0) {
        render_slot_states_[free_slot] = RenderSlotState::Writing;
    }
    unlock_mailbox(irq_state);
    return free_slot;
}

// Takes a frame/settings snapshot, publishes it for core 1 without blocking, and
// returns true when a render slot was queued.
bool RenderPipeline::publish(const ScopeFrame &frame, const ScopeSettings &settings)
{
    const int slot = reserve_slot();
    if (slot < 0) {
        return false;
    }

    render_slots_[slot].frame = frame;
    render_slots_[slot].settings = settings;

    const std::uint32_t irq_state = lock_mailbox();
    render_slots_[slot].sequence = ++publish_sequence_;
    render_slot_states_[slot] = RenderSlotState::Queued;
    unlock_mailbox(irq_state);

    __sev();
    return true;
}

// Takes no inputs, claims the newest queued frame for rendering, frees stale
// queued frames, and returns the slot index or -1 when none are queued.
int RenderPipeline::claim_next_slot()
{
    int selected_slot = -1;
    std::uint32_t newest_sequence = 0;
    const std::uint32_t irq_state = lock_mailbox();
    for (std::uint8_t slot = 0; slot < kRenderSlotCount; ++slot) {
        if (render_slot_states_[slot] != RenderSlotState::Queued) {
            continue;
        }
        if (selected_slot < 0 || render_slots_[slot].sequence > newest_sequence) {
            selected_slot = slot;
            newest_sequence = render_slots_[slot].sequence;
        }
    }

    for (std::uint8_t slot = 0; slot < kRenderSlotCount; ++slot) {
        if (render_slot_states_[slot] != RenderSlotState::Queued) {
            continue;
        }
        if (static_cast<int>(slot) == selected_slot) {
            render_slot_states_[slot] = RenderSlotState::Rendering;
        } else {
            render_slot_states_[slot] = RenderSlotState::Free;
        }
    }
    unlock_mailbox(irq_state);
    return selected_slot;
}

// Takes a render slot index, marks it reusable after core 1 finishes drawing, and
// returns nothing.
void RenderPipeline::release_slot(int slot)
{
    if (slot < 0 || slot >= kRenderSlotCount) {
        return;
    }

    const std::uint32_t irq_state = lock_mailbox();
    render_slot_states_[slot] = RenderSlotState::Free;
    unlock_mailbox(irq_state);
}

// Takes no inputs, runs forever on core 1, and owns all display/SPI rendering.
void RenderPipeline::render_forever()
{
    display_lcd_.init();
    display_dma_.init();

    for (;;) {
        const int slot = claim_next_slot();
        if (slot < 0) {
            __wfe();
            continue;
        }

        const StripRenderer renderer(render_slots_[slot].frame,
                                     render_slots_[slot].settings);
        display_dma_.render_frame(renderer);
        release_slot(slot);
    }
}

// Takes no inputs, forwards the Pico core-1 entry callback to active_pipeline_.
void RenderPipeline::core1_entry()
{
    if (active_pipeline_ != nullptr) {
        active_pipeline_->render_forever();
    }
}

// Takes no inputs, initializes the render mailbox and launches core 1.
void RenderPipeline::init()
{
    active_pipeline_ = this;
    render_lock_ = spin_lock_init(spin_lock_claim_unused(true));
    for (std::uint8_t slot = 0; slot < kRenderSlotCount; ++slot) {
        render_slot_states_[slot] = RenderSlotState::Free;
    }
    multicore_launch_core1(&RenderPipeline::core1_entry);
}

} // namespace picoscope
