#pragma once

// Initialize GPIO for user inputs (buttons, encoders, etc.).
void io_init();

// Returns true when play/pause button was pressed (debounced, edge-triggered).
bool io_poll_play_toggle();

// Blink LED on GP3 for quarter note indication.
void io_blink_led();
