#include "sequencer.h"

namespace {
volatile uint32_t bpm = 120;        // beats per minute
volatile uint32_t steps = 16;       // total steps
volatile uint32_t current_step = 0; // advanced on ticks
volatile bool playing = true;       // play/pause flag
}

void seq_init() {
    bpm = 120;
    steps = 16;
    current_step = 0;
    playing = true;
}

bool seq_toggle_play() {
    playing = !playing;
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
