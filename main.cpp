#include "pico/stdlib.h"

#include "clock.h"
#include "io.h"
#include "sequencer.h"
#include "ui.h"

int main() {
    io_init();
    io_encoder_init();
    seq_init();

    // Configure clock core using current BPM and PPQN
    clock_set_bpm(seq_get_bpm());
    clock_launch_core1();

    // Initialize display UI (if present)
    ui_init();
    ui_show_bpm(seq_get_bpm());
    // Draw step grid with no highlight on boot (step 16 is invalid/none)
    ui_show_steps(16, seq_get_steps());

    // MIDI to CV conversion: 1V/octave, C1 (MIDI 36) = 0V reference
    // 4 octaves = 48 semitones = 4096 DAC units
    // DAC per semitone = 4096 / 48 = 85.333...
    constexpr uint8_t MIDI_BASE = 36;  // C1 as 0V
    constexpr float DAC_PER_SEMITONE = 4096.0f / 48.0f;

    int encoder_step = 1; // 1 = fine, 10 = coarse
    while (true) {
        io_update_led();  // non-blocking LED update

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
            ui_show_bpm((uint32_t)new_bpm);
        }

        if (io_poll_play_toggle()) {
            bool is_playing = seq_toggle_play();
            clock_gate_enable(is_playing);
        }

        if (clock_consume_tick()) {
            if (!seq_is_playing()) {
                tight_loop_contents();
                continue;
            }

            // Advance one step per tick (tick represents 16th note when ppqn==4)
            seq_advance_step();

            // Get MIDI note for current step and convert to CV
            uint32_t cur = seq_current_step();
            uint8_t midi_note = seq_get_note(cur);
            
            // Convert MIDI note to DAC value (1V/octave)
            int32_t semitones = (int32_t)midi_note - MIDI_BASE;
            int32_t dac_val = (int32_t)(semitones * DAC_PER_SEMITONE + 0.5f);
            
            // Clamp to valid range
            if (dac_val < 0) dac_val = 0;
            if (dac_val > 0x0FFF) dac_val = 0x0FFF;
            
            // Send CV to core 1 for synchronized output
            clock_set_cv((uint16_t)dac_val);

            // Update step display (draw steps), then redraw BPM on top so BPM remains visible
            ui_show_steps(seq_current_step(), seq_get_steps());
            ui_show_bpm(seq_get_bpm());

            // Blink LED every 4 steps (quarter note)
            if (seq_current_step() % 4 == 0) {
                io_blink_led_start();
            }
        }

        tight_loop_contents();
    }
}