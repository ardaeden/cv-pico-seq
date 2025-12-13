#include "sequencer.h"

namespace {
volatile uint32_t bpm = 120;        // beats per minute
volatile uint32_t ppqn = 4;         // pulses per quarter note (4 for 16th notes)
volatile uint32_t steps = 16;       // total steps
volatile uint32_t current_step = 0; // advanced on ticks
volatile bool playing = true;       // play/pause flag
}

void seq_init() {
    bpm = 120;
    ppqn = 4;  // 4 pulses per quarter note for 16th notes per step
    steps = 16;
    current_step = 0;
    playing = false;  // start paused
}

bool seq_toggle_play() {
    playing = !playing;
    if (!playing) {
        current_step = 0;  // reset to first step when paused
    }
    return playing;
}

void seq_set_play(bool enable) {
    playing = enable;
}

bool seq_is_playing() {
    return playing;
}

void seq_advance_step() {
    current_step = (current_step + 1) % (steps ? steps : 16);
}

uint32_t seq_current_step() {
    return current_step;
}

uint32_t seq_get_bpm() {
    return bpm;
}

void seq_set_bpm(uint32_t new_bpm) {
    bpm = new_bpm ? new_bpm : 120;
}

uint32_t seq_get_ppqn() {
    return ppqn;
}

void seq_set_ppqn(uint32_t new_ppqn) {
    ppqn = new_ppqn ? new_ppqn : 4;
}
