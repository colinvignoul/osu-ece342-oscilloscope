// main.cpp
// Raspberry Pi Pico SDK version of your MicroPython code

#include <stdio.h>
#include <math.h>

#include <pico/stdio_usb.h>

#include <pico/stdlib.h>
#include <hardware/gpio.h>
// ============================================================
// Simple Rotary Encoder Class
// ============================================================
void gpio_callback(uint gpio, uint32_t events);

const uint SHIFT_CLK = 0;
const uint SHIFT_DT = 1;
const uint SCALE_CLK = 6;
const uint SCALE_DT = 2;
const uint TRIG_CLK = 3;
const uint TRIG_DT = 4;

const uint HORZ_SWITCH = 7;
const uint SIGNAL2_SWITCH = 5;

// ========================================================
// Variables
// ========================================================

volatile int horz1_pos = 0;
volatile int vert1_pos = 0;

volatile int horz2_pos = 0;
volatile int vert2_pos = 0;

float horz1_stretch = 1.0f;
float vert1_stretch = 1.0f;

float horz2_stretch = 1.0f;
float vert2_stretch = 1.0f;

float trig1_pos = 0.0f;
float trig2_pos = 0.0f;

const float trig_shift = 0.0125f;
const int graph_shift = 1;
const float graph_scale = pow(1.25, 0.25);

float voltage1 = -1.0f;
float voltage2 = -0.5f;

bool trig1_ready = false;
bool trig2_ready = false;

int counter = 0;

volatile bool horz = false;
volatile bool sig2 = false;

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

        // Register this instance
        instances[clk] = this;
        instances[dt] = this;

        // Enable interrupt. This debouncing technique catches all 4 transitions per detent.
        gpio_set_irq_enabled_with_callback(
            clk,
            GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
            true,
            &gpio_callback
        );

        gpio_set_irq_enabled_with_callback(
            dt,
            GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
            true,
            &gpio_callback
        );
    }

    int get_position() const {
        return position;
    }

    volatile uint32_t last_interrupt = 0;

    static void handle_gpio_irq(uint gpio) {
        RotaryEncoder* encoder = instances[gpio];

        if(encoder != nullptr) {
            
            uint32_t now = time_us_32();

            if (now - encoder->last_interrupt < 100) {
                return;
            }

            encoder->last_interrupt = now;

            encoder->handle_interrupt();
        }
    }

private:

    uint clk;
    uint dt;
    uint8_t previous_state =
    (gpio_get(clk) << 1) |
     gpio_get(dt);

    volatile int position;

    // ========================================================
    // Static instance lookup table
    // ========================================================

    static RotaryEncoder* instances[30];

    // ========================================================
    // Static ISR callback
    // ========================================================
    

    // ========================================================
    // Actual object interrupt handler
    // ========================================================
        //This function will use a state transition table to determine the direction of rotation and update the position accordingly. 
        //It also updates the global variables for graph position and scale based on the current mode (horizontal/vertical, signal 1/signal 2).
    void handle_interrupt() {

        uint8_t current_state =
        (gpio_get(clk) << 1) |
         gpio_get(dt);

        uint8_t transition =
            (previous_state << 2) |
            current_state;

        static const int8_t lookup[16] = {
            0, -1, +1,  0,
            +1,  0,  0, -1,
            -1,  0,  0, +1,
            0, +1, -1,  0
        };

        previous_state = current_state;
        bool clk_state = gpio_get(clk);
        bool dt_state  = gpio_get(dt);
        horz = gpio_get(HORZ_SWITCH);
        sig2 = gpio_get(SIGNAL2_SWITCH);

        if (clk == SHIFT_CLK) {
            if(lookup[transition] == 0) {
                return;
            }
            int shift = graph_shift * lookup[transition];

            if (horz && sig2)
                horz2_pos += shift;
            else if (horz && !sig2)
                horz1_pos += shift;
            else if (!horz && sig2)
                vert2_pos += shift;
            else
                vert1_pos += shift;

        }

        else if (clk == SCALE_CLK) {
            float scale;
            if(lookup[transition] == 1) {
                scale = graph_scale;
            } else if(lookup[transition] == -1) {
                scale = 1.0f / graph_scale;
            } else {
                return;
            }
                
            if (horz && sig2)
                horz2_stretch *= scale;

            else if (horz && !sig2)
                horz1_stretch *= scale;

            else if (!horz && sig2)
                vert2_stretch *= scale;

            else
                vert1_stretch *= scale;
        }

        else if (clk == TRIG_CLK) {
            if(lookup[transition] == 0) {
                return;
            }
            float tshift = trig_shift * lookup[transition];
                if (sig2)
                    trig2_pos += tshift;
                else
                    trig1_pos += tshift;
        }
    }
};

// ============================================================
// Static member definition
// ============================================================

RotaryEncoder* RotaryEncoder::instances[30] = {nullptr};


// ============================================================
// Main
// ============================================================

int main() {

    stdio_init_all();


    // ========================================================
    // GPIO Definitions
    // ========================================================

    const uint LED_PIN = PICO_DEFAULT_LED_PIN;

    // ========================================================
    // GPIO Setup
    // ========================================================

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(HORZ_SWITCH);
    gpio_set_dir(HORZ_SWITCH, GPIO_IN);
    gpio_pull_down(HORZ_SWITCH);

    gpio_init(SIGNAL2_SWITCH);
    gpio_set_dir(SIGNAL2_SWITCH, GPIO_IN);
    gpio_pull_down(SIGNAL2_SWITCH);

    // ========================================================
    // Rotary Encoders
    // ========================================================

    RotaryEncoder shift(SHIFT_CLK, SHIFT_DT);
    RotaryEncoder scale(SCALE_CLK, SCALE_DT);
    RotaryEncoder trig(TRIG_CLK, TRIG_DT);

    // ========================================================
    // Main Loop
    // ========================================================

    while (true) {

        // ----------------------------------------------------
        // Counter
        // ----------------------------------------------------

        if (counter > 100)
            counter = 0;

        counter++;


        //Trigger logic: works but might need to be adjusted for integration
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

        if (counter == 100) {

            gpio_put(LED_PIN, !gpio_get(LED_PIN));

            printf("=================================\n");

            printf("Vertical 1 Position: %d\n", vert1_pos);
            printf("Horizontal 1 Position: %d\n", horz1_pos);

            printf("Vertical 2 Position: %d\n", vert2_pos);
            printf("Horizontal 2 Position: %d\n", horz2_pos);

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

        sleep_ms(5);
    }

    return 0;
}

void gpio_callback(uint gpio, uint32_t events) {
    RotaryEncoder::handle_gpio_irq(gpio);

}