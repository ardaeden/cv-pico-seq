#include "sequencer.h"

#include <atomic>

namespace {
struct SequencerState {
    uint32_t bpm;
    uint32_t ppqn;
    uint32_t steps;
    uint32_t current_step;
    std::atomic<bool> playing;
};

static SequencerState state = {120, 4, 16, 0, false};
} // namespace

void seq_init() {
    state.bpm = 120;
    state.ppqn = 4;  // 4 pulses per quarter note for 16th notes per step
    state.steps = 16;
    state.current_step = 0;
    state.playing.store(false);
}

bool seq_toggle_play() {
    bool expected = state.playing.load();
    // Toggle atomically
    while (!state.playing.compare_exchange_weak(expected, !expected)) {
        // expected is updated by compare_exchange_weak on failure
    }
    bool is_playing = !expected;
    if (is_playing) {
        // When starting playback, set current_step to the previous step
        // so the first advance moves to step 0 (first step).
        state.current_step = state.steps ? (state.steps - 1) : 15;
    } else {
        state.current_step = 0; // reset to first step when paused
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

uint32_t seq_get_ppqn() {
    return state.ppqn;
}

uint32_t seq_get_steps() {
    return state.steps;
}
