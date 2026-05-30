#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

// Define buffer sizes: 32KB each = 16,384 samples (16-bit)
#define BUFFER_BYTES (32768)
#define BUFFER_SAMPLES (BUFFER_BYTES / sizeof(uint16_t))

// Two separate buffers, each aligned to its own 32KB boundary
uint16_t buffer_ping[BUFFER_SAMPLES] __attribute__((aligned(BUFFER_BYTES)));
uint16_t buffer_pong[BUFFER_SAMPLES] __attribute__((aligned(BUFFER_BYTES)));

// Global flags to tell the main loop when data is ready to process
volatile bool ping_ready = false;
volatile bool pong_ready = false;

// Channel handles
int dma_ping;
int dma_pong;

// Interrupt handler when a DMA transfer completes
void dma_handler() {
    if (dma_hw->ints0 & (1 << dma_ping)) {
        dma_hw->ints0 = (1 << dma_ping); // Clear interrupt flag
        ping_ready = true;
        // Re-arm Ping's transfer count so it's ready when Pong finishes
        dma_channel_set_trans_count(dma_ping, BUFFER_SAMPLES, false);
    }
    
    if (dma_hw->ints0 & (1 << dma_pong)) {
        dma_hw->ints0 = (1 << dma_pong); // Clear interrupt flag
        pong_ready = true;
        // Re-arm Pong's transfer count so it's ready when Ping finishes
        dma_channel_set_trans_count(dma_pong, BUFFER_SAMPLES, false);
    }
}

int main() {
    stdio_init_all();

    // SETUP ADC
    adc_init();
    adc_gpio_init(26);
    adc_gpio_init(27);
    adc_set_round_robin(0x03); 
    adc_select_input(0);
    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(95.0f); // 500 ks/s

    // CLAIM TWO DMA CHANNELS
    dma_ping = dma_claim_unused_channel(true);
    dma_pong = dma_claim_unused_channel(true);

    // CONFIGURE PING CHANNEL
    dma_channel_config c_ping = dma_channel_get_default_config(dma_ping);
    channel_config_set_read_increment(&c_ping, false);
    channel_config_set_write_increment(&c_ping, true);
    channel_config_set_transfer_data_size(&c_ping, DMA_SIZE_16);
    channel_config_set_dreq(&c_ping, DREQ_ADC);
    // Ring buffer size 15 = 2^15 = 32768 bytes
    channel_config_set_ring(&c_ping, false, 15); 
    // When ping finishes, it automatically fires pong
    channel_config_set_chain_to(&c_ping, dma_pong); 

    // CONFIGURE PONG CHANNEL
    dma_channel_config c_pong = dma_channel_get_default_config(dma_pong);
    channel_config_set_read_increment(&c_pong, false);
    channel_config_set_write_increment(&c_pong, true);
    channel_config_set_transfer_data_size(&c_pong, DMA_SIZE_16);
    channel_config_set_dreq(&c_pong, DREQ_ADC);
    channel_config_set_ring(&c_pong, false, 15);
    // When pong finishes, it automatically fires ping
    channel_config_set_chain_to(&c_pong, dma_ping);

    // INITIALIZE BOTH CHANNELS (Don't trigger yet)
    dma_channel_configure(dma_ping, &c_ping, buffer_ping, &adc_hw->fifo, BUFFER_SAMPLES, false);
    dma_channel_configure(dma_pong, &c_pong, buffer_pong, &adc_hw->fifo, BUFFER_SAMPLES, false);

    // SETUP INTERRUPTS TO CAPTURE COMPLETION EVENTS
    dma_set_irq0_channel_mask_enabled((1 << dma_ping) | (1 << dma_pong), true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // START CACHING (Kickoff Ping first, then start ADC)
    dma_channel_start(dma_ping);
    adc_run(true);
