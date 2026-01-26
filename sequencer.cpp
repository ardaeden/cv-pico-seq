#include "sequencer.h"

#include "eeprom.h"
#include "pico/stdlib.h"
#include <atomic>
#include <cstdlib>
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
    // Pattern 0: Acid Rush (C Minor) - High Energy, Heavy Slides
    uint8_t p0[32] = {36, 48, 36, 36, 48, 37, 39, 36, 36, 48, 48,
                      36, 51, 48, 46, 36, 36, 48, 36, 36, 48, 37,
                      39, 36, 51, 50, 48, 46, 43, 41, 39, 36};
    uint8_t v0[32] = {4, 2, 4, 1, 4, 2, 4, 1, 4, 2, 4, 1, 4, 2, 4, 1,
                      4, 2, 4, 1, 4, 2, 4, 1, 4, 4, 4, 4, 3, 3, 2, 2};
    uint32_t g0 = 0xAA55AA55;
    uint32_t t0 = 0x44114411;

    // Pattern 1: Techno Hammer (A Minor) - Rhythmic Accents
    uint8_t p1[32] = {45, 45, 57, 45, 45, 45, 57, 45, 48, 48, 60,
                      48, 50, 50, 62, 50, 45, 45, 57, 45, 45, 45,
                      57, 45, 52, 53, 55, 57, 52, 50, 48, 45};
    uint8_t v1[32] = {4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0,
                      4, 0, 4, 0, 4, 0, 4, 0, 4, 4, 4, 4, 2, 2, 1, 1};
    uint32_t g1 = 0xAAAAAAAA;
    uint32_t t1 = 0x00000000;

    // Pattern 2: Glitch Bounce (Wide Range) - Irregular
    uint8_t p2[32] = {36, 72, 48, 60, 36, 72, 48, 60, 39, 75, 51,
                      63, 42, 78, 54, 66, 36, 38, 40, 42, 44, 46,
                      48, 50, 52, 54, 56, 58, 60, 62, 64, 66};
    uint8_t v2[32] = {4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1,
                      2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4};
    uint32_t g2 = 0x92492492;
    uint32_t t2 = 0x00000000;

    // Pattern 3: Cyberpunk Driving (D# Minor) - Aggressive
    uint8_t p3[32] = {39, 39, 39, 42, 39, 39, 39, 44, 39, 39, 39,
                      46, 39, 39, 44, 42, 39, 39, 39, 42, 39, 39,
                      39, 44, 51, 51, 44, 44, 42, 42, 41, 39};
    uint8_t v3[32] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                      4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 2, 2, 1, 1};
    uint32_t g3 = 0xFFFFFFFF;
    uint32_t t3 = 0xEEEEEEEE;

    // Pattern 4: Arp Lightning (E Minor) - Fast Ascent
    uint8_t p4[32] = {40, 43, 47, 52, 52, 47, 43, 40, 40, 43, 47,
                      52, 55, 52, 47, 43, 52, 55, 59, 64, 64, 59,
                      55, 52, 64, 67, 71, 76, 76, 71, 67, 64};
    uint8_t v4[32] = {2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
                      3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
    uint32_t g4 = 0xFFFFFFFF;
    uint32_t t4 = 0x00000000;

    // Pattern 5: Industrial Sub (Low-end focus)
    uint8_t p5[32] = {24, 24, 36, 24, 24, 24, 36, 24, 27, 27, 39,
                      27, 24, 24, 24, 24, 24, 24, 36, 24, 24, 24,
                      36, 24, 31, 31, 30, 30, 29, 29, 28, 24};
    uint8_t v5[32] = {4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1,
                      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
    uint32_t g5 = 0x88888888;
    uint32_t t5 = 0x77777777;

    // Pattern 6: Metallic Stabs (High Frequency)
    uint8_t p6[32] = {72, 84, 75, 87, 72, 84, 75, 87, 73, 85, 76,
                      88, 73, 85, 76, 88, 72, 73, 72, 73, 75, 76,
                      75, 76, 72, 72, 72, 72, 84, 84, 84, 84};
    uint8_t v6[32] = {4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2,
                      4, 2, 4, 2, 4, 2, 4, 2, 4, 0, 4, 0, 4, 4, 4, 4};
    uint32_t g6 = 0x55555555;
    uint32_t t6 = 0x00000000;

    // Pattern 7: Syncopated Groove (F# Minor) - Off-beat
    uint8_t p7[32] = {42, 42, 54, 42, 42, 54, 42, 42, 45, 45, 57,
                      45, 54, 54, 42, 42, 42, 42, 54, 42, 42, 54,
                      42, 42, 47, 49, 50, 52, 54, 52, 50, 42};
    uint8_t v7[32] = {4, 0, 4, 4, 0, 4, 0, 4, 4, 0, 4, 4, 0, 4, 0, 4,
                      4, 0, 4, 4, 0, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4};
    uint32_t g7 = 0xED7B6D7B;
    uint32_t t7 = 0x00000000;

    // Pattern 8: Vertical Drop (4-Octave Descent)
    uint8_t p8[32] = {84, 72, 60, 48, 84, 72, 60, 48, 86, 74, 62,
                      50, 86, 74, 62, 50, 88, 76, 64, 52, 88, 76,
                      64, 52, 90, 78, 66, 54, 42, 36, 30, 24};
    uint8_t v8[32] = {4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3,
                      4, 3, 2, 1, 0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4};
    uint32_t g8 = 0x99999999;
    uint32_t t8 = 0x00000000;

    // Pattern 9: Chaotic Storm (Full Intensity)
    uint8_t p9[32] = {36, 48, 60, 72, 84, 96, 84, 72, 37, 49, 61,
                      73, 85, 97, 85, 73, 38, 50, 62, 74, 86, 98,
                      86, 74, 39, 41, 43, 45, 47, 49, 51, 53};
    uint8_t v9[32] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
    uint32_t g9 = 0xFFFFFFFF;
    uint32_t t9 = 0xAAAAAAAA;

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

void seq_randomize_gates(uint8_t density_percent) {
  for (int i = 0; i < 32; i++) {
    bool on = (rand() % 100) < density_percent;
    if (on) {
      state.gate_mask |= (1u << i);
    } else {
      state.gate_mask &= ~(1u << i);
    }
  }
}

void seq_clear_gates() { state.gate_mask = 0; }

struct Scale {
  const char *name;
  uint16_t mask; // 12 bits for 12 semitones
};

static const Scale scales[] = {
    {"MAJOR (IONIAN)", 0b101010110101},  // W-W-H-W-W-W-H
    {"MINOR (AEOLIAN)", 0b101101011010}, // W-H-W-W-H-W-W
    {"DORIAN", 0b101101011011},          // W-H-W-W-W-H-W
    {"PHRYGIAN", 0b110101011010},        // H-W-W-W-H-W-W
    {"LYDIAN", 0b101010101101},          // W-W-W-H-W-W-H
    {"MIXOLYDIAN", 0b110101010101},      // W-W-H-W-W-H-W
    {"LOCRIAN", 0b110101101010},         // H-W-W-H-W-W-W
    {"PENTATONIC MAJ", 0b101010100101},  // W-W-m3-W-m3
    {"CHROMATIC", 0b111111111111}};

static const int num_scales = sizeof(scales) / sizeof(Scale);

int seq_get_num_scales() { return num_scales; }
const char *seq_get_scale_name(int scale_idx) {
  if (scale_idx < 0 || scale_idx >= num_scales)
    return "NONE";
  return scales[scale_idx].name;
}

void seq_randomize_pitches(int scale_idx, uint8_t root_note) {
  if (scale_idx < 0 || scale_idx >= num_scales)
    return;
  const Scale &s = scales[scale_idx];

  // Possible notes in the scale across 3 octaves
  uint8_t scale_notes[36];
  int count = 0;
  for (int note = root_note; note < root_note + 36 && note < 128; note++) {
    int rel = (note - root_note) % 12;
    if (s.mask & (1u << (11 - rel))) {
      scale_notes[count++] = note;
    }
  }

  if (count == 0)
    return;

  for (int i = 0; i < 32; i++) {
    state.notes[i] = scale_notes[rand() % count];
  }
}

void seq_queue_pattern(uint8_t slot) {
  if (slot >= NUM_PATTERN_SLOTS)
    return;
  pending_pattern_slot = slot;
}

int8_t seq_get_pending_pattern() { return pending_pattern_slot; }
