// Purpose: Declares the RP2040 ADC/DMA sampler used to capture two analog
// channels into alternating DMA buffers or a continuous history ring.
// Interface: Use start()/read_block() for streaming fallback, or
// start_history()/history_snapshot() for pre-trigger/lookback capture.
// Constraints: Samples are configured as CH1/CH2 round-robin FIFO words with
// ADC error bits preserved; no dynamic allocation is used.
// Ownership: AdcSampler owns the DMA channel and sampler storage.

#pragma once

#include <cstdint>

#include "adc_history.hpp"
#include "config.hpp"

namespace picoscope {

// Owns ADC/DMA setup and two alternating sample buffers.
class AdcSampler {
public:
    enum class Mode : std::uint8_t {
        Stopped,
        Streaming,
        History,
    };

    // Takes no inputs, constructs an uninitialized sampler handle, and returns
    // the instance.
    AdcSampler();

    AdcSampler(const AdcSampler &) = delete;
    AdcSampler &operator=(const AdcSampler &) = delete;

    // Takes no inputs, initializes ADC pins/FIFO and claims a DMA channel, and
    // returns nothing.
    void init();

    // Takes the ADC clock divider, arms DMA and starts free-running ADC
    // sampling, and returns nothing. This is the block-streaming fallback mode.
    void start(float adc_clkdiv = 0.0f);

    // Takes the ADC clock divider, arms DMA to continuously write the history
    // ring, starts ADC sampling, and returns nothing.
    void start_history(float adc_clkdiv = 0.0f);

    // Takes no inputs, stops ADC/DMA activity, drains stale FIFO data, and
    // returns nothing.
    void stop();

    // Takes no inputs and returns true when sampling is currently active.
    bool running() const { return running_; }

    // Takes no inputs and returns the currently active sampler mode.
    Mode mode() const { return mode_; }

    // Takes no inputs and returns the ADC clock divider active for the current
    // sampler run.
    float adc_clkdiv() const { return adc_clkdiv_; }

    // Takes no inputs and returns true if the ADC FIFO overflowed since the
    // last sampler alignment reset.
    bool overflowed() const;

    // Takes an output block reference, fills it when a DMA buffer is complete,
    // and returns true only when block points at valid sampler-owned words.
    bool read_block(AdcSampleBlock &block);

    // Takes no inputs, inspects the DMA write head, and returns a stable
    // snapshot of the sampler-owned history ring.
    AdcHistorySnapshot history_snapshot() const;

private:
    // Takes a buffer index, configures DMA to fill the selected sampler-owned
    // buffer from the ADC FIFO, and returns nothing.
    void start_dma(std::uint8_t buffer_index);

    // Takes no inputs, configures DMA to continuously wrap writes inside the
    // sampler-owned history ring, and returns nothing.
    void start_history_dma();

    alignas(config::kAdcHistoryRingBytes) std::uint16_t history_ring_[
        config::kAdcHistoryRingWords] = {};
    std::uint16_t stream_buffers_[2][config::kAdcDmaBlockWords] = {};
    int dma_channel_ = -1;
    std::uint8_t active_buffer_ = 0;
    float adc_clkdiv_ = 0.0f;
    bool initialized_ = false;
    bool running_ = false;
    Mode mode_ = Mode::Stopped;
};

} // namespace picoscope
