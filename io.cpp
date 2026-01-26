#include "io.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include <atomic>

namespace {
constexpr uint BUTTON_PIN = 2;                 // GP2 - Play/Pause button
constexpr uint STOP_BUTTON_PIN = 7;            // GP7 - Stop button
constexpr uint STEP_BUTTON_PIN = 8;            // GP8 - Step count button
constexpr uint EDIT_BUTTON_PIN = 10;           // GP10 - Edit mode button
constexpr uint PATTERN_SELECT_BUTTON_PIN = 11; // GP11 - Pattern select button
constexpr uint SAVE_BUTTON_PIN = 12;           // GP12 - Save button
constexpr uint64_t DEBOUNCE_US = 20'000;       // 20 ms debounce is enough
constexpr uint LED_PIN = 3;                    // GP3
constexpr uint64_t LED_BLINK_DURATION_US = 20'000; // 20 ms LED on time

constexpr uint ENCODER_CLK = 14;
constexpr uint ENCODER_DATA = 15;
constexpr uint ENCODER_SW = 13;

struct ButtonTracker {
  uint pin;
  bool last_raw = true;
  bool debounced_state = false; // true = pressed
  uint64_t last_change_us = 0;
  bool edge_fell = false;

  void update() {
    bool current_raw = gpio_get(pin);
    uint64_t now = time_us_64();
    if (current_raw != last_raw) {
      last_change_us = now;
      last_raw = current_raw;
    }
    if ((now - last_change_us) >= DEBOUNCE_US) {
      bool next_debounced = !current_raw; // Active Low
      if (next_debounced && !debounced_state) {
        edge_fell = true; // Button just pressed
      }
      debounced_state = next_debounced;
    }
  }

  bool poll_press() {
    bool ret = edge_fell;
    edge_fell = false;
    return ret;
  }

  bool is_pressed() const { return debounced_state; }
};

ButtonTracker play_btn = {BUTTON_PIN};
ButtonTracker stop_btn = {STOP_BUTTON_PIN};
ButtonTracker step_btn = {STEP_BUTTON_PIN};
ButtonTracker edit_btn = {EDIT_BUTTON_PIN};
ButtonTracker pattern_btn = {PATTERN_SELECT_BUTTON_PIN};
ButtonTracker save_btn = {SAVE_BUTTON_PIN};
ButtonTracker encoder_btn = {ENCODER_SW};

bool led_blinking = false;
uint64_t led_blink_start_us = 0;

uint8_t encoder_prev_state = 0;
int8_t encoder_accum = 0;
std::atomic<int> encoder_pending{0};
constexpr int ENCODER_DETENT_STEPS = 4;

void encoder_gpio_irq(uint gpio, uint32_t events);

} // namespace

void io_init() {
  uint buttons[] = {
      BUTTON_PIN,      STOP_BUTTON_PIN,           STEP_BUTTON_PIN,
      EDIT_BUTTON_PIN, PATTERN_SELECT_BUTTON_PIN, SAVE_BUTTON_PIN};
  for (uint pin : buttons) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
  }

  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
}

bool io_poll_play_toggle() {
  play_btn.update();
  return play_btn.poll_press();
}
bool io_poll_edit_toggle() {
  edit_btn.update();
  return edit_btn.poll_press();
}
bool io_is_edit_button_pressed() {
  edit_btn.update();
  return edit_btn.is_pressed();
}
bool io_poll_pattern_select_button() {
  pattern_btn.update();
  return pattern_btn.poll_press();
}
bool io_poll_save_button() {
  save_btn.update();
  return save_btn.poll_press();
}
bool io_poll_stop_button() {
  stop_btn.update();
  return stop_btn.poll_press();
}
bool io_poll_step_button() {
  step_btn.update();
  return step_btn.poll_press();
}

bool io_is_step_button_pressed() {
  step_btn.update();
  return step_btn.is_pressed();
}
bool io_is_save_button_pressed() {
  save_btn.update();
  return save_btn.is_pressed();
}
bool io_is_pattern_select_button_pressed() {
  pattern_btn.update();
  return pattern_btn.is_pressed();
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

  bool clk = gpio_get(ENCODER_CLK);
  bool data = gpio_get(ENCODER_DATA);
  encoder_prev_state = (uint8_t)((clk << 1) | data);

  gpio_set_irq_enabled_with_callback(ENCODER_CLK,
                                     GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                     true, encoder_gpio_irq);
  gpio_set_irq_enabled_with_callback(ENCODER_DATA,
                                     GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                     true, encoder_gpio_irq);
}

int io_encoder_poll_delta() { return encoder_pending.exchange(0); }

namespace {
void encoder_gpio_irq(uint gpio, uint32_t events) {
  bool clk = gpio_get(ENCODER_CLK);
  bool data = gpio_get(ENCODER_DATA);
  uint8_t cur = (uint8_t)((clk << 1) | data);
  static const int8_t trans_table[16] = {0,  -1, 1, 0, 1, 0, 0,  -1,
                                         -1, 0,  0, 1, 0, 1, -1, 0};
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
  encoder_btn.update();
  return encoder_btn.poll_press();
}
