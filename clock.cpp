#include "clock.h"

#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
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
volatile bool gate_enabled = false;

// DAC/CV control (managed by core1)
constexpr uint DAC_CS_PIN = 17;

struct repeating_timer timer_state;

bool timer_callback(struct repeating_timer *t) {
    // Timer runs every 100 microseconds; accumulate real microseconds
    us_counter += 100;

    if (us_counter >= clock_interval_us) {
        us_counter = 0;
        tick_flag = true;
        
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
    
    // Initialize SPI0 for DAC (8 MHz, mode 0)
    spi_init(spi0, 8000000);
    gpio_set_function(18, GPIO_FUNC_SPI);  // SCK
    gpio_set_function(19, GPIO_FUNC_SPI);  // MOSI
    
    // Initialize DAC CS pin
    gpio_init(DAC_CS_PIN);
    gpio_set_dir(DAC_CS_PIN, GPIO_OUT);
    gpio_put(DAC_CS_PIN, true);
    
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

void clock_set_cv(uint16_t dac_val) {
    // Disable interrupts to safely write to SPI from core 0
    uint32_t save = save_and_disable_interrupts();
    
    if (dac_val > 0x0FFF) dac_val = 0x0FFF;
    
    // Build MCP4822 command: channel A, 2X gain, active
    uint16_t command = 0x1000 | (dac_val & 0x0FFF);
    uint8_t buf[2] = {(uint8_t)(command >> 8), (uint8_t)(command & 0xFF)};
    
    gpio_put(DAC_CS_PIN, false);
    spi_write_blocking(spi0, buf, 2);
    gpio_put(DAC_CS_PIN, true);
    
    restore_interrupts(save);
}
