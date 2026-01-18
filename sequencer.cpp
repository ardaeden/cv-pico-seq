#include "sequencer.h"

#include "eeprom.h"
#include "pico/stdlib.h"
#include <atomic>
#include <cstring>

namespace {
constexpr uint8_t NUM_PATTERN_SLOTS = 25;
constexpr uint8_t PATTERN_SIZE = 32;

uint8_t pattern_storage[NUM_PATTERN_SLOTS][PATTERN_SIZE] = {0};
uint8_t velocity_storage[NUM_PATTERN_SLOTS][PATTERN_SIZE] = {0};
uint32_t gate_mask_storage[NUM_PATTERN_SLOTS] = {0};
uint32_t tie_mask_storage[NUM_PATTERN_SLOTS] = {0};
uint8_t steps_storage[NUM_PATTERN_SLOTS] = {0};
bool pattern_dirty[NUM_PATTERN_SLOTS] = {false};
int8_t pending_pattern_slot = -1;

struct SequencerState {
  uint32_t bpm;
  uint32_t steps;
  uint32_t current_step;
  std::atomic<bool> playing;
  uint8_t notes[32];
  uint8_t velocities[32]; // 0:pp, 1:p, 2:mf, 3:f, 4:ff
  uint32_t gate_mask;
  uint32_t tie_mask;
};

static SequencerState state = {120,
                               32,
                               31,
                               false,
                               {48, 50, 52, 54, 55, 57, 59, 60, 62, 64, 66,
                                67, 69, 71, 72, 74, 48, 50, 52, 54, 55, 57,
                                59, 60, 62, 64, 66, 67, 69, 71, 72, 74},
                               {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                                2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
                               0xFFFFFFFF,
                               0x00000000};
} // namespace

void seq_init() {
  state.bpm = 120;
  state.steps = 32;
  state.current_step = (state.steps > 0) ? (state.steps - 1) : 31;
  state.playing.store(false);
  for (int i = 0; i < 32; i++) {
    state.velocities[i] = 2; // mf
  }
  state.tie_mask = 0;
}

bool seq_toggle_play() {
  bool expected = state.playing.load();
  while (!state.playing.compare_exchange_weak(expected, !expected)) {
  }
  return !expected;
}

void seq_stop() {
  state.playing.store(false);
  state.current_step = (state.steps > 0) ? (state.steps - 1) : 31;
}

bool seq_is_playing() { return state.playing.load(); }

void seq_advance_step() {
  uint32_t prev_step = state.current_step;
  uint32_t steps = state.steps ? state.steps : 32;
  state.current_step = (state.current_step + 1) % steps;

  if (prev_step == (steps - 1) && state.current_step == 0 &&
      pending_pattern_slot >= 0) {
    if (pending_pattern_slot < NUM_PATTERN_SLOTS) {
      memcpy(state.notes, pattern_storage[pending_pattern_slot], PATTERN_SIZE);
      memcpy(state.velocities, velocity_storage[pending_pattern_slot],
             PATTERN_SIZE);
      state.gate_mask = gate_mask_storage[pending_pattern_slot];
      state.tie_mask = tie_mask_storage[pending_pattern_slot];
      state.steps = steps_storage[pending_pattern_slot];
      if (state.steps < 1 || state.steps > 32) {
        state.steps = 32;
      }
    }
    pending_pattern_slot = -1;
  }
}

uint32_t seq_current_step() { return state.current_step; }

uint32_t seq_get_bpm() { return state.bpm; }

void seq_set_bpm(uint32_t new_bpm) { state.bpm = new_bpm ? new_bpm : 120; }

uint32_t seq_get_steps() { return state.steps; }

void seq_set_steps(uint32_t steps) {
  if (steps < 1)
    steps = 1;
  if (steps > 32)
    steps = 32;
  state.steps = steps;
}

uint8_t seq_get_note(uint32_t step) {
  if (step >= 32)
    step = 0;
  return state.notes[step];
}

void seq_set_note(uint32_t step, uint8_t note) {
  if (step >= 32)
    return;
  if (note > 127)
    note = 127;
  state.notes[step] = note;
}

bool seq_get_gate_enabled(uint32_t step) {
  if (step >= 32)
    return false;
  return (state.gate_mask & (1UL << step)) != 0;
}

uint32_t seq_get_gate_mask() { return state.gate_mask; }

void seq_toggle_gate(uint32_t step) {
  if (step >= 32)
    return;
  state.gate_mask ^= (1UL << step);
}

uint8_t seq_get_velocity(uint32_t step) {
  if (step >= 32)
    step = 0;
  return state.velocities[step];
}

void seq_set_velocity(uint32_t step, uint8_t velo_idx) {
  if (step >= 32)
    return;
  state.velocities[step] = velo_idx;
}

bool seq_get_tie(uint32_t step) {
  if (step >= 32)
    return false;
  return (state.tie_mask & (1UL << step)) != 0;
}

uint32_t seq_get_tie_mask() { return state.tie_mask; }

void seq_set_tie(uint32_t step, bool tie) {
  if (step >= 32)
    return;
  if (tie) {
    state.tie_mask |= (1UL << step);
  } else {
    state.tie_mask &= ~(1UL << step);
  }
}

void seq_init_flash() {
  eeprom_init();

  // Temporary force reload for new pattern update - change to true to use
  // EEPROM again after first run
  if (eeprom_is_initialized() && eeprom_has_valid_data()) {
    for (int i = 0; i < NUM_PATTERN_SLOTS; ++i) {
      eeprom_read_pattern(i, pattern_storage[i], velocity_storage[i],
                          &gate_mask_storage[i], &tie_mask_storage[i],
                          &steps_storage[i]);
      if (steps_storage[i] < 1 || steps_storage[i] > 32) {
        steps_storage[i] = 32;
      }
    }
  } else {
    // Pattern 0: Classic Acid Bass (C Minor) - Heavy Slides/Ties
    uint8_t p0[32] = {36, 36, 48, 37, 39, 41, 41, 36, 36, 36, 48,
                      36, 51, 51, 48, 36, 36, 36, 48, 37, 39, 41,
                      41, 36, 51, 50, 48, 46, 43, 41, 39, 36};
    uint8_t v0[32] = {4, 2, 4, 1, 2, 4, 2, 1, 4, 2, 4, 1, 4, 2, 4, 1,
                      4, 2, 4, 1, 2, 4, 2, 1, 3, 3, 2, 2, 1, 1, 0, 0};
    uint32_t g0 = 0xAA55AA55;
    uint32_t t0 = 0x44114411;

    // Pattern 1: Melodic Techno Rise (A Minor)
    uint8_t p1[32] = {45, 57, 45, 48, 52, 45, 57, 60, 45, 57, 45,
                      53, 50, 45, 57, 64, 45, 57, 45, 48, 52, 45,
                      57, 60, 52, 53, 55, 57, 52, 50, 48, 45};
    uint8_t v1[32] = {2, 4, 1, 2, 4, 1, 2, 4, 2, 4, 1, 2, 4, 1, 2, 4,
                      2, 4, 1, 2, 4, 1, 2, 4, 3, 3, 3, 4, 2, 2, 1, 0};
    uint32_t g1 = 0xEEEEEEEE;
    uint32_t t1 = 0x01010101;

    // Pattern 2: Deep Dub Stabs (G Minor) - Long Ties
    uint8_t p2[32] = {43, 43, 43, 43, 50, 50, 50, 50, 46, 46, 46,
                      46, 55, 55, 55, 55, 43, 43, 43, 43, 50, 50,
                      50, 50, 46, 46, 48, 50, 51, 53, 55, 43};
    uint8_t v2[32] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 4, 3, 2, 1};
    uint32_t g2 = 0x11111111;
    uint32_t t2 = 0x77777777;

    // Pattern 3: Cyberpunk EBM (D# Minor) - Aggressive Ties
    uint8_t p3[32] = {39, 39, 51, 39, 42, 42, 42, 42, 39, 39, 51,
                      39, 46, 46, 46, 46, 39, 39, 51, 39, 44, 44,
                      44, 44, 39, 39, 51, 39, 42, 41, 40, 39};
    uint8_t v3[32] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 2, 1, 0};
    uint32_t g3 = 0xFFFFFFFF;
    uint32_t t3 = 0xCC00CC00;

    // Pattern 4: Ambient Shimmer (E Major Pentatonic)
    uint8_t p4[32] = {64, 68, 71, 76, 78, 80, 83, 88, 64, 68, 71,
                      76, 78, 80, 83, 88, 88, 83, 80, 78, 76, 71,
                      68, 64, 64, 64, 64, 64, 64, 64, 64, 64};
    uint8_t v4[32] = {0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1,
                      2, 2, 3, 3, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t g4 = 0x88888888;
    uint32_t t4 = 0x77777777;

    // Pattern 5: Industrial Sludge (Low Octaves)
    uint8_t p5[32] = {24, 25, 24, 24, 27, 24, 24, 31, 24, 25, 24,
                      24, 27, 24, 24, 36, 24, 24, 31, 31, 24, 24,
                      25, 25, 24, 25, 26, 27, 28, 29, 30, 31};
    uint8_t v5[32] = {4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2,
                      4, 4, 4, 4, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4};
    uint32_t g5 = 0xF0F0F0F0;
    uint32_t t5 = 0x00FF00FF;

    // Pattern 6: Minimal Glitch (Techno)
    uint8_t p6[32] = {60, 72, 60, 48, 60, 72, 60, 36, 60, 72, 60,
                      48, 60, 72, 60, 36, 60, 60, 60, 60, 72, 72,
                      72, 72, 48, 48, 48, 48, 36, 36, 36, 36};
    uint8_t v6[32] = {4, 1, 0, 1, 4, 1, 0, 1, 4, 1, 0, 1, 4, 1, 0, 1,
                      3, 0, 3, 0, 3, 0, 3, 0, 3, 0, 2, 0, 4, 0, 4, 0};
    uint32_t g6 = 0x11111111;
    uint32_t t6 = 0xAAAAAAAA;

    // Pattern 7: Uplifting Trance Arp (B Minor)
    uint8_t p7[32] = {59, 62, 66, 71, 74, 71, 66, 62, 59, 62, 66,
                      71, 74, 71, 66, 62, 59, 62, 66, 71, 74, 59,
                      62, 66, 71, 71, 74, 74, 78, 79, 81, 83};
    uint8_t v7[32] = {2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2, 2, 2,
                      2, 2, 2, 2, 4, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4};
    uint32_t g7 = 0xFFFFFFFF;
    uint32_t t7 = 0x33333333;

    // Pattern 8: Funky Bassline (F Mixolydian)
    uint8_t p8[32] = {41, 53, 41, 41, 48, 41, 41, 51, 41, 41, 53,
                      41, 41, 48, 51, 53, 41, 53, 41, 41, 48, 41,
                      41, 51, 53, 51, 48, 46, 45, 43, 41, 39};
    uint8_t v8[32] = {4, 1, 1, 4, 1, 1, 4, 2, 4, 1, 1, 4, 1, 1, 4, 2,
                      4, 1, 1, 4, 1, 1, 4, 2, 4, 4, 4, 2, 2, 1, 0, 0};
    uint32_t g8 = 0x99999999;
    uint32_t t8 = 0x88888888;

    // Pattern 9: The Grand Finale (Multi-Scale)
    uint8_t p9[32] = {36, 48, 60, 72, 84, 72, 60, 48, 38, 50, 62,
                      74, 86, 74, 62, 50, 40, 52, 64, 76, 88, 76,
                      64, 52, 41, 43, 45, 47, 48, 50, 52, 54};
    uint8_t v9[32] = {4, 3, 2, 1, 4, 3, 2, 1, 4, 3, 2, 1, 4, 3, 2, 1,
                      4, 3, 2, 1, 4, 3, 2, 1, 4, 4, 4, 4, 4, 4, 4, 4};
    uint32_t g9 = 0xAAAAAAAA;
    uint32_t t9 = 0x55555555;

    memcpy(pattern_storage[0], p0, 32);
    memcpy(velocity_storage[0], v0, 32);
    gate_mask_storage[0] = g0;
    tie_mask_storage[0] = t0;
    memcpy(pattern_storage[1], p1, 32);
    memcpy(velocity_storage[1], v1, 32);
    gate_mask_storage[1] = g1;
    tie_mask_storage[1] = t1;
    memcpy(pattern_storage[2], p2, 32);
    memcpy(velocity_storage[2], v2, 32);
    gate_mask_storage[2] = g2;
    tie_mask_storage[2] = t2;
    memcpy(pattern_storage[3], p3, 32);
    memcpy(velocity_storage[3], v3, 32);
    gate_mask_storage[3] = g3;
    tie_mask_storage[3] = t3;
    memcpy(pattern_storage[4], p4, 32);
    memcpy(velocity_storage[4], v4, 32);
    gate_mask_storage[4] = g4;
    tie_mask_storage[4] = t4;
    memcpy(pattern_storage[5], p5, 32);
    memcpy(velocity_storage[5], v5, 32);
    gate_mask_storage[5] = g5;
    tie_mask_storage[5] = t5;
    memcpy(pattern_storage[6], p6, 32);
    memcpy(velocity_storage[6], v6, 32);
    gate_mask_storage[6] = g6;
    tie_mask_storage[6] = t6;
    memcpy(pattern_storage[7], p7, 32);
    memcpy(velocity_storage[7], v7, 32);
    gate_mask_storage[7] = g7;
    tie_mask_storage[7] = t7;
    memcpy(pattern_storage[8], p8, 32);
    memcpy(velocity_storage[8], v8, 32);
    gate_mask_storage[8] = g8;
    tie_mask_storage[8] = t8;
    memcpy(pattern_storage[9], p9, 32);
    memcpy(velocity_storage[9], v9, 32);
    gate_mask_storage[9] = g9;
    tie_mask_storage[9] = t9;

    for (int i = 0; i < NUM_PATTERN_SLOTS; ++i) {
      if (i >= 10) {
        memset(pattern_storage[i], 60, PATTERN_SIZE); // C4
        memset(velocity_storage[i], 2, PATTERN_SIZE); // mf
        gate_mask_storage[i] = 0; // New patterns start empty
        tie_mask_storage[i] = 0;
      }
      steps_storage[i] = 32;
    }

    if (eeprom_is_initialized()) {
      for (int i = 0; i < NUM_PATTERN_SLOTS; ++i) {
        eeprom_write_pattern(i, pattern_storage[i], velocity_storage[i],
                             gate_mask_storage[i], tie_mask_storage[i],
                             steps_storage[i]);
      }
      eeprom_mark_valid();
    }
  }
}

void seq_save_pattern_ram_only(uint8_t slot) {
  if (slot >= NUM_PATTERN_SLOTS)
    return;
  memcpy(pattern_storage[slot], state.notes, PATTERN_SIZE);
  memcpy(velocity_storage[slot], state.velocities, PATTERN_SIZE);
  gate_mask_storage[slot] = state.gate_mask;
  tie_mask_storage[slot] = state.tie_mask;
  steps_storage[slot] = (uint8_t)state.steps;
  pattern_dirty[slot] = true;
}

void seq_flush_all_patterns_to_eeprom() {
  if (!eeprom_is_initialized())
    return;
  bool any_written = false;
  for (int i = 0; i < NUM_PATTERN_SLOTS; i++) {
    if (pattern_dirty[i]) {
      eeprom_write_pattern(i, pattern_storage[i], velocity_storage[i],
                           gate_mask_storage[i], tie_mask_storage[i],
                           steps_storage[i]);
      pattern_dirty[i] = false;
      any_written = true;
    }
  }
  if (any_written && !eeprom_has_valid_data()) {
    eeprom_mark_valid();
  }
}

bool seq_has_dirty_patterns() {
  for (int i = 0; i < NUM_PATTERN_SLOTS; i++) {
    if (pattern_dirty[i]) {
      return true;
    }
  }
  return false;
}

void seq_load_pattern(uint8_t slot) {
  if (slot >= NUM_PATTERN_SLOTS)
    return;

  memcpy(state.notes, pattern_storage[slot], PATTERN_SIZE);
  memcpy(state.velocities, velocity_storage[slot], PATTERN_SIZE);
  state.gate_mask = gate_mask_storage[slot];
  state.tie_mask = tie_mask_storage[slot];
  state.steps = steps_storage[slot];
  if (state.steps < 1 || state.steps > 32) {
    state.steps = 32;
  }
  state.current_step = (state.steps > 0) ? (state.steps - 1) : 31;
}

void seq_queue_pattern(uint8_t slot) {
  if (slot >= NUM_PATTERN_SLOTS)
    return;
  pending_pattern_slot = slot;
}

int8_t seq_get_pending_pattern() { return pending_pattern_slot; }
