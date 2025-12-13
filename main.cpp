#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "pico/multicore.h"
#include <cstdio>

// Core1: precise tick generator; Core0: sequencer logic
volatile uint64_t us_counter = 0;            // accumulates microseconds
volatile uint32_t clock_interval_us = 5000;  // target interval in microseconds
volatile uint64_t last_tick = 0;
volatile bool tick_flag = false; // set by core1, consumed by core0

// Simple sequencer state (example placeholders)
volatile uint32_t bpm = 120;             // can be updated by core0
volatile uint32_t steps = 16;            // total steps
volatile uint32_t current_step = 0;      // advanced on ticks

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
    // Derive microsecond interval from BPM and PPQN
    // microseconds per quarter note = 60e6 / BPM
    // microseconds per pulse = (60e6 / BPM) / PPQN
    uint64_t us_per_beat = 60000000ULL / (bpm ? bpm : 120);
    clock_interval_us = (uint32_t)us_per_beat;

    struct repeating_timer timer;
    // Run hardware timer at 100us resolution; accumulate to desired beat interval
    add_repeating_timer_us(-100, timer_callback, NULL, &timer);
    while(true) {
        tight_loop_contents();
    }
}

int main() {
    stdio_init_all();

    // Launch core1 for timing
    multicore_launch_core1(core1_main);

    // Core0: sequencer loop consumes tick_flag and advances state
    while (true) {
        if (tick_flag) {
            // Clear flag with minimal contention
            tick_flag = false;

            // Advance step
            current_step = (current_step + 1) % (steps ? steps : 16);

            // TODO: Update CV/Gate outputs here (DAC, GPIO, etc.)
            // Placeholder: lightweight printf for debug (can be disabled)
            printf("Step: %u\n", (unsigned)current_step);
        }

        // Non-time-critical work can be done here: read inputs, update BPM, etc.
        tight_loop_contents();
    }
}