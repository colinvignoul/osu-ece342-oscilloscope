#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// Define pins (Matching the previous wiring guide)
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19
#define PIN_DC   20
#define PIN_RST  21

// Set SPI speed 
#define SPI_HZ 62500000

// Helper: Send Command
void ili9341_write_cmd(uint8_t cmd) {
    gpio_put(PIN_DC, 0);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(PIN_CS, 1);
}

// Helper: Send Data
void ili9341_write_data(uint8_t data) {
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &data, 1);
    gpio_put(PIN_CS, 1);
}

// Initialize the ILI9341 Controller
void ili9341_init() {
    // Hardware reset pulse
    gpio_put(PIN_RST, 1); sleep_ms(5);
    gpio_put(PIN_RST, 0); sleep_ms(20);
    gpio_put(PIN_RST, 1); sleep_ms(150);

    // Minimal Init Sequence
    ili9341_write_cmd(0x01); // Software reset
    sleep_ms(100);

    ili9341_write_cmd(0x11); // Sleep out
    sleep_ms(250);

    ili9341_write_cmd(0x3A); // Set Pixel Format 
    ili9341_write_data(0x55); // 16-bit RGB565

    ili9341_write_cmd(0x36); // Set Memory Access Control
    ili9341_write_data(0x48); // MV = 1 (landscape); BGR = 1 

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
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    
    uint8_t color_high = color >> 8;
    uint8_t color_low = color & 0xFF;

    // Send the screen fill in chunks instead of one byte at a time
    // Each pixel is 2 bytes
    const uint32_t pixel_count = 240 * 320;
    const uint32_t pixels_per_chunk = 240;
    uint8_t buffer[pixels_per_chunk * 2];

    for (uint32_t i = 0; i < pixels_per_chunk; i++) {
        buffer[2 * i] = color_high;
        buffer[2 * i + 1] = color_low;
    }

    uint32_t pixels_remaining = pixel_count;
    while (pixels_remaining > 0) {
        uint32_t pixels_this_chunk = pixels_remaining > pixels_per_chunk
            ? pixels_per_chunk
            : pixels_remaining;

        spi_write_blocking(SPI_PORT, buffer, pixels_this_chunk * 2);
        pixels_remaining -= pixels_this_chunk;
    }
    gpio_put(PIN_CS, 1);
}

int main() {
    stdio_init_all();
    sleep_ms(7000);
    // 1. Init SPI at SPI_MHZ
    uint actual_baudrate = spi_init(SPI_PORT, SPI_HZ);
    printf("Requested SPI baudrate: %u Hz\n", SPI_HZ);
    printf("Actual SPI baudrate: %u Hz\n", actual_baudrate);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // 2. Init GPIO Pins
    gpio_init(PIN_CS); gpio_set_dir(PIN_CS, GPIO_OUT); gpio_put(PIN_CS, 1);
    gpio_init(PIN_DC); gpio_set_dir(PIN_DC, GPIO_OUT); gpio_put(PIN_DC, 0);
    gpio_init(PIN_RST); gpio_set_dir(PIN_RST, GPIO_OUT); gpio_put(PIN_RST, 1);

    // 3. Turn on the display
    ili9341_init();

    // 4. Cycle colors infinitely
    while (true) {
        ili9341_fill_screen(0xFFFF); // White
        ili9341_fill_screen(0x4228); // Grey
        // printf("Requested SPI baudrate: %u MHz\n", SPI_MHZ);
        //printf("Actual SPI baudrate: %u Hz\n", actual_baudrate);
    }
    return 0;
}