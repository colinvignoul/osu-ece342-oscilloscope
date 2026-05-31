// Purpose: Declares the sampling/acquisition orchestration pipeline used by
// firmware core 0.
// Interface: Call init(), restart_for_settings(), process(), frame(), and
// acknowledge_frame() from the main polling loop.
// Constraints: Borrows fixed-storage sampler/acquisition objects and performs no
// dynamic allocation.
// Ownership: CapturePipeline owns sampler-mode policy and overflow recovery; the
// borrowed sampler, history acquisition, and acquisition engine own their data.

#pragma once

#include "acquisition_engine.hpp"
#include "adc_sampler.hpp"
#include "history_acquisition.hpp"
#include "scope_types.hpp"

namespace picoscope {

class CapturePipeline {
public:
    // Takes long-lived sampler/acquisition objects by reference and stores them
    // for the firmware lifetime.
    CapturePipeline(AdcSampler &sampler,
                    HistoryAcquisition &history_acquisition,
                    AcquisitionEngine &acquisition);

    CapturePipeline(const CapturePipeline &) = delete;
    CapturePipeline &operator=(const CapturePipeline &) = delete;

    // Takes initial settings, initializes sampler/acquisition state, and returns
    // nothing.
    void init(const ScopeSettings &settings);

    // Takes changed settings, clears capture/history state, restarts sampling
    // when running, and returns nothing.
    void restart_for_settings(const ScopeSettings &settings);

    // Takes current settings, processes available ADC data, and returns true when
    // a frame is ready to publish.
    bool process(const ScopeSettings &settings);

    // Takes no inputs and returns the current acquisition frame.
    const ScopeFrame &frame() const;

    // Takes current settings, releases the current ready frame, and returns
    // nothing.
    void acknowledge_frame(const ScopeSettings &settings);

private:
    // Takes current settings, switches to the required ADC mode if needed, and
    // returns nothing.
    void ensure_sampler_mode(const ScopeSettings &settings);

    // Takes current settings after an ADC overflow, realigns sampling/acquisition,
    // and returns nothing.
    void recover_overflow(const ScopeSettings &settings);

    AdcSampler &sampler_;
    HistoryAcquisition &history_acquisition_;
    AcquisitionEngine &acquisition_;
};

} // namespace picoscope
