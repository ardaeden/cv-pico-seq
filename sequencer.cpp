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

struct EucParams {
  uint32_t steps;
  uint32_t fills;
  int rotation;
  uint8_t probability;
  bool pending = false;
};
EucParams pending_euc;

struct SequencerState {
  uint32_t bpm;
  uint32_t steps;
  uint32_t current_step;
  std::atomic<bool> playing;
  uint8_t notes[32];
  uint8_t velocities[32]; // 0:pp, 1:p, 2:mf, 3:f, 4:ff
  uint32_t gate_mask;
  uint32_t tie_mask;
  std::atomic<int> global_scale_idx;
  int8_t global_octave;
  int8_t global_transpose;
  PatternLoadMode load_mode;
};

static SequencerState state = {
    120,
    32,
    31,
    false,
    {48, 50, 52, 54, 55, 57, 59, 60, 62, 64, 66, 67, 69, 71, 72, 74,
     48, 50, 52, 54, 55, 57, 59, 60, 62, 64, 66, 67, 69, 71, 72, 74},
    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    0xFFFFFFFF,
    0x00000000,
    8, // CHROMATIC
    0  // Default octave 0
};
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
  state.global_scale_idx.store(8); // CHROMATIC
  state.global_octave = 0;
  state.global_transpose = 0;
  state.load_mode = LOAD_WAIT_END;
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

void seq_set_playing(bool p) { state.playing.store(p); }

void seq_advance_step() {
  uint32_t prev_step = state.current_step;
  uint32_t steps = state.steps ? state.steps : 32;
  state.current_step = (state.current_step + 1) % steps;

  if (prev_step == (steps - 1) && state.current_step == 0) {
    if (pending_pattern_slot >= 0) {
      if (pending_pattern_slot < NUM_PATTERN_SLOTS) {
        memcpy(state.notes, pattern_storage[pending_pattern_slot],
               PATTERN_SIZE);
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

    if (pending_euc.pending) {
      seq_generate_euclidean(pending_euc.steps, pending_euc.fills,
                             pending_euc.rotation, pending_euc.probability);
      pending_euc.pending = false;
    }
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
        steps_storage[i] = 16;
      }
    }
  } else {
    for (int i = 0; i < NUM_PATTERN_SLOTS; ++i) {
      memset(pattern_storage[i], 60, PATTERN_SIZE); // C4
      memset(velocity_storage[i], 2, PATTERN_SIZE); // mf
      gate_mask_storage[i] = 0;
      tie_mask_storage[i] = 0;
      steps_storage[i] = 16; // Default to 16 steps
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
    state.steps = 16;
  }
  state.current_step = (state.steps > 0) ? (state.steps - 1) : 31;

  // Ensure global_octave/transpose is safe for the new pattern (DAC Range:
  // 36-84)
  uint8_t min_n, max_n;
  seq_get_note_range(min_n, max_n);
  int8_t current_oct = state.global_octave;
  int8_t current_tr = state.global_transpose;

  while (min_n + (current_oct * 12) + current_tr < 36 && current_oct < 10)
    current_oct++;
  while (max_n + (current_oct * 12) + current_tr > 84 && current_oct > -10)
    current_oct--;

  state.global_octave = current_oct;
}

void seq_reset_pattern() {
  state.gate_mask = 0;
  state.tie_mask = 0;
  state.steps = 16;
  for (int i = 0; i < 32; i++) {
    state.notes[i] = 60;     // C4
    state.velocities[i] = 2; // mf
  }
}

struct Scale {
  const char *name;
  uint16_t mask; // 12 bits for 12 semitones
};

static const Scale scales[] = {
    {"MAJOR (IONIAN)", 0b101011010101}, {"DORIAN", 0b101101010110},
    {"PHRYGIAN", 0b110101011010},       {"LYDIAN", 0b101010110101},
    {"MIXOLYDIAN", 0b101011010110},     {"MINOR (AEOLIAN)", 0b101101011010},
    {"LOCRIAN", 0b110101101010},        {"HARMONIC MINOR", 0b101101011001},
    {"MELODIC MINOR", 0b101101010101},  {"PHRYGIAN DOM", 0b110011011010},
    {"MAJ PENTATONIC", 0b101010010100}, {"MIN PENTATONIC", 0b100101010010},
    {"BLUES", 0b100101110010},          {"NEAPOLITAN MAJ", 0b110101010101},
    {"NEAPOLITAN MIN", 0b110101011001}, {"HIRAJOŞİ", 0b101100011000},
    {"IN-SEN", 0b110000101010},         {"IWATO", 0b110000110010},
    {"ARABIAN", 0b101011101010},        {"HUNGARIAN MIN", 0b101100111001},
    {"WHOLE TONE", 0b101010101010},     {"DIMINISHED", 0b101101101101},
    {"CHROMATIC", 0b111111111111}};

static const int num_scales = sizeof(scales) / sizeof(Scale);

int seq_get_num_scales() { return num_scales; }
const char *seq_get_scale_name(int scale_idx) {
  if (scale_idx < 0 || scale_idx >= num_scales)
    return "NONE";
  return scales[scale_idx].name;
}

void seq_queue_pattern(uint8_t slot) {
  if (slot >= NUM_PATTERN_SLOTS)
    return;
  pending_pattern_slot = slot;
}

int seq_get_global_scale() { return state.global_scale_idx.load(); }

void seq_set_global_scale(int scale_idx) {
  if (scale_idx >= 0 && scale_idx < num_scales) {
    state.global_scale_idx.store(scale_idx);
  }
}

uint8_t seq_get_next_note_in_scale(uint8_t current_note, int delta) {
  int scale_idx = state.global_scale_idx.load();
  if (scale_idx < 0 || scale_idx >= num_scales)
    return current_note;

  const Scale &s = scales[scale_idx];
  if (s.mask == 0b111111111111) { // Chromatic shortcut
    int n = (int)current_note + delta;
    if (n < 36)
      n = 36;
    if (n > 84)
      n = 84;
    return (uint8_t)n;
  }

  int dir = (delta > 0) ? 1 : -1;
  int abs_delta = (delta > 0) ? delta : -delta;
  uint8_t note = current_note;
  uint8_t root = 0;

  for (int i = 0; i < abs_delta; i++) {
    while (true) {
      if (dir > 0) {
        if (note >= 84)
          break;
        note++;
      } else {
        if (note <= 36)
          break;
        note--;
      }
      int rel = (note - root) % 12;
      if (s.mask & (1u << (11 - rel))) {
        break; // Found a note in scale
      }
    }
  }
  return note;
}

uint8_t seq_quantize_note(uint8_t note) {
  // Clamp to operative range
  uint8_t clamped = note;
  if (clamped < 36)
    clamped = 36;
  if (clamped > 84)
    clamped = 84;

  int scale_idx = state.global_scale_idx.load();
  if (scale_idx < 0 || scale_idx >= num_scales)
    return clamped;

  const Scale &s = scales[scale_idx];
  if (s.mask == 0b111111111111) { // Chromatic shortcut
    return clamped;
  }

  // Find nearest note in scale (rounding down)
  uint8_t root = 0;
  uint8_t current = clamped;

  while (current >= 36) {
    int rel = (current - root) % 12;
    if (s.mask & (1u << (11 - rel))) {
      return current;
    }
    current--;
  }

  // No note found downwards? Look upwards from range floor
  current = 36;
  while (current <= 84) {
    int rel = (current - root) % 12;
    if (s.mask & (1u << (11 - rel))) {
      return current;
    }
    current++;
  }
  return clamped;
}

int8_t seq_get_pending_pattern() { return pending_pattern_slot; }

PatternLoadMode seq_get_load_mode() { return state.load_mode; }

void seq_set_load_mode(PatternLoadMode mode) { state.load_mode = mode; }

void seq_set_global_octave(int8_t octave) {
  if (octave < -10)
    octave = -10;
  if (octave > 10)
    octave = 10;
  state.global_octave = octave;
}

int8_t seq_get_global_octave() { return state.global_octave; }

void seq_set_global_transpose(int8_t semitones) {
  if (semitones < -11)
    semitones = -11;
  if (semitones > 11)
    semitones = 11;
  state.global_transpose = semitones;
}

int8_t seq_get_global_transpose() { return state.global_transpose; }

void seq_get_note_range(uint8_t &min_note, uint8_t &max_note) {
  min_note = 255;
  max_note = 0;
  bool active = false;
  for (int i = 0; i < state.steps; ++i) {
    if (state.gate_mask & (1ULL << i)) {
      uint8_t n = state.notes[i];
      if (n < min_note)
        min_note = n;
      if (n > max_note)
        max_note = n;
      active = true;
    }
  }
  if (!active) {
    min_note = 60; // Default C4 if no notes
    max_note = 60;
  }
}

void seq_generate_euclidean(uint32_t steps, uint32_t fills, int rotation,
                            uint8_t probability) {
  if (steps == 0)
    return;
  if (steps > 32)
    steps = 32;
  if (fills > steps)
    fills = steps;

  bool pattern[32] = {false};
  int count = 0;

  // Bresenham-based Euclidean Distribution
  for (uint32_t i = 0; i < steps; i++) {
    count += (int)fills;
    if (count >= (int)steps) {
      pattern[i] = true;
      count -= (int)steps;
    }
  }

  // Clear current mask and apply rotated pattern
  state.gate_mask = 0;
  for (uint32_t i = 0; i < steps; i++) {
    int rotated_idx = (int)(i + rotation) % (int)steps;
    if (rotated_idx < 0)
      rotated_idx += (int)steps;
    if (pattern[rotated_idx]) {
      // Apply probability check
      if ((rand() % 100) < probability) {
        state.gate_mask |= (1UL << i);
      }
    }
  }
}

void seq_queue_euclidean(uint32_t steps, uint32_t fills, int rotation,
                         uint8_t probability) {
  pending_euc.steps = steps;
  pending_euc.fills = fills;
  pending_euc.rotation = rotation;
  pending_euc.probability = probability;
  pending_euc.pending = true;

  if (!state.playing.load()) {
    seq_generate_euclidean(steps, fills, rotation, probability);
    pending_euc.pending = false;
  }
}

void seq_evolve_pattern(uint8_t chaos, uint8_t walk_pct, uint8_t octave_limit) {
  if (chaos == 0)
    return;

  // Map 0-100% walk to 1-12 scale degrees
  int max_walk = 1 + (walk_pct * 11) / 100;

  // Get current scale info
  int scale_idx = seq_get_global_scale();

  for (uint32_t i = 0; i < state.steps; i++) {
    // Only evolve active gates
    if (!(state.gate_mask & (1UL << i)))
      continue;

    // Probability check
    if ((uint8_t)(rand() % 100) >= chaos)
      continue;

    // Mutate note by random walk in scale
    int delta = (rand() % (max_walk * 2 + 1)) - max_walk;
    if (delta == 0)
      delta = (rand() % 2) ? 1 : -1;

    uint8_t old_note = state.notes[i];
    uint8_t new_note = seq_get_next_note_in_scale(old_note, delta);

    // Apply octave limit relative to middle C (60)
    int min_allowed = 60 - (octave_limit * 12);
    int max_allowed = 60 + (octave_limit * 12);

    // Hard DAC boundaries
    if (min_allowed < 36)
      min_allowed = 36;
    if (max_allowed > 84)
      max_allowed = 84;

    if (new_note < min_allowed)
      new_note = seq_quantize_note(min_allowed);
    if (new_note > max_allowed)
      new_note = seq_quantize_note(max_allowed);

    state.notes[i] = new_note;
  }
}
