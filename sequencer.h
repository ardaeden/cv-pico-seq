#pragma once

#include <cstdint>

void seq_init();

// Play state helpers
bool seq_toggle_play();
void seq_stop();
bool seq_is_playing();

// Step operations
void seq_advance_step();
uint32_t seq_current_step();

// Tempo helpers
uint32_t seq_get_bpm();
void seq_set_bpm(uint32_t bpm);

// Return number of steps in the sequence
uint32_t seq_get_steps();
void seq_set_steps(uint32_t steps);

// Note operations (MIDI note numbers 0-127)
uint8_t seq_get_note(uint32_t step);
void seq_set_note(uint32_t step, uint8_t note);

// Gate enable/disable operations
bool seq_get_gate_enabled(uint32_t step);
void seq_toggle_gate(uint32_t step);

// Pattern save/load operations (10 slots: 0-9)
void seq_save_pattern(uint8_t slot);
void seq_save_pattern_ram_only(uint8_t slot);
void seq_flush_all_patterns_to_eeprom();
bool seq_has_dirty_patterns();
void seq_load_pattern(uint8_t slot);
void seq_queue_pattern(uint8_t slot);
int8_t seq_get_pending_pattern();
void seq_init_flash();
