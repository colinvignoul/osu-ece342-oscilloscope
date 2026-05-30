#include "adc_dma_setup.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

// Define a unified 64KB block of physical memory with strict 64KB alignment.
// This is required for the RP2040 DMA's hardware ring address wrapping feature (2^16 = 65536 bytes).
// Due to round-robin multiplexing, ADC0 lands on even indices, and ADC1 lands on odd indices.
uint16_t combined_dma_buffer[BUFFER_SAMPLES * 2] __attribute__((aligned(BUFFER_BYTES * 2)));

// Global handle to store the allocated hardware DMA channel
int dma_ping;

void init_adc_dma_ping_pong() {
		// INITIALIZE ADC HARDWARE
		adc_init();
		adc_gpio_init(26); // Setup Pin 26 (ADC0)
		adc_gpio_init(27); // Setup Pin 27 (ADC1)

		// Enable Round Robin multiplexing for channels 0 and 1
		adc_set_round_robin(0x03); 
		adc_select_input(0); // Start the rotation at ADC0

		// Setup the internal ADC FIFO
		// true: Enable FIFO, true: Enable DMA Request (DREQ)
		// 1: Threshold to assert DREQ (1 sample present), false: Don't shift error bits
		adc_fifo_setup(true, true, 1, false, false);

		// Set sample clock divider (95 means 96 clock cycles per conversion)
		// 48,000,000 Hz / 96 = 500,000 total conversions per second (500ks/s)
		adc_set_clkdiv(95.0f); 

		// ALLOCATE AND CONFIGURE DMA CHANNELS
		// Claim one unused hardware DMA channel for continuous infinite ring wrapping
		dma_ping = dma_claim_unused_channel(true);

		// Configure DMA Channel as an infinite hardware ring buffer
		dma_channel_config c_ping = dma_channel_get_default_config(dma_ping);
		channel_config_set_read_increment(&c_ping, false); // Read from fixed ADC FIFO address
		channel_config_set_write_increment(&c_ping, true); // Increment write pointer into RAM
		channel_config_set_transfer_data_size(&c_ping, DMA_SIZE_16); // 16-bit data chunks
		channel_config_set_dreq(&c_ping, DREQ_ADC); // Pace transfers to ADC clock speed
		channel_config_set_ring(&c_ping, false, 16); // 2^16 = 65536 byte write ring wrap (Infinite circular execution)
		channel_config_set_chain_to(&c_ping, dma_ping); // Self-chaining keeps the channel indefinitely armed

		// APPLY CONFIGURATIONS AND INITIALIZE HARDWARE COUNT
		// Set transfer count to 0xFFFFFFFF for endless execution inside the hardware ring bitmask
		dma_channel_configure(dma_ping, &c_ping, combined_dma_buffer, &adc_hw->fifo, 0xFFFFFFFF, false);

		// KICKSTART HARDWARE LOOP
		dma_channel_start(dma_ping); // Arm the unified channel first
		adc_run(true); // Turn on the free-running hardware ADC clock
}

void pause_adc_dma() {
		adc_run(false); // Freeze hardware ADC clock
		adc_fifo_drain(); // Clear any conversions in FIFO
}

void resume_adc_dma() {
		adc_run(true); // Restart hardware ADC clock
}

void get_write_heads(dual_write_heads_t* heads) {
		// Read the current remaining transfer count directly from the hardware register
		uint32_t ping_remaining = dma_hw->ch[dma_ping].transfer_count;

		// Convert down-counter into the global hardware index (0 to 32767 range)
		uint32_t global_next = (0xFFFFFFFF - ping_remaining + 1) % (BUFFER_SAMPLES * 2);

		// Pin down the next slots to be written for each sequence
		uint32_t ping_next = global_next & ~1u; // Always even (ADC0)
		uint32_t pong_next = global_next | 1u;  // Always odd (ADC1)

		// Shift backward by 2 steps to get the actual LAST written sample location for each channel.
		// If the calculation drops below 0, wrap around safely to the end of the combined buffer.
		heads->ping_write_index = (ping_next >= 2) ? (ping_next - 2) : ((BUFFER_SAMPLES * 2) - 2);
		heads->pong_write_index = (pong_next >= 3) ? (pong_next - 2) : ((BUFFER_SAMPLES * 2) - 1);
}
