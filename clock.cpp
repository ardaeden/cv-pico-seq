#include "clock.h"

#include "hardware/timer.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

namespace {
volatile uint64_t us_counter = 0;            // accumulates microseconds
volatile uint32_t clock_interval_us = 5000;  // target interval in microseconds
volatile uint64_t last_tick = 0;
volatile bool tick_flag = false;             // set by core1, consumed by core0

struct repeating_timer timer_state;

bool timer_callback(struct repeating_timer *t) {
    // Timer runs every 100 microseconds; accumulate real microseconds
    us_counter += 100;

    if (us_counter >= clock_interval_us) {
        last_tick = last_tick + us_counter;
        us_counter = 0;
        tick_flag = true; // notify core0 of a timing tick
    }
    return true;
}

void core1_main() {
    add_repeating_timer_us(-100, timer_callback, nullptr, &timer_state);
    while (true) {
        tight_loop_contents();
    }
}
} // namespace

void clock_set_bpm(uint32_t bpm) {
    uint64_t us_per_beat = 60000000ULL / (bpm ? bpm : 120);
    clock_interval_us = static_cast<uint32_t>(us_per_beat);
}

void clock_launch_core1() {
    multicore_launch_core1(core1_main);
}

bool clock_consume_tick() {
    if (!tick_flag) {
        return false;
    }
    tick_flag = false;
    return true;
}
