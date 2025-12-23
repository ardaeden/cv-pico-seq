#pragma once

#include <cstdint>

// Initialize GPIO for user inputs (buttons, encoders, etc.).
void io_init();

// Returns true when play/pause button was pressed (debounced, edge-triggered).
bool io_poll_play_toggle();

// Returns true when edit mode button was pressed (debounced, edge-triggered).
bool io_poll_edit_toggle();

// Returns true when pattern select button (GP11) was pressed (debounced, edge-triggered).
bool io_poll_pattern_select_button();



// Returns true when save button (GP12) was pressed (debounced, edge-triggered).
bool io_poll_save_button();

// Returns true when stop button (GP7) was pressed (debounced, edge-triggered).
bool io_poll_stop_button();

// Returns true when step button (GP8) is currently pressed (held down).
bool io_is_step_button_pressed();

// Start LED blink (non-blocking)
void io_blink_led_start();

// Update LED state - call every loop iteration
void io_update_led();

// Initialize rotary encoder (GPIO14=CLK, GPIO15=DATA, GPIO13=SW)
void io_encoder_init();

// Poll encoder rotation delta (+1=CW, -1=CCW, 0=no change)
int io_encoder_poll_delta();

// Encoder switch (button) press - debounced edge (active-low)
bool io_encoder_button_pressed();
