// Purpose: Implements RP2040 ADC round-robin sampling into double-buffered DMA
// blocks or one continuous hardware-wrapped history ring.
// Interface: Streaming callers poll read_block(); history callers inspect
// snapshots and build wrapped ranges from the sampler-owned ring.
// Constraints: The ADC FIFO emits CH1/CH2 16-bit words with error bits; DMA is
// paced by ADC DREQ and only one DMA channel is claimed.
// Ownership: AdcSampler owns sampler storage and the DMA channel lifetime.

#include "adc_sampler.hpp"

#include "hardware/adc.h"
#include "hardware/dma.h"
#include "pico/stdlib.h"

namespace picoscope {
namespace {

// Takes no inputs, resets ADC FIFO/channel alignment for CH1/CH2 round-robin
// sampling, and returns nothing.
void reset_adc_alignment()
{
    adc_run(false);
    adc_fifo_drain();
    adc_hw->fcs |= ADC_FCS_OVER_BITS | ADC_FCS_UNDER_BITS;
    adc_select_input(config::kAdcInputCh1);
    adc_set_round_robin((1u << config::kAdcInputCh1) | (1u << config::kAdcInputCh2));
}

} // namespace

// Takes no inputs, constructs an uninitialized sampler object, and returns the
// instance.
AdcSampler::AdcSampler() = default;

// Takes no inputs, configures ADC pins/FIFO and claims DMA if needed, and
// returns nothing.
void AdcSampler::init()
{
    if (initialized_) {
        return;
    }

    adc_init();
    adc_gpio_init(config::kAdcPinCh1);
    adc_gpio_init(config::kAdcPinCh2);
    adc_select_input(config::kAdcInputCh1);
    // Free-running round-robin yields alternating CH1/CH2 FIFO words.
    adc_set_round_robin((1u << config::kAdcInputCh1) | (1u << config::kAdcInputCh2));
    adc_set_clkdiv(0.0f);
    // Retain the ERR bit in each 16-bit FIFO word so acquisition can drop
    // failed conversions instead of treating them as large samples.
    adc_fifo_setup(true, true, 1, true, false);
    adc_fifo_drain();

    dma_channel_ = dma_claim_unused_channel(true);
    initialized_ = true;
}

// Takes the ADC clock divider, resets FIFO/channel alignment, arms DMA, starts
// the ADC, and returns nothing.
void AdcSampler::start(float adc_clkdiv)
{
    if (!initialized_) {
        init();
    }
    if (running_ && mode_ == Mode::Streaming && adc_clkdiv_ == adc_clkdiv) {
        return;
    }
    if (running_) {
        stop();
    }

    reset_adc_alignment();
    adc_set_clkdiv(adc_clkdiv);
    adc_clkdiv_ = adc_clkdiv;
    active_buffer_ = 0;
    start_dma(active_buffer_);
    adc_run(true);
    running_ = true;
    mode_ = Mode::Streaming;
}

// Takes the ADC clock divider, resets FIFO/channel alignment, arms the
// hardware-wrapped history ring, starts the ADC, and returns nothing.
void AdcSampler::start_history(float adc_clkdiv)
{
    if (!initialized_) {
        init();
    }
    if (running_ && mode_ == Mode::History && adc_clkdiv_ == adc_clkdiv) {
        return;
    }
    if (running_) {
        stop();
    }

    reset_adc_alignment();
    adc_set_clkdiv(adc_clkdiv);
    adc_clkdiv_ = adc_clkdiv;
    start_history_dma();
    adc_run(true);
    running_ = true;
    mode_ = Mode::History;
}

// Takes no inputs, stops ADC/DMA activity, drains stale FIFO data, and returns
// nothing.
void AdcSampler::stop()
{
    if (!running_) {
        return;
    }

    adc_run(false);
    dma_channel_abort(dma_channel_);
    adc_fifo_drain();
    running_ = false;
    mode_ = Mode::Stopped;
}

// Takes no inputs and returns true if the ADC FIFO overflow flag is set.
bool AdcSampler::overflowed() const
{
    return (adc_hw->fcs & ADC_FCS_OVER_BITS) != 0u;
}

// Takes an output block reference, publishes a completed DMA buffer when ready,
// and returns true only when block contains valid words/count.
bool AdcSampler::read_block(AdcSampleBlock &block)
{
    block = {};
    if (!running_ || mode_ != Mode::Streaming || dma_channel_is_busy(dma_channel_)) {
        return false;
    }

    // Return the just-filled buffer while immediately arming DMA on the other
    // buffer, giving the caller a stable block until that buffer is reused.
    dma_channel_wait_for_finish_blocking(dma_channel_);
    const std::uint8_t completed_buffer = active_buffer_;
    active_buffer_ ^= 1u;
    start_dma(active_buffer_);

    block.words = stream_buffers_[completed_buffer];
    block.word_count = config::kAdcDmaBlockWords;
    block.overflowed = overflowed();
    return true;
}

// Takes no inputs, derives a stable history window from the DMA down-counter,
// and returns the current sampler-owned ring snapshot.
AdcHistorySnapshot AdcSampler::history_snapshot() const
{
    AdcHistorySnapshot snapshot = {};
    snapshot.ring_words = history_ring_;
    snapshot.ring_word_count = config::kAdcHistoryRingWords;
    snapshot.ring_pair_count = config::kAdcHistoryRingPairs;
    snapshot.guard_pairs = config::kAdcHistoryGuardPairs;
    snapshot.overflowed = overflowed();

    if (!running_ || mode_ != Mode::History || dma_channel_ < 0) {
        return snapshot;
    }

    const std::uint32_t remaining = dma_hw->ch[dma_channel_].transfer_count;
    const std::uint64_t words_written =
        static_cast<std::uint64_t>(config::kAdcHistoryTransferWords - remaining);
    const std::uint64_t complete_pairs = words_written / 2u;
    const std::uint64_t stable_pairs =
        complete_pairs > config::kAdcHistoryGuardPairs
            ? complete_pairs - config::kAdcHistoryGuardPairs
            : 0u;
    if (stable_pairs == 0u) {
        return snapshot;
    }

    const std::uint64_t capacity_pairs =
        config::kAdcHistoryRingPairs > config::kAdcHistoryGuardPairs
            ? config::kAdcHistoryRingPairs - config::kAdcHistoryGuardPairs
            : 0u;
    snapshot.has_complete_pair = true;
    snapshot.latest_complete_pair_sequence = stable_pairs - 1u;
    snapshot.oldest_available_pair_sequence =
        stable_pairs > capacity_pairs ? stable_pairs - capacity_pairs : 0u;
    return snapshot;
}

// Takes a buffer index, configures the DMA channel to copy ADC FIFO words into
// that buffer, and returns nothing.
void AdcSampler::start_dma(std::uint8_t buffer_index)
{
    dma_channel_config config = dma_channel_get_default_config(dma_channel_);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_high_priority(&config, true);
    // ADC DREQ paces one FIFO read per conversion.
    channel_config_set_dreq(&config, DREQ_ADC);

    dma_channel_configure(dma_channel_,
                          &config,
                          stream_buffers_[buffer_index & 1u],
                          &adc_hw->fifo,
                          config::kAdcDmaBlockWords,
                          true);
}

// Takes no inputs, configures DMA to wrap writes within the 32KB history ring,
// and returns nothing.
void AdcSampler::start_history_dma()
{
    dma_channel_config dma_config = dma_channel_get_default_config(dma_channel_);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_config, false);
    channel_config_set_write_increment(&dma_config, true);
    channel_config_set_high_priority(&dma_config, true);
    channel_config_set_dreq(&dma_config, DREQ_ADC);
    channel_config_set_ring(&dma_config, true, config::kAdcHistoryRingBits);

    dma_channel_configure(dma_channel_,
                          &dma_config,
                          history_ring_,
                          &adc_hw->fifo,
                          config::kAdcHistoryTransferWords,
                          true);
}

} // namespace picoscope
