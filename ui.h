#pragma once

#include <cstdint>

// Initialize the SSD1306 display (I2C0, SDA=GP4, SCL=GP5, addr=0x3C)
void ui_init();

// Boot animation (16 steps wave + pulsing effect)
void ui_boot_animation();

// Clear display framebuffer
void ui_clear();

// Immediately update displayed BPM value and pattern slot (non-blocking)
void ui_show_bpm(uint32_t bpm, uint8_t pattern_slot, bool blink_slot = false);

// Display 16-step grid (current_step in [0..steps-1]).
// Shows 8 squares on top row and 8 on bottom; fills the current step square.
void ui_show_steps(uint32_t current_step, uint32_t steps);

// Display edit mode: step selection
void ui_show_edit_step(uint32_t selected_step, uint8_t note);

// Display edit mode: note editing
void ui_show_edit_note(uint32_t step, uint8_t note);

// Display pattern select mode (slot 0-9)
void ui_show_pattern_select(uint8_t slot);

// Blink pattern number as save confirmation
void ui_pattern_select_blink_confirm(uint8_t slot);

// Low-level drawing functions for custom animations
void clear_region(int x, int y, int w, int h);
void draw_scaled_char(int x0, int y0, char c, int scale);
void ssd1306_update();
