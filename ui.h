#pragma once

#include <cstdint>

// Initialize the SSD1306 display (I2C0, SDA=GP4, SCL=GP5, addr=0x3C)
void ui_init();

// Immediately update displayed BPM value (non-blocking)
void ui_show_bpm(uint32_t bpm);

// Display 16-step grid (current_step in [0..steps-1]).
// Shows 8 squares on top row and 8 on bottom; fills the current step square.
void ui_show_steps(uint32_t current_step, uint32_t steps);
