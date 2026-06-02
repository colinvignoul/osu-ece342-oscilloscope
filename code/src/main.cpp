// Purpose: Owns the firmware entry point and wires display, input, sampling,
// acquisition, and rendering pipelines.
// Constraints: Core 0 runs input/sampling/acquisition without dynamic
// allocation; core 1 is launched by RenderPipeline and owns display rendering.
// Ownership: main() owns long-lived subsystem instances for the process lifetime.

#include "acquisition_engine.hpp"
#include "adc_sampler.hpp"
#include "capture_pipeline.hpp"
#include "encoder_manager.hpp"
#include "history_acquisition.hpp"
#include "render_pipeline.hpp"
#include "scope_control.hpp"

#include "pico/stdlib.h"

#include <cstdio>

namespace {

picoscope::RenderPipeline render_pipeline;
picoscope::EncoderManager encoders;
picoscope::AdcSampler sampler;
picoscope::HistoryAcquisition history_acquisition;
picoscope::AcquisitionEngine acquisition;
picoscope::CapturePipeline capture_pipeline(sampler, history_acquisition, acquisition);

} // namespace

// main() initializes firmware systems and runs the oscilloscope polling loop forever
int main()
{
    stdio_init_all();
    sleep_ms(200);
    //sleep_ms(7000);
    // printf("USB serial working");

    picoscope::ScopeSettings settings = picoscope::default_scope_settings();
    capture_pipeline.init(settings);
    render_pipeline.init();
    render_pipeline.publish(capture_pipeline.frame(), settings);
    encoders.init();

    for (;;) {
        const picoscope::InputEvents input = encoders.poll();
        const picoscope::ScopeUpdate update =
            picoscope::apply_input_events(settings, input);

        if (update.reset_capture) {
            capture_pipeline.restart_for_settings(settings);
        }
        if (update.redraw) {
            render_pipeline.publish(capture_pipeline.frame(), settings);
        }

        if (capture_pipeline.process(settings)) {
            render_pipeline.publish(capture_pipeline.frame(), settings);
            capture_pipeline.acknowledge_frame(settings);
        }
    }
}
