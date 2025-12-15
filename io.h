#pragma once

#include <cstdint>

// Initialize GPIO for user inputs (buttons, encoders, etc.).
void io_init();

// Returns true when play/pause button was pressed (debounced, edge-triggered).
bool io_poll_play_toggle();

// Start LED blink (non-blocking)
void io_blink_led_start();

// Update LED state - call every loop iteration
void io_update_led();

// Gate output on GP6: pulse for a step. Duration in microseconds.
void io_gate_pulse_us(uint64_t duration_us);
void io_update_gate();

// Initialize rotary encoder (GPIO14=CLK, GPIO15=DATA, GPIO13=SW)
void io_encoder_init();

// Poll encoder rotation delta (+1=CW, -1=CCW, 0=no change)
int io_encoder_poll_delta();

// Encoder switch (button) press - debounced edge (active-low)
bool io_encoder_button_pressed();

// (no suppression APIs; LED blink controlled by main loop)
