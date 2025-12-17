#include "io.h"

#include "hardware/gpio.h"
#include <atomic>
#include "pico/stdlib.h"
#include "pico/time.h"

namespace {
constexpr uint BUTTON_PIN = 2;            // GP2 - Play button
constexpr uint EDIT_BUTTON_PIN = 10;      // GP10 - Edit mode button
constexpr uint SAVE_BUTTON_PIN = 11;      // GP11 - Save pattern button
constexpr uint LOAD_BUTTON_PIN = 12;      // GP12 - Load pattern button
constexpr uint64_t DEBOUNCE_US = 50'000;  // 50 ms debounce window
constexpr uint LED_PIN = 3;               // GP3
constexpr uint64_t LED_BLINK_DURATION_US = 50'000;  // 50 ms LED on time

constexpr uint ENCODER_CLK = 14;          // GP14
constexpr uint ENCODER_DATA = 15;         // GP15
constexpr uint ENCODER_SW = 13;           // GP13

bool button_prev = true;               // starts high because of pull-up
uint64_t last_button_event_us = 0;     // last time we toggled play state

bool edit_button_prev = true;          // edit button starts high (pull-up)
uint64_t last_edit_button_event_us = 0;

bool save_button_prev = true;          // save button starts high (pull-up)
uint64_t last_save_button_event_us = 0;

bool load_button_prev = true;          // load button starts high (pull-up)
uint64_t last_load_button_event_us = 0;

// Encoder switch debounce
bool encoder_sw_prev = true;
uint64_t last_encoder_sw_event_us = 0;

bool led_blinking = false;             // LED blink state
uint64_t led_blink_start_us = 0;       // LED blink start timestamp


// Encoder state tracking (quadrature)
uint8_t encoder_prev_state = 0;        // combined CLK/DATA previous state
int8_t encoder_accum = 0;              // accumulate quadrature deltas per detent
std::atomic<int> encoder_pending{0};   // completed detent steps awaiting main loop
constexpr int ENCODER_DETENT_STEPS = 2; // transitions required per detent (lower = more sensitive)

// Forward-declare IRQ handler (defined later in another anonymous namespace block)
void encoder_gpio_irq(uint gpio, uint32_t events);

} // namespace

void io_init() {
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    gpio_init(EDIT_BUTTON_PIN);
    gpio_set_dir(EDIT_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(EDIT_BUTTON_PIN);

    gpio_init(SAVE_BUTTON_PIN);
    gpio_set_dir(SAVE_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(SAVE_BUTTON_PIN);

    gpio_init(LOAD_BUTTON_PIN);
    gpio_set_dir(LOAD_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(LOAD_BUTTON_PIN);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
}

bool io_poll_play_toggle() {
    bool button_now = gpio_get(BUTTON_PIN);
    uint64_t now_us = time_us_64();
    if (button_now != button_prev && (now_us - last_button_event_us) >= DEBOUNCE_US) {
        button_prev = button_now;
        last_button_event_us = now_us;
        if (!button_now) { // active-low press
            return true;
        }
    }
    return false;
}

bool io_poll_edit_toggle() {
    bool button_now = gpio_get(EDIT_BUTTON_PIN);
    uint64_t now_us = time_us_64();
    if (button_now != edit_button_prev && (now_us - last_edit_button_event_us) >= DEBOUNCE_US) {
        edit_button_prev = button_now;
        last_edit_button_event_us = now_us;
        if (!button_now) { // active-low press
            return true;
        }
    }
    return false;
}

bool io_poll_save_button() {
    bool button_now = gpio_get(SAVE_BUTTON_PIN);
    uint64_t now_us = time_us_64();
    if (button_now != save_button_prev && (now_us - last_save_button_event_us) >= DEBOUNCE_US) {
        save_button_prev = button_now;
        last_save_button_event_us = now_us;
        if (!button_now) { // active-low press
            return true;
        }
    }
    return false;
}

bool io_poll_load_button() {
    bool button_now = gpio_get(LOAD_BUTTON_PIN);
    uint64_t now_us = time_us_64();
    if (button_now != load_button_prev && (now_us - last_load_button_event_us) >= DEBOUNCE_US) {
        load_button_prev = button_now;
        last_load_button_event_us = now_us;
        if (!button_now) { // active-low press
            return true;
        }
    }
    return false;
}

void io_blink_led_start() {
    gpio_put(LED_PIN, true);
    led_blinking = true;
    led_blink_start_us = time_us_64();
}

void io_update_led() {
    if (led_blinking) {
        uint64_t now_us = time_us_64();
        if ((now_us - led_blink_start_us) >= LED_BLINK_DURATION_US) {
            gpio_put(LED_PIN, false);
            led_blinking = false;
        }
    }
}

void io_encoder_init() {
    gpio_init(ENCODER_CLK);
    gpio_set_dir(ENCODER_CLK, GPIO_IN);
    gpio_pull_up(ENCODER_CLK);

    gpio_init(ENCODER_DATA);
    gpio_set_dir(ENCODER_DATA, GPIO_IN);
    gpio_pull_up(ENCODER_DATA);

    gpio_init(ENCODER_SW);
    gpio_set_dir(ENCODER_SW, GPIO_IN);
    gpio_pull_up(ENCODER_SW);

    // Initialize previous combined state (CLK<<1 | DATA)
    bool clk = gpio_get(ENCODER_CLK);
    bool data = gpio_get(ENCODER_DATA);
    encoder_prev_state = (uint8_t)((clk << 1) | data);
    encoder_sw_prev = gpio_get(ENCODER_SW);

    // Install IRQ callback for both encoder pins (handle in ISR for robustness)
    // Callback will run on GPIO edge and accumulate quadrature transitions.
    gpio_set_irq_enabled_with_callback(ENCODER_CLK, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, encoder_gpio_irq);
    gpio_set_irq_enabled_with_callback(ENCODER_DATA, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, encoder_gpio_irq);
}

int io_encoder_poll_delta() {
    // Return any accumulated detent steps from ISR (atomic)
    int pending = encoder_pending.exchange(0);
    return pending;
}

namespace {
// GPIO IRQ handler implemented after poll function to keep top-level flow readable.
void encoder_gpio_irq(uint gpio, uint32_t events) {
    bool clk = gpio_get(ENCODER_CLK);
    bool data = gpio_get(ENCODER_DATA);
    uint8_t cur = (uint8_t)((clk << 1) | data);

    // State transition table for quadrature decoding
    static const int8_t trans_table[16] = {
        0, -1,  1,  0,
        1,  0,  0, -1,
       -1,  0,  0,  1,
        0,  1, -1,  0
    };

    uint8_t idx = (uint8_t)((encoder_prev_state << 2) | cur);
    int8_t delta = trans_table[idx & 0x0F];
    encoder_prev_state = cur;

    if (delta != 0) {
        encoder_accum += delta;
        if (encoder_accum >= ENCODER_DETENT_STEPS) {
            encoder_accum = 0;
            encoder_pending.fetch_add(1);
        } else if (encoder_accum <= -ENCODER_DETENT_STEPS) {
            encoder_accum = 0;
            encoder_pending.fetch_sub(1);
        }
    }
}
} // namespace

bool io_encoder_button_pressed() {
    bool sw_now = gpio_get(ENCODER_SW);
    uint64_t now_us = time_us_64();
    if (sw_now != encoder_sw_prev && (now_us - last_encoder_sw_event_us) >= DEBOUNCE_US) {
        encoder_sw_prev = sw_now;
        last_encoder_sw_event_us = now_us;
        if (!sw_now) { // active-low press
            return true;
        }
    }
    return false;
}
