#include "adc_dma_setup.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

// Define the physical buffers with 32KB alignment.
// This is strictly required for the DMA's ring buffer address wrapping feature.
uint16_t buffer_ping[BUFFER_SAMPLES] __attribute__((aligned(BUFFER_BYTES)));
uint16_t buffer_pong[BUFFER_SAMPLES] __attribute__((aligned(BUFFER_BYTES)));

// Global handles to store the allocated hardware DMA channels
int dma_ping;
int dma_pong;

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
		// Claim two unused hardware DMA channels
		dma_ping = dma_claim_unused_channel(true);
		dma_pong = dma_claim_unused_channel(true);

		// Configure Ping Channel
		dma_channel_config c_ping = dma_channel_get_default_config(dma_ping);
		channel_config_set_read_increment(&c_ping, false); // Read from fixed ADC FIFO address
		channel_config_set_write_increment(&c_ping, true); // Increment write pointer into RAM
		channel_config_set_transfer_data_size(&c_ping, DMA_SIZE_16); // 16-bit data chunks
		channel_config_set_dreq(&c_ping, DREQ_ADC); // Pace transfers to ADC clock speed
		channel_config_set_ring(&c_ping, false, 15); // 2^15 = 32768 byte write ring wrap
		channel_config_set_chain_to(&c_ping, dma_pong); // When Ping finishes, trigger Pong

		// Configure Pong Channel
		dma_channel_config c_pong = dma_channel_get_default_config(dma_pong);
		channel_config_set_read_increment(&c_pong, false);
		channel_config_set_write_increment(&c_pong, true);
		channel_config_set_transfer_data_size(&c_pong, DMA_SIZE_16);
		channel_config_set_dreq(&c_pong, DREQ_ADC);
		channel_config_set_ring(&c_pong, false, 15);
		channel_config_set_chain_to(&c_pong, dma_ping); // When Pong finishes, trigger Ping

		// APPLY CONFIGURATIONS AND INITIALIZE HARDWARE COUNT
		dma_channel_configure(dma_ping, &c_ping, buffer_ping, &adc_hw->fifo, BUFFER_SAMPLES, false);
		dma_channel_configure(dma_pong, &c_pong, buffer_pong, &adc_hw->fifo, BUFFER_SAMPLES, false);

		// KICKSTART HARDWARE LOOP
		dma_channel_start(dma_ping); // Arm the Ping channel first
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
		// Read the current remaining transfer counts directly from the hardware registers
		uint32_t ping_remaining = dma_hw->ch[dma_ping].transfer_count;
		uint32_t pong_remaining = dma_hw->ch[dma_pong].transfer_count;

		// Convert down-counter into the next slot to be written (0 to 16384 range)
		uint32_t ping_next = BUFFER_SAMPLES - ping_remaining;
		uint32_t pong_next = BUFFER_SAMPLES - pong_remaining;

		// Handle out-of-bounds wrapping for an inactive/completed channel
		if (ping_next >= BUFFER_SAMPLES) ping_next = 0;
		if (pong_next >= BUFFER_SAMPLES) pong_next = 0;

		// Shift backward by 1 to get the actual LAST written sample.
		// If next is 0, the last written sample is at the very end of the buffer (BUFFER_SAMPLES - 1)
		heads->ping_write_index = (ping_next == 0) ? (BUFFER_SAMPLES - 1) : (ping_next - 1);
		heads->pong_write_index = (pong_next == 0) ? (BUFFER_SAMPLES - 1) : (pong_next - 1);
}
