/**
 * ILI9341 datasheet: https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf
 * 
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

// Define pins
#define SPI_PORT spi0
#define PIN_SPI_MISO 16 // not used but define in case of future need
#define PIN_SPI_CS   17 // active low
#define PIN_SPI_SCK  18
#define PIN_SPI_MOSI 19
#define PIN_SPI_DC   20
#define PIN_SPI_RST  21

// Set SPI speed 
#define SPI_HZ 62500000

// DMA channel used for SPI TX transfers
static int dma_tx_channel;
static dma_channel_config dma_tx_config;

// Helper: Write a buffer to SPI using DMA and wait for completion
void spi_write_dma_blocking(const uint8_t *data, size_t len) {
    dma_channel_configure(
        dma_tx_channel,
        &dma_tx_config,
        &spi_get_hw(SPI_PORT)->dr, // write address: SPI data register
        data, // read address: source buffer
        len, // number of 8-bit transfers
        true // start immediately
    );

    // DMA is finished when the last byte has entered the SPI TX FIFO
    dma_channel_wait_for_finish_blocking(dma_tx_channel);

    // Wait until SPI has actually shifted out the final byte before changing CS.
    while (spi_is_busy(SPI_PORT)) { }
}

// Helper: Send Command
void ili9341_write_cmd(uint8_t cmd) {
    gpio_put(PIN_SPI_DC, 0); // DC low for command
    gpio_put(PIN_SPI_CS, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(PIN_SPI_CS, 1);
}

// Helper: Send Data
void ili9341_write_data(uint8_t data) {
    gpio_put(PIN_SPI_DC, 1); // DC high for data
    gpio_put(PIN_SPI_CS, 0);
    spi_write_blocking(SPI_PORT, &data, 1);
    gpio_put(PIN_SPI_CS, 1);
}

// Initialize the ILI9341 Controller
void ili9341_init() {
    // Hardware reset pulse
    gpio_put(PIN_SPI_RST, 1); sleep_ms(5);
    gpio_put(PIN_SPI_RST, 0); sleep_ms(20);
    gpio_put(PIN_SPI_RST, 1); sleep_ms(150);

    ili9341_write_cmd(0x01); // Software reset
    sleep_ms(100);

    ili9341_write_cmd(0x11); // Sleep out (take display out of sleep)
    sleep_ms(250);

    ili9341_write_cmd(0x3A); // Set Pixel Format
    ili9341_write_data(0x55); // 16-bit RGB565

    ili9341_write_cmd(0x36); // Set Memory Access Control
    ili9341_write_data(0x48); // MX = 1; BGR = 1;

    ili9341_write_cmd(0x29); // Display ON
    sleep_ms(100);
}

// Fill screen with a 16-bit RGB565 color
void ili9341_fill_screen(uint16_t color) {
    // Set Column Address (0 to 239)
    ili9341_write_cmd(0x2A); 
    ili9341_write_data(0x00); ili9341_write_data(0x00);
    ili9341_write_data(0x00); ili9341_write_data(0xEF);

    // Set Page Address (0 to 319)
    ili9341_write_cmd(0x2B); 
    ili9341_write_data(0x00); ili9341_write_data(0x00);
    ili9341_write_data(0x01); ili9341_write_data(0x3F);

    // Write Memory Start
    ili9341_write_cmd(0x2C);

    // Fast block write
    gpio_put(PIN_SPI_DC, 1);
    gpio_put(PIN_SPI_CS, 0);
    
    uint8_t color_high = color >> 8;
    uint8_t color_low = color & 0xFF;

    // Send the screen fill in chunks instead of one byte at a time
    // Each pixel is 2 bytes
    const uint32_t pixel_count = 240 * 320;
    const uint32_t pixels_per_chunk = 240;
    uint8_t buffer[pixels_per_chunk * 2];

    for (uint32_t i = 0; i < pixels_per_chunk; ++i) {
        buffer[2 * i] = color_high;
        buffer[2 * i + 1] = color_low;
    }

    uint32_t pixels_remaining = pixel_count;
    while (pixels_remaining > 0) {
        uint32_t pixels_this_chunk = pixels_remaining > pixels_per_chunk
            ? pixels_per_chunk
            : pixels_remaining;

        spi_write_dma_blocking(buffer, pixels_this_chunk * 2);
        pixels_remaining -= pixels_this_chunk;
    }
    gpio_put(PIN_SPI_CS, 1);
}

int main() {
    // stdio_init_all();
    // sleep_ms(2000);
    
    // Init SPI at SPI_HZ
    /*uint actual_baudrate = */spi_init(SPI_PORT, SPI_HZ);
    gpio_set_function(PIN_SPI_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_MOSI, GPIO_FUNC_SPI);
    // printf("Requested SPI baudrate: %u Hz\n", SPI_HZ);
    // printf("Actual SPI baudrate: %u Hz\n", actual_baudrate);

    // Claim and configure one DMA channel for SPI TX
    dma_tx_channel = dma_claim_unused_channel(true);
    dma_tx_config = dma_channel_get_default_config(dma_tx_channel);
    channel_config_set_transfer_data_size(&dma_tx_config, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_tx_config, true);
    channel_config_set_write_increment(&dma_tx_config, false);
    channel_config_set_dreq(&dma_tx_config, spi_get_dreq(SPI_PORT, true));

    // Initialize GPIO Pins
    gpio_init(PIN_SPI_CS);
    gpio_set_dir(PIN_SPI_CS, GPIO_OUT);
    gpio_put(PIN_SPI_CS, 1);

    gpio_init(PIN_SPI_DC);
    gpio_set_dir(PIN_SPI_DC, GPIO_OUT);
    gpio_put(PIN_SPI_DC, 0);

    gpio_init(PIN_SPI_RST);
    gpio_set_dir(PIN_SPI_RST, GPIO_OUT);
    gpio_put(PIN_SPI_RST, 1);

    // Turn on the display
    ili9341_init();

    // Cycle colors infinitely
    while (true) {
        ili9341_fill_screen(0xFFFF); // White
        ili9341_fill_screen(0x4228); // Grey
    }
    return 0;
}