#include "sequencer.h"

#include "eeprom.h"
#include "pico/stdlib.h"
#include <atomic>
#include <cstring>

namespace {
constexpr uint8_t NUM_PATTERN_SLOTS = 10;
constexpr uint8_t PATTERN_SIZE = 32;

uint8_t pattern_storage[NUM_PATTERN_SLOTS][PATTERN_SIZE] = {0};
uint32_t gate_mask_storage[NUM_PATTERN_SLOTS] = {0};
uint8_t steps_storage[NUM_PATTERN_SLOTS] = {0};
bool pattern_dirty[NUM_PATTERN_SLOTS] = {false};
int8_t pending_pattern_slot = -1;

struct SequencerState {
  uint32_t bpm;
  uint32_t steps;
  uint32_t current_step;
  std::atomic<bool> playing;
  uint8_t notes[32];
  uint32_t gate_mask;
};

static SequencerState state = {120,
                               32,
                               31,
                               false,
                               {48, 50, 52, 54, 55, 57, 59, 60, 62, 64, 66,
                                67, 69, 71, 72, 74, 48, 50, 52, 54, 55, 57,
                                59, 60, 62, 64, 66, 67, 69, 71, 72, 74},
                               0xFFFFFFFF};
} // namespace

void seq_init() {
  state.bpm = 120;
  state.steps = 32;
  state.current_step = (state.steps > 0) ? (state.steps - 1) : 31;
  state.playing.store(false);
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
      state.gate_mask = gate_mask_storage[pending_pattern_slot];
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

void seq_init_flash() {
  eeprom_init();

  if (eeprom_is_initialized() && eeprom_has_valid_data()) {
    for (int i = 0; i < NUM_PATTERN_SLOTS; ++i) {
      eeprom_read_pattern(i, pattern_storage[i], &gate_mask_storage[i],
                          &steps_storage[i]);
      if (steps_storage[i] < 1 || steps_storage[i] > 32) {
        steps_storage[i] = 32;
      }
    }
  } else {
    // Pattern 0: Melodic Techno Rise (A Minor) - More movement
    uint8_t pattern0[32] = {45, 45, 57, 45, 48, 52, 45, 57, 45, 45, 57,
                            45, 50, 53, 45, 57, 45, 45, 57, 45, 52, 55,
                            45, 57, 53, 52, 50, 48, 47, 45, 43, 45};

    // Pattern 1: Funky Acid Bass (C Mixolydian) - Extra spice
    uint8_t pattern1[32] = {36, 48, 36, 36, 46, 36, 48, 46, 36, 43, 36,
                            46, 41, 36, 43, 46, 36, 48, 43, 36, 46, 36,
                            51, 36, 51, 50, 48, 46, 43, 41, 39, 36};

    // Pattern 2: Ambient Pentatonic Drift (G Major) - More contrast
    uint8_t pattern2[32] = {55, 59, 62, 55, 67, 69, 55, 62, 55, 59, 62,
                            55, 71, 74, 55, 62, 55, 59, 62, 74, 79, 81,
                            74, 67, 62, 59, 55, 62, 55, 59, 55, 55};

    // Pattern 3: Energetic Trance Lead (F# Minor) - Higher energy
    uint8_t pattern3[32] = {54, 61, 54, 66, 54, 61, 54, 69, 54, 61, 54,
                            73, 54, 61, 54, 69, 57, 61, 57, 69, 57, 61,
                            57, 72, 73, 71, 69, 66, 64, 61, 57, 54};

    // Pattern 4: Dark Synthwave Bass (D Minor) - More melodic
    uint8_t pattern4[32] = {38, 38, 50, 38, 41, 38, 50, 38, 45, 38, 50,
                            43, 38, 41, 38, 38, 38, 50, 38, 53, 38, 50,
                            38, 55, 50, 48, 45, 41, 43, 41, 40, 38};

    // Pattern 5: Polyrhythmic Euclidean (C Minor) - Sparser & cleaner
    uint8_t pattern5[32] = {48, 60, 48, 48, 51, 48, 48, 60, 48, 48, 53,
                            48, 48, 60, 48, 55, 48, 60, 48, 48, 51, 48,
                            48, 60, 63, 62, 60, 58, 55, 53, 51, 48};

    // Pattern 6: Chromatic Tension Lead - Fixed spacing
    uint8_t pattern6[32] = {60, 61, 63, 64, 66, 60, 61, 63, 67, 68, 70,
                            71, 73, 67, 68, 70, 60, 61, 63, 64, 66, 60,
                            61, 63, 74, 73, 72, 71, 70, 69, 68, 67};

    // Pattern 7: Uplifting Maj7 Arpeggios - More movement
    uint8_t pattern7[32] = {60, 64, 67, 71, 72, 67, 64, 60, 65, 69, 72,
                            76, 77, 72, 69, 65, 67, 71, 74, 78, 79, 74,
                            71, 67, 72, 76, 79, 83, 84, 79, 76, 72};

    // Pattern 8: Industrial Cyberpunk Bass (EBM Style) - DYNAMIC VERSION
    uint8_t pattern8[32] = {36, 36, 48, 36, 36, 39, 42, 36, 36, 36, 48,
                            36, 36, 47, 36, 48, 36, 36, 48, 36, 36, 39,
                            42, 36, 51, 50, 48, 47, 45, 43, 41, 39};

    // Pattern 9: The Grand Finale (Multi-Octave Melodic)
    uint8_t pattern9[32] = {60, 67, 64, 72, 67, 76, 72, 79, 60, 67, 64,
                            72, 67, 79, 72, 84, 65, 72, 69, 77, 72, 81,
                            77, 84, 67, 74, 71, 79, 74, 83, 79, 91};

    memcpy(pattern_storage[0], pattern0, PATTERN_SIZE);
    memcpy(pattern_storage[1], pattern1, PATTERN_SIZE);
    memcpy(pattern_storage[2], pattern2, PATTERN_SIZE);
    memcpy(pattern_storage[3], pattern3, PATTERN_SIZE);
    memcpy(pattern_storage[4], pattern4, PATTERN_SIZE);
    memcpy(pattern_storage[5], pattern5, PATTERN_SIZE);
    memcpy(pattern_storage[6], pattern6, PATTERN_SIZE);
    memcpy(pattern_storage[7], pattern7, PATTERN_SIZE);
    memcpy(pattern_storage[8], pattern8, PATTERN_SIZE);
    memcpy(pattern_storage[9], pattern9, PATTERN_SIZE);

    for (int i = 0; i < NUM_PATTERN_SLOTS; ++i) {
      gate_mask_storage[i] = 0xFFFFFFFF;
      steps_storage[i] = 32;
    }

    if (eeprom_is_initialized()) {
      for (int i = 0; i < NUM_PATTERN_SLOTS; ++i) {
        eeprom_write_pattern(i, pattern_storage[i], gate_mask_storage[i],
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
  gate_mask_storage[slot] = state.gate_mask;
  steps_storage[slot] = (uint8_t)state.steps;
  pattern_dirty[slot] = true;
}

void seq_flush_all_patterns_to_eeprom() {
  if (!eeprom_is_initialized())
    return;
  bool any_written = false;
  for (int i = 0; i < NUM_PATTERN_SLOTS; i++) {
    if (pattern_dirty[i]) {
      eeprom_write_pattern(i, pattern_storage[i], gate_mask_storage[i],
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
  state.gate_mask = gate_mask_storage[slot];
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
