#pragma once

// Initialize GPIO for user inputs (buttons, encoders, etc.).
void io_init();

// Returns true when play/pause button was pressed (debounced, edge-triggered).
bool io_poll_play_toggle();

// Start LED blink (non-blocking)
void io_blink_led_start();

// Update LED state - call every loop iteration
void io_update_led();
