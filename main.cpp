#include "pico/stdlib.h"
#include <cstdio>

#include "clock.h"
#include "io.h"
#include "sequencer.h"

int main() {
    stdio_init_all();

    io_init();
    seq_init();

    // Configure clock core using current BPM
    clock_set_bpm(seq_get_bpm());
    clock_launch_core1();

    // Core0: sequencer loop consumes tick_flag and advances state
    while (true) {
        if (io_poll_play_toggle()) {
            bool is_playing = seq_toggle_play();
            printf("Play state: %s\n", is_playing ? "ON" : "OFF");
        }

        if (clock_consume_tick()) {
            if (!seq_is_playing()) {
                tight_loop_contents();
                continue;
            }

            seq_advance_step();

            // TODO: Update CV/Gate outputs here (DAC, GPIO, etc.)
            // Placeholder: lightweight printf for debug (can be disabled)
            printf("Step: %u\n", (unsigned)seq_current_step());
        }

        // Non-time-critical work can be done here: read inputs, update BPM, etc.
        tight_loop_contents();
    }
}