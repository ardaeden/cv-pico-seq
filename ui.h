#pragma once

#include "clock.h"
#include <cstdint>

enum TransportState { TSTATE_STOP, TSTATE_PLAY, TSTATE_PAUSE };

// Initialize the SSD1306 display (I2C0, SDA=GP4, SCL=GP5, addr=0x3C)
void ui_init();

// Boot animation: digital noise sweep, logo reveal, and glitch effects
void ui_boot_animation();

// Clear display framebuffer
void ui_clear();

// Immediately update displayed BPM value and pattern slot (non-blocking)
void ui_show_bpm(uint32_t bpm, uint8_t pattern_slot, ClockSource clock_source,
                 TransportState tstate = TSTATE_STOP, bool blink_slot = false,
                 bool bpm_inverted = false);

// Display 16-step grid (current_step in [0..steps-1]).
// Shows 8 squares on top row and 8 on bottom; fills the current step square.
void ui_show_steps(uint32_t current_step, uint32_t steps);

// Display edit mode: step selection
void ui_show_edit_step(uint32_t selected_step, uint8_t note);

// Display edit mode: note/velocity editing
void ui_show_edit_note(uint32_t step, uint8_t note, uint8_t velocity,
                       bool edit_velocity = false);

// HUD for quick adjustments
void ui_show_velocity_hud(uint8_t velocity);

// Show Pattern Select Screen
void ui_show_pattern_select(uint8_t current_slot);

// Show Settings Screen
void ui_show_settings(int current_option, ClockSource clock_source,
                      uint8_t gate_length, bool edit_mode = false);

// Low-level drawing functions for custom animations
void clear_region(int x, int y, int w, int h);
void draw_scaled_char(int x0, int y0, char c, int scale);
void draw_scaled_text(int x, int y, const char *text, int scale);
void ssd1306_update();
