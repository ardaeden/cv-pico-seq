#include "clock.h"

#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

namespace {
volatile uint64_t us_counter = 0;            // accumulates microseconds
volatile uint32_t clock_interval_us = 5000;  // target interval in microseconds
volatile bool tick_flag = false;             // set by core1, consumed by core0
volatile uint32_t ppqn = 4;                  // pulses per quarter note

// Gate control (managed by core1)
constexpr uint GATE_PIN = 6;
volatile bool gate_active = false;
volatile uint64_t gate_start_us = 0;
volatile uint64_t gate_duration_us = 0;
volatile bool gate_enabled = false;  // Set by core 0 when playing

struct repeating_timer timer_state;

bool timer_callback(struct repeating_timer *t) {
    // Timer runs every 100 microseconds; accumulate real microseconds
    us_counter += 100;

    if (us_counter >= clock_interval_us) {
        us_counter = 0;
        tick_flag = true; // notify core0 of a timing tick
        
        // Start gate automatically on each tick (50% duty cycle) - only when enabled
        if (gate_enabled && !gate_active) {
            gpio_put(GATE_PIN, true);
            gate_active = true;
            gate_start_us = time_us_64();
            gate_duration_us = clock_interval_us / 2;
        }
    }
    
    // Check if gate duration elapsed (checked every 100us callback)
    if (gate_active) {
        uint64_t now_us = time_us_64();
        if ((now_us - gate_start_us) >= gate_duration_us) {
            gpio_put(GATE_PIN, false);
            gate_active = false;
        }
    }
    
    return true;
}

void core1_main() {
    // Initialize gate pin on core 1
    gpio_init(GATE_PIN);
    gpio_set_dir(GATE_PIN, GPIO_OUT);
    gpio_put(GATE_PIN, false);
    
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
}

void clock_set_ppqn(uint32_t new_ppqn) {
    ppqn = new_ppqn ? new_ppqn : 4;
    // Recalculate interval if needed, but since bpm might change, better to call set_bpm after
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

uint32_t clock_get_interval_us() {
    return clock_interval_us;
}

void clock_gate_enable(bool enable) {
    gate_enabled = enable;
    if (!enable && gate_active) {
        // Turn off gate immediately when disabled
        gpio_put(GATE_PIN, false);
        gate_active = false;
    }
}
