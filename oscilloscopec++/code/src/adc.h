#pragma once
#include "pico/stdlib.h"

// Buffer sizes: 32KB each = 16,384 samples (16-bit)
#define BUFFER_BYTES (32768)
#define BUFFER_SAMPLES (BUFFER_BYTES / sizeof(uint16_t))

// External declarations for main program file
extern uint16_t buffer_ping[BUFFER_SAMPLES];
extern uint16_t buffer_pong[BUFFER_SAMPLES];

// Head index struct for main program to process samples
typedef struct {
		uint32_t ping_write_index;
		uint32_t pong_write_index;
} dual_write_heads_t;

// Function prototypes
void init_adc_dma_ping_pong();
void pause_adc_dma();
void resume_adc_dma();
void get_write_heads(dual_write_heads_t* heads);
