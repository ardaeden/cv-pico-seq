#pragma once

#include <cstdint>

void seq_init();

// Play state helpers
bool seq_toggle_play();
void seq_set_play(bool enable);
bool seq_is_playing();

// Step operations
void seq_advance_step();
uint32_t seq_current_step();

// Tempo helpers
uint32_t seq_get_bpm();
void seq_set_bpm(uint32_t bpm);

// PPQN helpers
uint32_t seq_get_ppqn();
void seq_set_ppqn(uint32_t ppqn);

// Return number of steps in the sequence
uint32_t seq_get_steps();
