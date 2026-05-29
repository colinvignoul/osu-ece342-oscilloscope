// main.cpp
// Raspberry Pi Pico SDK version of your MicroPython code

#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

// ============================================================
// Simple Rotary Encoder Class
// ============================================================

class RotaryEncoder {
public:
    RotaryEncoder(uint clk_pin, uint dt_pin)
        : clk(clk_pin), dt(dt_pin), position(0)
    {
        gpio_init(clk);
        gpio_set_dir(clk, GPIO_IN);
        gpio_pull_up(clk);

        gpio_init(dt);
        gpio_set_dir(dt, GPIO_IN);
        gpio_pull_up(dt);

        last_clk_state = gpio_get(clk);
    }

    int value() {
        update();
        return position;
    }

private:
    uint clk;
    uint dt;

    int position;

    bool last_clk_state;

    void update() {

        bool clk_state = gpio_get(clk);

        // Detect rising edge
        if (clk_state && !last_clk_state) {

            if (gpio_get(dt) != clk_state)
                position++;
            else
                position--;
        }

        last_clk_state = clk_state;
    }
};

// ============================================================
// Main
// ============================================================

int main() {

    stdio_init_all();

    // ========================================================
    // Variables
    // ========================================================

    float horz1_pos = 0.0f;
    float vert1_pos = 0.0f;

    float horz2_pos = 0.0f;
    float vert2_pos = 0.0f;

    float horz1_stretch = 1.0f;
    float vert1_stretch = 1.0f;

    float horz2_stretch = 1.0f;
    float vert2_stretch = 1.0f;

    float trig1_pos = 0.0f;
    float trig2_pos = 0.0f;

    const float trig_shift = 0.05f;
    const float graph_shift = 1.0f;
    const float graph_scale = 1.25f;

    float voltage1 = -1.0f;
    float voltage2 = -0.5f;

    bool trig1_ready = false;
    bool trig2_ready = false;

    int counter = 0;

    // ========================================================
    // GPIO Definitions
    // ========================================================

    const uint LED_PIN = PICO_DEFAULT_LED_PIN;

    uint shift_pins[2] = {3, 4};
    uint scale_pins[2] = {5, 6};
    uint trig_pins[2]  = {0, 1};

    const uint horz_switch_pin = 7;
    const uint signal2_switch_pin = 2;

    // ========================================================
    // GPIO Setup
    // ========================================================

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(horz_switch_pin);
    gpio_set_dir(horz_switch_pin, GPIO_IN);
    gpio_pull_down(horz_switch_pin);

    gpio_init(signal2_switch_pin);
    gpio_set_dir(signal2_switch_pin, GPIO_IN);
    gpio_pull_down(signal2_switch_pin);

    // ========================================================
    // Rotary Encoders
    // ========================================================

    RotaryEncoder shift(shift_pins[0], shift_pins[1]);
    RotaryEncoder scale(scale_pins[0], scale_pins[1]);
    RotaryEncoder trig(trig_pins[0], trig_pins[1]);

    int shift_old = shift.value();
    int scale_old = scale.value();
    int trig_old = trig.value();

    // ========================================================
    // Main Loop
    // ========================================================

    while (true) {

        // ----------------------------------------------------
        // Counter
        // ----------------------------------------------------

        if (counter > 500)
            counter = 0;

        counter++;

        // ----------------------------------------------------
        // Read switches
        // ----------------------------------------------------

        bool horz = gpio_get(horz_switch_pin);
        bool sig2 = gpio_get(signal2_switch_pin);

        // ----------------------------------------------------
        // Read encoder values
        // ----------------------------------------------------

        int shift_new = shift.value();
        int scale_new = scale.value();
        int trig_new  = trig.value();

        // ====================================================
        // POSITION CONTROL
        // ====================================================

        if (shift_new > shift_old) {

            if (horz && sig2)
                horz2_pos += graph_shift;

            else if (horz && !sig2)
                horz1_pos += graph_shift;

            else if (!horz && sig2)
                vert2_pos += graph_shift;

            else
                vert1_pos += graph_shift;
        }

        else if (shift_new < shift_old) {

            if (horz && sig2)
                horz2_pos -= graph_shift;

            else if (horz && !sig2)
                horz1_pos -= graph_shift;

            else if (!horz && sig2)
                vert2_pos -= graph_shift;

            else
                vert1_pos -= graph_shift;
        }

        // ====================================================
        // SCALE CONTROL
        // ====================================================

        if (scale_new > scale_old) {

            if (horz && sig2)
                horz2_stretch *= graph_scale;

            else if (horz && !sig2)
                horz1_stretch *= graph_scale;

            else if (!horz && sig2)
                vert2_stretch *= graph_scale;

            else
                vert1_stretch *= graph_scale;
        }

        else if (scale_new < scale_old) {

            if (horz && sig2)
                horz2_stretch /= graph_scale;

            else if (horz && !sig2)
                horz1_stretch /= graph_scale;

            else if (!horz && sig2)
                vert2_stretch /= graph_scale;

            else
                vert1_stretch /= graph_scale;
        }

        // ====================================================
        // TRIGGER LEVEL CONTROL
        // ====================================================

        if (trig_new > trig_old) {

            if (sig2)
                trig2_pos += trig_shift;
            else
                trig1_pos += trig_shift;
        }

        else if (trig_new < trig_old) {

            if (sig2)
                trig2_pos -= trig_shift;
            else
                trig1_pos -= trig_shift;
        }

        // ====================================================
        // Update old encoder values
        // ====================================================

        shift_old = shift_new;
        scale_old = scale_new;
        trig_old = trig_new;

        // ====================================================
        // Trigger Detection
        // ====================================================

        if ((voltage1 < trig1_pos) && !trig1_ready) {

            trig1_ready = true;
        }

        else if ((voltage1 >= trig1_pos) && trig1_ready) {

            trig1_ready = false;
            printf("Trigger 1 Activated\n");
        }

        if ((voltage2 < trig2_pos) && !trig2_ready) {

            trig2_ready = true;
        }

        else if ((voltage2 >= trig2_pos) && trig2_ready) {

            trig2_ready = false;
            printf("Trigger 2 Activated\n");
        }

        // ====================================================
        // Debug Output
        // ====================================================

        if (counter == 500) {

            gpio_put(LED_PIN, !gpio_get(LED_PIN));

            printf("=================================\n");

            printf("Vertical 1 Position: %f\n", vert1_pos);
            printf("Horizontal 1 Position: %f\n", horz1_pos);

            printf("Vertical 2 Position: %f\n", vert2_pos);
            printf("Horizontal 2 Position: %f\n", horz2_pos);

            printf("Vertical 1 Scale: %f\n", vert1_stretch);
            printf("Horizontal 1 Scale: %f\n", horz1_stretch);

            printf("Vertical 2 Scale: %f\n", vert2_stretch);
            printf("Horizontal 2 Scale: %f\n", horz2_stretch);

            printf("Trigger 1 Level: %f Voltage 1: %f\n",
                   trig1_pos,
                   voltage1);

            printf("Trigger 2 Level: %f Voltage 2: %f\n",
                   trig2_pos,
                   voltage2);
        }

        sleep_ms(10);
    }

    return 0;
}