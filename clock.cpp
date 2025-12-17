#include "clock.h"

#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

namespace {
volatile uint64_t us_counter = 0;
volatile uint32_t clock_interval_us = 5000;
volatile bool tick_flag = false;

constexpr uint GATE_PIN = 6;
volatile bool gate_active = false;
volatile uint64_t gate_start_us = 0;
volatile uint64_t gate_duration_us = 0;
volatile bool gate_enabled = false;

constexpr uint DAC_CS_PIN = 17;

struct repeating_timer timer_state;

bool timer_callback(struct repeating_timer *t) {
    us_counter += 100;

    if (us_counter >= clock_interval_us) {
        us_counter = 0;
        tick_flag = true;
        
        if (gate_enabled && !gate_active) {
            gpio_put(GATE_PIN, true);
            gate_active = true;
            gate_start_us = time_us_64();
            gate_duration_us = clock_interval_us / 2;
        }
    }
    
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
    gpio_init(GATE_PIN);
    gpio_set_dir(GATE_PIN, GPIO_OUT);
    gpio_put(GATE_PIN, false);
    
    spi_init(spi0, 8000000);
    gpio_set_function(18, GPIO_FUNC_SPI);
    gpio_set_function(19, GPIO_FUNC_SPI);
    
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
    uint64_t us_per_quarter = 60000000ULL / (bpm ? bpm : 120);
    clock_interval_us = static_cast<uint32_t>(us_per_quarter / 4);
}

void clock_launch_core1() {
    multicore_launch_core1(core1_main);
}

bool clock_consume_tick() {
    if (!tick_flag) return false;
    tick_flag = false;
    return true;
}

void clock_gate_enable(bool enable) {
    gate_enabled = enable;
}

void clock_set_cv(uint16_t dac_val) {
    uint32_t save = save_and_disable_interrupts();
    
    if (dac_val > 0x0FFF) dac_val = 0x0FFF;
    
    uint16_t command = 0x1000 | (dac_val & 0x0FFF);
    uint8_t buf[2] = {(uint8_t)(command >> 8), (uint8_t)(command & 0xFF)};
    
    gpio_put(DAC_CS_PIN, false);
    spi_write_blocking(spi0, buf, 2);
    gpio_put(DAC_CS_PIN, true);
    
    restore_interrupts(save);
}
