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

// PPQN helpers
uint32_t seq_get_ppqn();

// Return number of steps in the sequence
uint32_t seq_get_steps();

// Note operations (MIDI note numbers 0-127)
uint8_t seq_get_note(uint32_t step);
void seq_set_note(uint32_t step, uint8_t note);
