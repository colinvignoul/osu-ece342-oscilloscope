#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "adc_dma_setup.h"

// NOTE: adc.cpp needs to be in the add_executable() field after adc.cpp to be linked

// Example trigger criteria configurations
#define TRIGGER_START_THRESHOLD 3000 // High ADC value to start copying
#define TRIGGER_STOP_THRESHOLD 500 // Low ADC value to stop copying
#define MAX_SEARCH_SAMPLES 8000 // Safety limit to prevent infinite loops
// NOTE: these thresholds, AS WELL as the actual conditions checks in the example
// below, will need to be tailored to whatever trigger conditions we want to look
// for. some additional logic/tweaking is likely necessary if we want to get MORE
// than one "loop" of the signal, which I think we certainly will (since we want to
// be able to zoom out and see a larger period of the waveform than just one trigger
// cycle)

// Macros to handle internal variable definitions matching your script's logic
#define HARD_SAFETY_LIMIT MAX_SEARCH_SAMPLES
#define SIGNAL_TRIGGER_START TRIGGER_START_THRESHOLD
#define SIGNAL_TRIGGER_STOP TRIGGER_STOP_THRESHOLD

void draw_loop_example() {
		// Pause adc system
		pause_adc_dma();

		// Querry adc data buffer location of last samples
		dual_write_heads_t heads;
		get_write_heads(&heads);

		// Iterate through samples and check conditions (ping)
		// NOTE: this only handles one of the two channels at a time; both ping and pong
		// will need to be handled, and variables may need to be defined so that the
		// logic for both ping and pong do not collide.
		// NOTE: the rest of the code until the adc is unpaused might best be contained
		// in a function, so that we can just run
		// it back to back on each of the values in the "heads" variable to
		// do both of the channels independently and cleanly
		int32_t lookback_idx = (int32_t)heads.ping_write_index; // Points directly to an EVEN index in the interleaved buffer
    
    int32_t sample_selection_start = -1;
    int32_t sample_selection_stop = -1;
    uint32_t inspected_samples_count = 0;

    while (inspected_samples_count < HARD_SAFETY_LIMIT) {
				// Enforce boundary wrapping inside the 0 to (BUFFER_SAMPLES * 2) - 1 array bounds.
        // If lookback_idx drops below 0 (either -1 or -2 depending on where the step landed), 
        // adding it directly to the total length mathematically ensures that:
        // - An EVEN stream (ADC0) wraps precisely to the last even index: (BUFFER_SAMPLES * 2) - 2
        // - An ODD stream (ADC1) wraps precisely to the last odd index: (BUFFER_SAMPLES * 2) - 1
        if (lookback_idx < 0) {
            lookback_idx = (BUFFER_SAMPLES * 2) + lookback_idx; 
        }

        // Pull the historic data point directly out of the ring buffer
        uint16_t sample = combined_dma_buffer[lookback_idx];

        // Condition Check A: Find where the selection "ends" walking backward (the start trigger)
        if (sample_selection_start == -1 && sample > SIGNAL_TRIGGER_START) {
            sample_selection_start = lookback_idx;
        }
        // Condition Check B: Once start is locked, find the threshold where the consecutive chunk begins
        else if (sample_selection_start != -1 && sample_selection_stop == -1 && sample < SIGNAL_TRIGGER_STOP) {
            sample_selection_stop = lookback_idx;
            break; // Target selection found, terminate search loop
        }

        lookback_idx -= 2; // Step backward by 2 to stay strictly on ADC0 (Even indices)
        inspected_samples_count++;
    }

		// Allocation and copying of selected samples
		// NOTE: Only execute if both triggers were succesfully satisfied.
		// Consider that if any part of the process fails, this needs to be handled
		// in the draw logic
		if (sample_selection_start != -1 && sample_selection_stop != -1) {
        
        // Calculate total consecutive elements needed, factoring in array boundary wrapping
        uint32_t total_samples_to_copy;
        if (sample_selection_start >= sample_selection_stop) {
            total_samples_to_copy = ((sample_selection_start - sample_selection_stop) / 2) + 1;
        } else {
            total_samples_to_copy = (((BUFFER_SAMPLES * 2) - sample_selection_stop + sample_selection_start) / 2) + 1;
        }

        // Dynamically allocate fresh memory block on the heap
        uint16_t* freshly_allocated_buffer = (uint16_t*)malloc(total_samples_to_copy * sizeof(uint16_t));

        if (freshly_allocated_buffer != NULL) {
            // Read forward from the chronological beginning (stop trigger) to the end (start trigger)
            int32_t copy_cursor = sample_selection_stop;
            for (uint32_t i = 0; i < total_samples_to_copy; i++) {
								if (copy_cursor >= (BUFFER_SAMPLES * 2)) {
										// Subtracting the total size ensures an even cursor wraps to 0, 
										// and an odd cursor wraps precisely to 1.
										copy_cursor = copy_cursor - (BUFFER_SAMPLES * 2); 
								}
                
                // Copy the selection into consecutive memory space
                freshly_allocated_buffer[i] = combined_dma_buffer[copy_cursor];
                copy_cursor += 2; // Step forward by 2 to stay strictly on ADC0 (Even indices)
            }

            // ----------------------------------------------------------------
            // SIMULATED SLOW CONSUMPTION
            // This is where the drawing system would use the extracted data 
            // for another purpose (e.g., rendering pixels to a screen buffer)
            // ----------------------------------------------------------------
            for (uint32_t i = 0; i < total_samples_to_copy; i++) {
                uint16_t sample_to_render = freshly_allocated_buffer[i];
                // perform_slow_render_operation(sample_to_render);
            }

            // ----------------------------------------------------------------
            // DISCARD/FREE DATA
            // Clear out the dynamically allocated heap block immediately 
            // ----------------------------------------------------------------
            free(freshly_allocated_buffer);
            freshly_allocated_buffer = NULL;
        }
		}
		
		resume_adc_dma();
}

int main() {
		stdio_init_all();

		init_adc_dma_ping_pong();

		while (1) {
		draw_loop_example();
		}
}
