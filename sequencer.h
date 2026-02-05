#pragma once

#include <cstdint>

enum PatternLoadMode { LOAD_INSTANT, LOAD_WAIT_END };

void seq_init();

// Play state helpers
bool seq_toggle_play();
void seq_stop();
bool seq_is_playing();
void seq_set_playing(bool playing);

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
uint32_t seq_get_gate_mask();
void seq_toggle_gate(uint32_t step);

// Tie (Slur) operations
bool seq_get_tie(uint32_t step);
uint32_t seq_get_tie_mask();
void seq_set_tie(uint32_t step, bool tie);

// Velocity operations (0:pp, 1:p, 2:mf, 3:f, 4:ff)
uint8_t seq_get_velocity(uint32_t step);
void seq_set_velocity(uint32_t step, uint8_t velo_idx);

void seq_save_pattern_ram_only(uint8_t slot);
void seq_flush_all_patterns_to_eeprom();
bool seq_has_dirty_patterns();
void seq_load_pattern(uint8_t slot);

// Global Scale operations
int seq_get_global_scale();
void seq_set_global_scale(int scale_idx);
uint8_t seq_get_next_note_in_scale(uint8_t current_note, int delta);
uint8_t seq_quantize_note(uint8_t note);
int8_t seq_get_global_octave();
void seq_set_global_octave(int8_t octave);
int8_t seq_get_global_transpose();
void seq_set_global_transpose(int8_t semitones);
void seq_get_note_range(uint8_t &min_note, uint8_t &max_note);

void seq_randomize_gates(uint8_t density_percent);
void seq_reset_pattern();
int seq_get_num_scales();
const char *seq_get_scale_name(int scale_idx);

void seq_queue_pattern(uint8_t slot);
int8_t seq_get_pending_pattern();
void seq_init_flash();

PatternLoadMode seq_get_load_mode();
void seq_set_load_mode(PatternLoadMode mode);
