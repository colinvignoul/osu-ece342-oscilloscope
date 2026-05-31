// Purpose: Implements core-0 sampling/acquisition orchestration.
// Interface: The main loop asks process() for publishable frames and restarts the
// pipeline when settings require capture state to reset.
// Constraints: Chooses history mode when one raw frame fits in the ADC history
// ring, falls back to streaming otherwise, and realigns ADC channel order after
// FIFO overflow.
// Ownership: CapturePipeline mutates borrowed sampler/history/acquisition
// objects but does not own their storage.

#include "capture_pipeline.hpp"

#include "pico/stdlib.h"
#include "scope_control.hpp"

namespace picoscope {

// Takes long-lived sampler/acquisition objects by reference and stores them for
// the firmware lifetime.
CapturePipeline::CapturePipeline(AdcSampler &sampler,
                                 HistoryAcquisition &history_acquisition,
                                 AcquisitionEngine &acquisition)
    : sampler_(sampler)
    , history_acquisition_(history_acquisition)
    , acquisition_(acquisition)
{
}

// Takes initial settings, initializes sampler/acquisition state, and returns
// nothing.
void CapturePipeline::init(const ScopeSettings &settings)
{
    acquisition_.reset(settings);
    sampler_.init();
    ensure_sampler_mode(settings);
}

// Takes changed settings, clears capture/history state, restarts sampling when
// running, and returns nothing.
void CapturePipeline::restart_for_settings(const ScopeSettings &settings)
{
    sampler_.stop();
    history_acquisition_.reset();
    acquisition_.reset(settings, false);
    ensure_sampler_mode(settings);
}

// Takes current settings, switches to the required ADC mode if needed, and
// returns nothing.
void CapturePipeline::ensure_sampler_mode(const ScopeSettings &settings)
{
    if (!settings.running) {
        return;
    }

    const AdcSampler::Mode desired_mode =
        history_acquisition_.supported(settings) ? AdcSampler::Mode::History
                                                 : AdcSampler::Mode::Streaming;
    const float adc_clkdiv = timebase_adc_clkdiv(settings);
    if (sampler_.running() && sampler_.mode() == desired_mode &&
        sampler_.adc_clkdiv() == adc_clkdiv) {
        return;
    }

    sampler_.stop();
    history_acquisition_.reset();
    if (desired_mode == AdcSampler::Mode::History) {
        sampler_.start_history(adc_clkdiv);
    } else {
        sampler_.start(adc_clkdiv);
    }
}

// Takes current settings after an ADC overflow, realigns sampling/acquisition,
// and returns nothing.
void CapturePipeline::recover_overflow(const ScopeSettings &settings)
{
    sampler_.stop();
    history_acquisition_.reset();
    acquisition_.reset(settings, false);
    ensure_sampler_mode(settings);
}

// Takes current settings, processes available ADC data, and returns true when a
// frame is ready to publish.
bool CapturePipeline::process(const ScopeSettings &settings)
{
    if (!settings.running) {
        if (sampler_.running()) {
            sampler_.stop();
        }
        tight_loop_contents();
        return false;
    }

    ensure_sampler_mode(settings);

    if (sampler_.mode() == AdcSampler::Mode::History) {
        const AdcHistorySnapshot snapshot = sampler_.history_snapshot();
        if (snapshot.overflowed) {
            recover_overflow(settings);
            tight_loop_contents();
            return false;
        }
        if (history_acquisition_.process(snapshot, acquisition_, settings)) {
            return true;
        }
        tight_loop_contents();
        return false;
    }

    if (sampler_.overflowed()) {
        recover_overflow(settings);
        tight_loop_contents();
        return false;
    }

    AdcSampleBlock block = {};
    if (sampler_.read_block(block)) {
        if (block.overflowed) {
            recover_overflow(settings);
            tight_loop_contents();
            return false;
        }
        acquisition_.process_interleaved(block.words, block.word_count, settings);
    }

    if (acquisition_.frame_ready()) {
        return true;
    }
    tight_loop_contents();
    return false;
}

// Takes no inputs and returns the current acquisition frame.
const ScopeFrame &CapturePipeline::frame() const
{
    return acquisition_.frame();
}

// Takes current settings, releases the current ready frame, and returns nothing.
void CapturePipeline::acknowledge_frame(const ScopeSettings &settings)
{
    acquisition_.acknowledge_frame(settings);
}

} // namespace picoscope
