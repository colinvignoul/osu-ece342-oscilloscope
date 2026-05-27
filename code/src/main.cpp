/**
 * ILI9341 datasheet: https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf
 * 
 * TODO: re-write in C++, break into header+implementation files
 */

#include <stdio.h>
#include <stdint.h>
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

// Display geometry in landscape orientation
#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240
#define BYTES_PER_PIXEL 2

// Some RGB565 colors
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_GREEN 0x07E0
#define COLOR_GREY  0x4228

// Full-screen framebuffer stored in display byte order: high byte, then low byte.
// 320 * 240 * 2 = 153,600 bytes, which fits in RP2040 SRAM but is a significant chunk of it.
static uint8_t framebuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT * BYTES_PER_PIXEL];

// Example waveform sample buffer used for testing/demo.
// This buffer will be filled by ADC eventually.
static int16_t waveform_samples[DISPLAY_WIDTH];

// DMA channel used for SPI TX transfers
static int dma_tx_channel;
static dma_channel_config dma_tx_config;

// Helper: Write a buffer to SPI using DMA and wait for completion
void spi_write_dma_blocking(const uint8_t *data, size_t len)
{
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
void ili9341_write_cmd(uint8_t cmd)
{
    gpio_put(PIN_SPI_DC, 0); // DC low for command
    gpio_put(PIN_SPI_CS, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(PIN_SPI_CS, 1);
}

// Helper: Send Data
void ili9341_write_data(uint8_t data)
{
    gpio_put(PIN_SPI_DC, 1); // DC high for data
    gpio_put(PIN_SPI_CS, 0);
    spi_write_blocking(SPI_PORT, &data, 1);
    gpio_put(PIN_SPI_CS, 1);
}

// Set the rectangular region that subsequent pixel writes will fill.
void ili9341_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // Column Address Set
    ili9341_write_cmd(0x2A);
    ili9341_write_data(x0 >> 8);
    ili9341_write_data(x0 & 0xFF);
    ili9341_write_data(x1 >> 8);
    ili9341_write_data(x1 & 0xFF);

    // Page Address Set
    ili9341_write_cmd(0x2B);
    ili9341_write_data(y0 >> 8);
    ili9341_write_data(y0 & 0xFF);
    ili9341_write_data(y1 >> 8);
    ili9341_write_data(y1 & 0xFF);
}

// Send a prepared RGB565 pixel buffer to the display.
void ili9341_write_pixels_dma(const uint8_t *pixels, size_t byte_count)
{
    ili9341_set_addr_window(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
    ili9341_write_cmd(0x2C); // Memory Write

    gpio_put(PIN_SPI_DC, 1); // Data mode for pixel bytes
    gpio_put(PIN_SPI_CS, 0);
    spi_write_dma_blocking(pixels, byte_count);
    gpio_put(PIN_SPI_CS, 1);
}

void ili9341_write_framebuffer_dma(void)
{
    ili9341_write_pixels_dma(framebuffer, sizeof(framebuffer));
}

void framebuffer_set_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
        return;
    }

    uint32_t pixel_index = (uint32_t)y * DISPLAY_WIDTH + (uint32_t)x;
    framebuffer[pixel_index * 2] = color >> 8;
    framebuffer[pixel_index * 2 + 1] = color & 0xFF;
}

void framebuffer_fill(uint16_t color)
{
    uint8_t color_high = color >> 8;
    uint8_t color_low = color & 0xFF;

    for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
        framebuffer[i * 2] = color_high;
        framebuffer[i * 2 + 1] = color_low;
    }
}

static int abs_int(int value)
{
    return value < 0 ? -value : value;
}

// Simple integer line drawing so adjacent waveform samples connect visually.
void framebuffer_draw_line(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = abs_int(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs_int(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        framebuffer_set_pixel(x0, y0, color);

        if (x0 == x1 && y0 == y1) {
            break;
        }

        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

int sample_to_screen_y(int16_t sample)
{
    // Map signed 16-bit sample range [-32768, 32767] onto screen y coordinates.
    uint32_t shifted = (uint32_t)((int32_t)sample + 32768); // 0 to 65535
    return (DISPLAY_HEIGHT - 1) - (int)((shifted * (DISPLAY_HEIGHT - 1)) / 65535);
}

// Render a waveform sample buffer into the RGB565 framebuffer.
void render_waveform_to_framebuffer(const int16_t *samples, uint32_t sample_count, uint16_t trace_color, uint16_t background_color)
{
    framebuffer_fill(background_color);

    if (sample_count == 0) {
        return;
    }

    int prev_x = 0;
    uint32_t prev_sample_index = 0;
    int prev_y = sample_to_screen_y(samples[prev_sample_index]);

    for (int x = 1; x < DISPLAY_WIDTH; ++x) {
        uint32_t sample_index = ((uint32_t)x * sample_count) / DISPLAY_WIDTH;
        if (sample_index >= sample_count) {
            sample_index = sample_count - 1;
        }

        int y = sample_to_screen_y(samples[sample_index]);
        framebuffer_draw_line(prev_x, prev_y, x, y, trace_color);

        prev_x = x;
        prev_y = y;
    }
}

// Initialize the ILI9341 Controller
void ili9341_init()
{
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
    ili9341_write_data(0x48); // Landscape orientation, BGR color order

    ili9341_write_cmd(0x29); // Display ON
    sleep_ms(100);
}

// Fill screen with a 16-bit RGB565 color
void ili9341_fill_screen(uint16_t color)
{
    framebuffer_fill(color);
    ili9341_write_framebuffer_dma();
}

int main()
{
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

    // Draw an example waveform repeatedly.
    // Replace this sample-generation code with ADC-filled samples.
    uint32_t frame = 0;
    while (true) {
        for (uint32_t x = 0; x < DISPLAY_WIDTH; ++x) {
            uint32_t phase = (x + frame) % 80;
            int32_t triangle = phase < 40 ? (int32_t)phase : (int32_t)(80 - phase);
            waveform_samples[x] = (int16_t)(triangle * 1400 - 28000);
        }

        render_waveform_to_framebuffer(waveform_samples, DISPLAY_WIDTH, COLOR_GREEN, COLOR_BLACK);
        ili9341_write_framebuffer_dma();

        ++frame;
    }
    return 0;
}