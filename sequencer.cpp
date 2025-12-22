#include "sequencer.h"

#include <atomic>
#include <cstring>
#include "pico/stdlib.h"
#include "eeprom.h"

namespace {
constexpr uint8_t NUM_PATTERN_SLOTS = 10;
constexpr uint8_t PATTERN_SIZE = 16;

uint8_t pattern_storage[NUM_PATTERN_SLOTS][PATTERN_SIZE] = {0};
uint16_t gate_mask_storage[NUM_PATTERN_SLOTS] = {0};
uint8_t steps_storage[NUM_PATTERN_SLOTS] = {0};
bool pattern_dirty[NUM_PATTERN_SLOTS] = {false};
int8_t pending_pattern_slot = -1;

struct SequencerState {
    uint32_t bpm;
    uint32_t steps;
    uint32_t current_step;
    std::atomic<bool> playing;
    uint8_t notes[16];
    uint16_t gate_mask;
};

static SequencerState state = {120, 16, 15, false, 
    {48, 50, 52, 54, 55, 57, 59, 60, 62, 64, 66, 67, 69, 71, 72, 74}, 0xFFFF};
} // namespace

void seq_init() {
    state.bpm = 120;
    state.steps = 16;
    state.current_step = 15;
    state.playing.store(false);
}

bool seq_toggle_play() {
    bool expected = state.playing.load();
    while (!state.playing.compare_exchange_weak(expected, !expected)) {}
    return !expected;
}

void seq_stop() {
    state.playing.store(false);
    state.current_step = (state.steps > 0) ? (state.steps - 1) : 15;
}

bool seq_is_playing() {
    return state.playing.load();
}

void seq_advance_step() {
    uint32_t prev_step = state.current_step;
    uint32_t steps = state.steps ? state.steps : 16;
    state.current_step = (state.current_step + 1) % steps;
    
    if (prev_step == (steps - 1) && state.current_step == 0 && pending_pattern_slot >= 0) {
        if (pending_pattern_slot < NUM_PATTERN_SLOTS) {
            memcpy(state.notes, pattern_storage[pending_pattern_slot], PATTERN_SIZE);
            state.gate_mask = gate_mask_storage[pending_pattern_slot];
            state.steps = steps_storage[pending_pattern_slot];
            if (state.steps < 1 || state.steps > 16) {
                state.steps = 16;
            }
        }
        pending_pattern_slot = -1;
    }
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

void seq_set_steps(uint32_t steps) {
    if (steps < 1) steps = 1;
    if (steps > 16) steps = 16;
    state.steps = steps;
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

bool seq_get_gate_enabled(uint32_t step) {
    if (step >= 16) return false;
    return (state.gate_mask & (1 << step)) != 0;
}

void seq_toggle_gate(uint32_t step) {
    if (step >= 16) return;
    state.gate_mask ^= (1 << step);
}

void seq_init_flash() {
    eeprom_init();
    
    // GEÇICI: EEPROM'u zorla sıfırla - yeni patternleri yüklemek için
    bool force_reset = false;  // Bu satırı sonra false yap veya sil
    
    if (eeprom_is_initialized() && eeprom_has_valid_data() && !force_reset) {
        for (int i = 0; i < NUM_PATTERN_SLOTS; ++i) {
            eeprom_read_pattern(i, pattern_storage[i], &gate_mask_storage[i], &steps_storage[i]);
            if (steps_storage[i] < 1 || steps_storage[i] > 16) {
                steps_storage[i] = 16;
            }
        }
    } else {
        // Pattern 0: C Major Scale (C3 to C4)
        uint8_t pattern0[16] = {48, 50, 52, 53, 55, 57, 59, 60, 59, 57, 55, 53, 52, 50, 48, 60};
        
        // Pattern 1: Minor Arpeggio (Am)
        uint8_t pattern1[16] = {57, 60, 64, 69, 64, 60, 57, 69, 57, 60, 64, 69, 72, 69, 64, 60};
        
        // Pattern 2: Pentatonic Sequence
        uint8_t pattern2[16] = {60, 62, 65, 67, 70, 72, 70, 67, 65, 62, 60, 67, 65, 70, 62, 72};
        
        // Pattern 3: Bass Line (Techno Style)
        uint8_t pattern3[16] = {36, 48, 36, 43, 36, 48, 40, 36, 38, 50, 38, 45, 38, 50, 43, 38};
        
        // Pattern 4: Octave Jump Pattern
        uint8_t pattern4[16] = {48, 60, 50, 62, 52, 64, 53, 65, 55, 67, 57, 69, 59, 71, 60, 72};
        
        // Pattern 5: Chord Progression (C-F-G-Am)
        uint8_t pattern5[16] = {48, 52, 55, 60, 53, 57, 60, 65, 55, 59, 62, 67, 57, 60, 64, 69};
        
        // Pattern 6: Ambient Pad
        uint8_t pattern6[16] = {60, 64, 67, 72, 67, 64, 60, 72, 62, 65, 69, 74, 69, 65, 62, 74};
        
        // Pattern 7: Chromatic Walk
        uint8_t pattern7[16] = {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 71, 70, 69};
        
        // Pattern 8: Melodic Sequence (Uplifting)
        uint8_t pattern8[16] = {60, 64, 67, 72, 64, 67, 72, 76, 67, 72, 76, 79, 72, 76, 79, 84};
        
        // Pattern 9: Rhythmic Pattern (Hi-Low)
        uint8_t pattern9[16] = {72, 48, 72, 60, 74, 50, 74, 62, 76, 52, 76, 64, 77, 53, 77, 65};
        
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
            gate_mask_storage[i] = 0xFFFF;
            steps_storage[i] = 16;
        }
        
        if (eeprom_is_initialized()) {
            for (int i = 0; i < NUM_PATTERN_SLOTS; ++i) {
                eeprom_write_pattern(i, pattern_storage[i], gate_mask_storage[i], steps_storage[i]);
            }
            eeprom_mark_valid();
        }
    }
}

void seq_save_pattern(uint8_t slot) {
    if (slot >= NUM_PATTERN_SLOTS) return;
    memcpy(pattern_storage[slot], state.notes, PATTERN_SIZE);
    gate_mask_storage[slot] = state.gate_mask;
    steps_storage[slot] = (uint8_t)state.steps;
    
    if (eeprom_is_initialized()) {
        eeprom_write_pattern(slot, state.notes, state.gate_mask, (uint8_t)state.steps);
    }
}

void seq_save_pattern_ram_only(uint8_t slot) {
    if (slot >= NUM_PATTERN_SLOTS) return;
    memcpy(pattern_storage[slot], state.notes, PATTERN_SIZE);
    gate_mask_storage[slot] = state.gate_mask;
    steps_storage[slot] = (uint8_t)state.steps;
    pattern_dirty[slot] = true;
}

void seq_flush_all_patterns_to_eeprom() {
    if (!eeprom_is_initialized()) return;
    bool any_written = false;
    for (int i = 0; i < NUM_PATTERN_SLOTS; i++) {
        if (pattern_dirty[i]) {
            eeprom_write_pattern(i, pattern_storage[i], gate_mask_storage[i], steps_storage[i]);
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
    if (slot >= NUM_PATTERN_SLOTS) return;
    
    memcpy(state.notes, pattern_storage[slot], PATTERN_SIZE);
    state.gate_mask = gate_mask_storage[slot];
    state.steps = steps_storage[slot];
    if (state.steps < 1 || state.steps > 16) {
        state.steps = 16;
    }
    state.current_step = (state.steps > 0) ? (state.steps - 1) : 15;
}

void seq_queue_pattern(uint8_t slot) {
    if (slot >= NUM_PATTERN_SLOTS) return;
    pending_pattern_slot = slot;
}

int8_t seq_get_pending_pattern() {
    return pending_pattern_slot;
}
