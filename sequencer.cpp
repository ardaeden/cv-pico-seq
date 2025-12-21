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
    bool is_playing = !expected;
    if (!is_playing) {
        state.current_step = 15;
    }
    return is_playing;
}

bool seq_is_playing() {
    return state.playing.load();
}

void seq_advance_step() {
    uint32_t prev_step = state.current_step;
    state.current_step = (state.current_step + 1) % (state.steps ? state.steps : 16);
    
    if (prev_step == 15 && state.current_step == 0 && pending_pattern_slot >= 0) {
        if (pending_pattern_slot < NUM_PATTERN_SLOTS) {
            memcpy(state.notes, pattern_storage[pending_pattern_slot], PATTERN_SIZE);
            state.gate_mask = gate_mask_storage[pending_pattern_slot];
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
    
    if (eeprom_is_initialized() && eeprom_has_valid_data()) {
        for (int i = 0; i < NUM_PATTERN_SLOTS; ++i) {
            eeprom_read_pattern(i, pattern_storage[i], &gate_mask_storage[i]);
        }
    } else {
        uint8_t pattern0[16] = {48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72, 74};
        uint8_t pattern1[16] = {48, 50, 52, 54, 55, 57, 59, 60, 62, 64, 66, 67, 69, 71, 72, 74};
        uint8_t pattern2[16] = {48, 50, 52, 55, 57, 60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84};
        uint8_t pattern3[16] = {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};
        uint8_t pattern4[16] = {48, 52, 55, 60, 64, 67, 72, 76, 79, 84, 79, 76, 72, 67, 64, 60};
        uint8_t pattern5[16] = {48, 60, 48, 60, 50, 62, 50, 62, 52, 64, 52, 64, 53, 65, 53, 65};
        uint8_t pattern6[16] = {36, 36, 43, 36, 36, 36, 43, 36, 38, 38, 45, 38, 36, 36, 43, 48};
        uint8_t pattern7[16] = {84, 83, 81, 79, 77, 76, 74, 72, 71, 69, 67, 65, 64, 62, 60, 48};
        uint8_t pattern8[16] = {48, 55, 50, 57, 52, 59, 53, 60, 55, 62, 57, 64, 59, 65, 60, 67};
        uint8_t pattern9[16] = {60, 62, 67, 65, 69, 64, 72, 60, 64, 67, 71, 69, 74, 72, 67, 60};
        
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
        }
        
        if (eeprom_is_initialized()) {
            for (int i = 0; i < NUM_PATTERN_SLOTS; ++i) {
                eeprom_write_pattern(i, pattern_storage[i], gate_mask_storage[i]);
            }
            eeprom_mark_valid();
        }
    }
}

void seq_save_pattern(uint8_t slot) {
    if (slot >= NUM_PATTERN_SLOTS) return;
    memcpy(pattern_storage[slot], state.notes, PATTERN_SIZE);
    gate_mask_storage[slot] = state.gate_mask;
    
    if (eeprom_is_initialized()) {
        eeprom_write_pattern(slot, state.notes, state.gate_mask);
    }
}

void seq_save_pattern_ram_only(uint8_t slot) {
    if (slot >= NUM_PATTERN_SLOTS) return;
    memcpy(pattern_storage[slot], state.notes, PATTERN_SIZE);
    gate_mask_storage[slot] = state.gate_mask;
}

void seq_flush_all_patterns_to_eeprom() {
    if (!eeprom_is_initialized()) return;
    for (int i = 0; i < NUM_PATTERN_SLOTS; i++) {
        eeprom_write_pattern(i, pattern_storage[i], gate_mask_storage[i]);
    }
}

void seq_load_pattern(uint8_t slot) {
    if (slot >= NUM_PATTERN_SLOTS) return;
    
    memcpy(state.notes, pattern_storage[slot], PATTERN_SIZE);
    state.gate_mask = gate_mask_storage[slot];
}

void seq_queue_pattern(uint8_t slot) {
    if (slot >= NUM_PATTERN_SLOTS) return;
    pending_pattern_slot = slot;
}

int8_t seq_get_pending_pattern() {
    return pending_pattern_slot;
}
