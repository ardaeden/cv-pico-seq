#include "clock.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "sequencer.h"

namespace {
volatile uint64_t us_counter = 0;
volatile uint32_t clock_interval_us = 5000;
volatile bool tick_flag = false;

constexpr uint GATE_PIN = 6;
constexpr uint CLOCK_OUT_PIN = 22;
volatile bool gate_active = false;
volatile uint64_t gate_start_us = 0;
volatile bool gate_enabled = false;
volatile bool clock_out_enabled = false;
volatile uint64_t gate_duration_us = 2500;

constexpr uint CLOCK_EXT_PIN = 21;
volatile ClockSource current_source = CLOCK_INTERNAL;

volatile uint32_t internal_tick_count = 0;
volatile bool step_advanced_flag = false;

constexpr uint8_t MIDI_BASE = 36;
constexpr float DAC_PER_SEMITONE = 4096.0f / 48.0f;

void handle_tick() {
  tick_flag = true;

  if (seq_is_playing()) {
    if (internal_tick_count % 6 == 0) {
      seq_advance_step();
      uint32_t step = seq_current_step();

      // Update CV on Core 1
      if (seq_get_gate_enabled(step)) {
        uint8_t midi_note = seq_get_note(step);
        int32_t semitones = (int32_t)midi_note - MIDI_BASE;
        int32_t dac_val = (int32_t)(semitones * DAC_PER_SEMITONE + 0.5f);
        if (dac_val < 0)
          dac_val = 0;
        if (dac_val > 0x0FFF)
          dac_val = 0x0FFF;

        // Set CV immediately
        uint32_t save = save_and_disable_interrupts();
        uint16_t command = 0x1000 | (dac_val & 0x0FFF);
        uint8_t buf[2] = {(uint8_t)(command >> 8), (uint8_t)(command & 0xFF)};
        gpio_put(17, false); // DAC_CS_PIN
        spi_write_blocking(spi0, buf, 2);
        gpio_put(17, true);
        restore_interrupts(save);

        // Trigger Gate
        if (gate_enabled) {
          gpio_put(GATE_PIN, true);
          gate_active = true;
          gate_start_us = time_us_64();
        }
      }
      step_advanced_flag = true;
    }

    internal_tick_count++;
    if (internal_tick_count >= 24)
      internal_tick_count = 0;
  }
}

constexpr uint DAC_CS_PIN = 17;

struct repeating_timer timer_state;

void external_clock_callback(uint gpio, uint32_t events) {
  if (current_source == CLOCK_EXTERNAL) {
    handle_tick();
  }
}

bool timer_callback(struct repeating_timer *t) {
  us_counter += 100;

  if (current_source == CLOCK_INTERNAL && us_counter >= clock_interval_us) {
    us_counter = 0;
    handle_tick();
  }

  // 24 PPQN Clock Out (50% Duty Cycle) - Only meaningful in internal mode
  if (current_source == CLOCK_INTERNAL && clock_out_enabled) {
    if (us_counter < clock_interval_us / 2) {
      gpio_put(CLOCK_OUT_PIN, true);
    } else {
      gpio_put(CLOCK_OUT_PIN, false);
    }
  } else {
    gpio_put(CLOCK_OUT_PIN, false);
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
  // Enable GPIO interrupt on Core 1 specifically
  gpio_set_irq_enabled_with_callback(CLOCK_EXT_PIN, GPIO_IRQ_EDGE_FALL, true,
                                     &external_clock_callback);

  // Core 1 loop with microsecond precision
  uint64_t last_us = time_us_64();
  while (true) {
    uint64_t now_us = time_us_64();
    uint64_t diff = now_us - last_us;

    // Simulate 100us timer tick
    if (diff >= 100) {
      last_us = now_us;
      timer_callback(NULL);
    }

    tight_loop_contents();
  }
}
} // namespace

void clock_set_source(ClockSource source) { current_source = source; }

ClockSource clock_get_source() { return current_source; }

bool clock_consume_step() {
  if (!step_advanced_flag)
    return false;
  step_advanced_flag = false;
  return true;
}

void clock_set_bpm(uint32_t bpm) {
  uint64_t us_per_quarter = 60000000ULL / (bpm ? bpm : 120);
  clock_interval_us = static_cast<uint32_t>(us_per_quarter / 24);
  // 50% duty cycle for a 16th note (one step = 6 ticks, so 3 ticks)
  gate_duration_us = clock_interval_us * 3;
}

void clock_init() {
  gpio_init(GATE_PIN);
  gpio_set_dir(GATE_PIN, GPIO_OUT);
  gpio_put(GATE_PIN, false);

  gpio_init(CLOCK_OUT_PIN);
  gpio_set_dir(CLOCK_OUT_PIN, GPIO_OUT);
  gpio_put(CLOCK_OUT_PIN, false);

  gpio_init(CLOCK_EXT_PIN);
  gpio_set_dir(CLOCK_EXT_PIN, GPIO_IN);
  gpio_pull_up(CLOCK_EXT_PIN);

  // SPI setup
  spi_init(spi0, 8000000);
  gpio_set_function(18, GPIO_FUNC_SPI);
  gpio_set_function(19, GPIO_FUNC_SPI);

  gpio_init(DAC_CS_PIN);
  gpio_set_dir(DAC_CS_PIN, GPIO_OUT);
  gpio_put(DAC_CS_PIN, true);
}

void clock_launch_core1() {
  // We'll setup the GPIO interrupt on the core that will handle it (Core 1)
  multicore_launch_core1(core1_main);
}

bool clock_consume_tick() {
  if (!tick_flag)
    return false;
  tick_flag = false;
  return true;
}

void clock_gate_enable(bool enable) { gate_enabled = enable; }

void clock_out_enable(bool enable) { clock_out_enabled = enable; }

void clock_set_cv(uint16_t dac_val) {
  uint32_t save = save_and_disable_interrupts();

  if (dac_val > 0x0FFF)
    dac_val = 0x0FFF;

  uint16_t command = 0x1000 | (dac_val & 0x0FFF);
  uint8_t buf[2] = {(uint8_t)(command >> 8), (uint8_t)(command & 0xFF)};

  gpio_put(DAC_CS_PIN, false);
  spi_write_blocking(spi0, buf, 2);
  gpio_put(DAC_CS_PIN, true);

  restore_interrupts(save);
}

void clock_restart() {
  uint32_t save = save_and_disable_interrupts();
  us_counter = 0;
  internal_tick_count = 0;
  tick_flag = false;
  step_advanced_flag = false;
  gpio_put(GATE_PIN, false);
  gate_active = false;
  restore_interrupts(save);
}
