#include "clock.h"

#include "hardware/timer.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <atomic>

namespace {
volatile uint64_t us_counter = 0;            // accumulates microseconds
volatile uint32_t clock_interval_us = 5000;  // target interval in microseconds
volatile uint64_t last_tick = 0;
std::atomic<uint32_t> tick_count{0};         // number of pending ticks produced by core1
volatile uint32_t ppqn = 4;                  // pulses per quarter note

struct repeating_timer timer_state;

bool timer_callback(struct repeating_timer *t) {
    // Timer runs every 100 microseconds; accumulate real microseconds
    us_counter += 100;
    // Produce one or more ticks if we've accumulated enough microseconds.
    // Use a loop so we don't lose ticks if clock_interval_us is small.
    while (us_counter >= clock_interval_us && clock_interval_us > 0) {
        us_counter -= clock_interval_us;
        last_tick = last_tick + clock_interval_us;
        tick_count.fetch_add(1, std::memory_order_relaxed);
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
    // BPM = quarter notes per minute
    // PPQN = pulses per quarter note
    // microseconds per pulse = (60e6 / BPM) / PPQN
    uint64_t us_per_quarter = 60000000ULL / (bpm ? bpm : 120);
    clock_interval_us = static_cast<uint32_t>(us_per_quarter / ppqn);
    // Reset accumulator to avoid immediate leftover partial interval causing jitter
    us_counter = 0;
    // Clear pending ticks so changes apply cleanly
    tick_count.store(0, std::memory_order_relaxed);
}

void clock_set_ppqn(uint32_t new_ppqn) {
    ppqn = new_ppqn ? new_ppqn : 4;
    // Recalculate interval if needed, but since bpm might change, better to call set_bpm after
}

void clock_launch_core1() {
    multicore_launch_core1(core1_main);
}

bool clock_consume_tick() {
    uint32_t c = tick_count.load(std::memory_order_relaxed);
    if (c == 0) return false;
    // decrement one tick atomically
    tick_count.fetch_sub(1, std::memory_order_relaxed);
    return true;
}

uint32_t clock_get_interval_us() {
    return clock_interval_us;
}
