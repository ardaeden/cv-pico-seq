#pragma once

#include <cstdint>

void seq_init();

// Play state helpers
bool seq_toggle_play();
bool seq_is_playing();

// Step operations
void seq_advance_step();
uint32_t seq_current_step();

// Tempo helpers
uint32_t seq_get_bpm();
void seq_set_bpm(uint32_t bpm);

// Return number of steps in the sequence
uint32_t seq_get_steps();

// Note operations (MIDI note numbers 0-127)
uint8_t seq_get_note(uint32_t step);
void seq_set_note(uint32_t step, uint8_t note);

// Gate enable/disable operations
bool seq_get_gate_enabled(uint32_t step);  // Check if gate is enabled for step
void seq_set_gate_enabled(uint32_t step, bool enabled);  // Set gate enable state
void seq_toggle_gate(uint32_t step);  // Toggle gate enable state

// Pattern save/load operations (10 slots: 0-9)
void seq_save_pattern(uint8_t slot);    // Save current pattern to slot
void seq_load_pattern(uint8_t slot);    // Load pattern from slot immediately
void seq_queue_pattern(uint8_t slot);   // Queue pattern to load at next pattern boundary
void seq_init_flash();                   // Initialize pattern storage on boot
