#include "sequencer.h"

#include <atomic>
#include <cstring>
#include "pico/stdlib.h"

namespace {
// Pattern storage in RAM (10 slots x 16 notes)
constexpr uint8_t NUM_PATTERN_SLOTS = 10;
constexpr uint8_t PATTERN_SIZE = 16;  // 16 notes per pattern

uint8_t pattern_storage[NUM_PATTERN_SLOTS][PATTERN_SIZE] = {0};

struct SequencerState {
    uint32_t bpm;
    uint32_t ppqn;
    uint32_t steps;
    uint32_t current_step;
    std::atomic<bool> playing;
    uint8_t notes[16];  // MIDI note numbers for each step
};

// Default: C major scale starting from C3 (MIDI 48)
static SequencerState state = {120, 4, 16, 15, false, 
    {48, 50, 52, 54, 55, 57, 59, 60, 62, 64, 66, 67, 69, 71, 72, 74}};
} // namespace

void seq_init() {
    state.bpm = 120;
    state.ppqn = 4;  // 4 pulses per quarter note for 16th notes per step
    state.steps = 16;
    state.current_step = 15;  // Start at 15 so first tick moves to step 0
    state.playing.store(false);
}

bool seq_toggle_play() {
    bool expected = state.playing.load();
    // Toggle atomically
    while (!state.playing.compare_exchange_weak(expected, !expected)) {
        // expected is updated by compare_exchange_weak on failure
    }
    bool is_playing = !expected;
    if (!is_playing) {
        // Reset to step 15 so next play starts at 0 after first tick
        state.current_step = 15;
    }
    return is_playing;
}

bool seq_is_playing() {
    return state.playing.load();
}

void seq_advance_step() {
    state.current_step = (state.current_step + 1) % (state.steps ? state.steps : 16);
}

uint32_t seq_current_step() {
    return state.current_step;
}

uint32_t seq_get_bpm() {
    return state.bpm;
}

void seq_set_bpm(uint32_t new_bpm) {
    state.bpm = new_bpm ? new_bpm : 120;
}

uint32_t seq_get_steps() {
    return state.steps;
}

uint8_t seq_get_note(uint32_t step) {
    if (step >= 16) step = 0;
    return state.notes[step];
}

void seq_set_note(uint32_t step, uint8_t note) {
    if (step >= 16) return;
    if (note > 127) note = 127;
    state.notes[step] = note;
}

void seq_init_flash() {
    // Initialize all pattern slots with default C major scale (C3-B4)
    uint8_t default_pattern[16] = {48, 50, 52, 54, 55, 57, 59, 60, 62, 64, 66, 67, 69, 71, 72, 74};
    for (int i = 0; i < NUM_PATTERN_SLOTS; ++i) {
        memcpy(pattern_storage[i], default_pattern, PATTERN_SIZE);
    }
}

void seq_save_pattern(uint8_t slot) {
    if (slot >= NUM_PATTERN_SLOTS) return;
    
    // Save current notes to RAM slot
    memcpy(pattern_storage[slot], state.notes, PATTERN_SIZE);
}

void seq_load_pattern(uint8_t slot) {
    if (slot >= NUM_PATTERN_SLOTS) return;
    
    // Load pattern from RAM slot
    memcpy(state.notes, pattern_storage[slot], PATTERN_SIZE);
}
