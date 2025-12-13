#include "io.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

namespace {
constexpr uint BUTTON_PIN = 2;            // GP2
constexpr uint64_t DEBOUNCE_US = 20'000;  // 50 ms debounce window
constexpr uint LED_PIN = 3;               // GP3
constexpr uint64_t LED_BLINK_DURATION_US = 50'000;  // 50 ms LED on time

bool button_prev = true;               // starts high because of pull-up
uint64_t last_button_event_us = 0;     // last time we toggled play state

bool led_blinking = false;             // LED blink state
uint64_t led_blink_start_us = 0;       // LED blink start timestamp
} // namespace

void io_init() {
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

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
