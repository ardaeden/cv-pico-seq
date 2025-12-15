#include "pico/stdlib.h"

#include "clock.h"
#include "io.h"
#include "sequencer.h"
#include "ui.h"
#include "mcp4822.h"

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
    // Draw step grid immediately on boot so boxes are visible before playback
    ui_show_steps(seq_current_step(), seq_get_steps());

    // Initialize MCP4822 DAC (SPI0: SCK=GP18, MOSI=GP19, CS=GP17)
    mcp4822_init(17);

    // NOTE scaling to match example: 88-note mapping to 0..4095 units
    // NOTE_SF approximates (4095/87) per example
    const float NOTE_SF = 47.069f;

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
            // debug: BPM changed
            ui_show_bpm((uint32_t)new_bpm);
        }

        if (io_poll_play_toggle()) {
            bool is_playing = seq_toggle_play();
            // debug: play toggled
            if (is_playing) {
                io_blink_led_start();  // blink immediately when starting
                // debug: LED blink at start
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

            // Compute and set CV for current step using MCP4822 channel A
            // Use same mapping as example: scaled by NOTE_SF
            uint32_t cur = seq_current_step();
            uint16_t dac_val = (uint16_t)((float)cur * NOTE_SF + 0.5f);
            // clamp
            if (dac_val > 0x0FFF) dac_val = 0x0FFF;
            // Use gain=1 to match example semantics (gain=2x hardware selection)
            mcp4822_set_voltage(0, 1, dac_val);
            // Update step display (draw steps), then redraw BPM on top so BPM remains visible
            ui_show_steps(seq_current_step(), seq_get_steps());
            ui_show_bpm(seq_get_bpm());

            // Blink LED every 4 steps (quarter note)
            if (seq_current_step() % 4 == 0) {
                io_blink_led_start();
                // debug: LED blink at step
            }

            // TODO: Update CV/Gate outputs here (DAC, GPIO, etc.)
        }

        // Non-time-critical work can be done here: read inputs, update BPM, etc.
        tight_loop_contents();
    }
}