#pragma once
#include "pico/stdlib.h"

// Buffer sizes: 32KB per channel = 16,384 samples (16-bit)
#define BUFFER_BYTES (32768)
#define BUFFER_SAMPLES (BUFFER_BYTES / sizeof(uint16_t))

// External declaration for the unified interleaved hardware ring buffer.
// Size is doubled to accommodate both ADC0 (Even indices) and ADC1 (Odd indices) concurrently.
extern uint16_t combined_dma_buffer[BUFFER_SAMPLES * 2];

// Head index struct for main program to process samples.
// These return the absolute index within combined_dma_buffer where the last valid sample was written.
typedef struct {
		uint32_t ping_write_index; // Absolute index for the latest ADC0 (Even) sample
		uint32_t pong_write_index; // Absolute index for the latest ADC1 (Odd) sample
} dual_write_heads_t;

// Function prototypes
void init_adc_dma_ping_pong();
void pause_adc_dma();
void resume_adc_dma();
void get_write_heads(dual_write_heads_t* heads);
