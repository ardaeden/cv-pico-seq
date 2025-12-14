#include "pico/stdlib.h"
#include <cstdio>

#include "clock.h"
#include "io.h"
#include "sequencer.h"
#include "ui.h"

int main() {
    stdio_init_all();

    io_init();
    io_encoder_init();
    seq_init();

    // Configure clock core using current BPM and PPQN
    clock_set_bpm(seq_get_bpm());
    clock_set_ppqn(seq_get_ppqn());
    clock_launch_core1();

    // Initialize display UI (if present)
    ui_init();
    ui_show_bpm(seq_get_bpm());

    // Core0: sequencer loop consumes tick_flag and advances state
    int encoder_step = 1; // 1 = fine, 10 = coarse
    while (true) {
        io_update_led();  // non-blocking LED update
        io_update_gate(); // non-blocking gate update

        // Encoder button toggles fine/coarse step
        if (io_encoder_button_pressed()) {
            encoder_step = (encoder_step == 1) ? 10 : 1;
        }

        // Poll encoder for BPM changes
        int encoder_delta = io_encoder_poll_delta();
        if (encoder_delta != 0) {
            uint32_t current_bpm = seq_get_bpm();
            int new_bpm = (int)current_bpm + encoder_delta * encoder_step;
            // Clamp BPM between 20 and 300
            if (new_bpm < 20) new_bpm = 20;
            if (new_bpm > 300) new_bpm = 300;
            
            seq_set_bpm((uint32_t)new_bpm);
            clock_set_bpm((uint32_t)new_bpm);
            // printf("BPM: %u\n", (unsigned)new_bpm);
            ui_show_bpm((uint32_t)new_bpm);
        }

        if (io_poll_play_toggle()) {
            bool is_playing = seq_toggle_play();
            // printf("Play state: %s\n", is_playing ? "ON" : "OFF");
            if (is_playing) {
                io_blink_led_start();  // blink immediately when starting
                // printf("LED blink at step: 0\n");
            }
        }

        if (clock_consume_tick()) {
            if (!seq_is_playing()) {
                tight_loop_contents();
                continue;
            }

            seq_advance_step();

            // Pulse gate on each step: 50% of current step duration
            uint32_t step_us = clock_get_interval_us();
            io_gate_pulse_us(step_us / 2);

            // Blink LED every 4 steps (quarter note)
            if (seq_current_step() % 4 == 0) {
                io_blink_led_start();
                // printf("LED blink at step: %u\n", (unsigned)seq_current_step());
            }

            // TODO: Update CV/Gate outputs here (DAC, GPIO, etc.)
            // Placeholder: lightweight printf for debug (can be disabled)
            // printf("Step: %u\n", (unsigned)seq_current_step());
        }

        // Non-time-critical work can be done here: read inputs, update BPM, etc.
        tight_loop_contents();
    }
}