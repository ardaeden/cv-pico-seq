#pragma once

// Initialize GPIO for user inputs (buttons, encoders, etc.).
void io_init();

// Returns true when play/pause button was pressed (debounced, edge-triggered).
bool io_poll_play_toggle();

// Start LED blink (non-blocking)
void io_blink_led_start();

// Update LED state - call every loop iteration
void io_update_led();

// Initialize rotary encoder (GPIO14=CLK, GPIO15=DATA, GPIO13=SW)
void io_encoder_init();

// Poll encoder rotation delta (+1=CW, -1=CCW, 0=no change)
int io_encoder_poll_delta();
