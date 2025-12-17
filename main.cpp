#include "pico/stdlib.h"

#include "clock.h"
#include "io.h"
#include "sequencer.h"
#include "ui.h"

int main() {
    io_init();
    io_encoder_init();
    seq_init();
    seq_init_flash();  // Initialize pattern storage with default patterns

    // Configure clock core using current BPM and PPQN
    clock_set_bpm(seq_get_bpm());
    clock_launch_core1();

    // Initialize display UI (if present)
    ui_init();
    ui_show_bpm(seq_get_bpm(), 0);
    // Draw step grid with no highlight on boot (step 16 is invalid/none)
    ui_show_steps(16, seq_get_steps());

    // MIDI to CV conversion: 1V/octave, C2 (MIDI 36) = 0V reference
    // 4 octaves = 48 semitones = 4096 DAC units
    // DAC per semitone = 4096 / 48 = 85.333...
    constexpr uint8_t MIDI_BASE = 36;  // C2 as 0V
    constexpr float DAC_PER_SEMITONE = 4096.0f / 48.0f;

    // Edit mode state
    enum EditMode { EDIT_NONE, EDIT_SELECT_STEP, EDIT_NOTE, PATTERN_SELECT };
    EditMode edit_mode = EDIT_NONE;
    uint32_t edit_step = 0;
    uint8_t pattern_slot = 0;  // Current pattern slot (0-9)
    
    int encoder_step = 1; // 1 = fine, 10 = coarse (for BPM)
    while (true) {
        io_update_led();  // non-blocking LED update

        // Play button: only play/stop
        if (io_poll_play_toggle()) {
            bool is_playing = seq_toggle_play();
            clock_gate_enable(is_playing);
            
            // If entering play mode, exit edit mode
            if (is_playing && edit_mode != EDIT_NONE) {
                edit_mode = EDIT_NONE;
                ui_clear();
                ui_show_bpm(seq_get_bpm(), pattern_slot);
                ui_show_steps(16, seq_get_steps());
            }
        }

        // Edit button: cycle through modes (only when stopped)
        if (io_poll_edit_toggle()) {
            if (!seq_is_playing()) {
                if (edit_mode == EDIT_NONE) {
                    // Enter step edit mode
                    edit_mode = EDIT_SELECT_STEP;
                    edit_step = 0;
                    ui_show_edit_step(edit_step, seq_get_note(edit_step));
                } else if (edit_mode == EDIT_SELECT_STEP || edit_mode == EDIT_NOTE) {
                    // Enter pattern select mode
                    edit_mode = PATTERN_SELECT;
                    ui_show_pattern_select(pattern_slot);
                } else if (edit_mode == PATTERN_SELECT) {
                    // Return to normal mode
                    edit_mode = EDIT_NONE;
                    ui_clear();
                    ui_show_bpm(seq_get_bpm(), pattern_slot);
                    ui_show_steps(16, seq_get_steps());
                }
            }
        }

        // Encoder switch handling
        if (io_encoder_button_pressed()) {
            if (edit_mode == EDIT_SELECT_STEP) {
                // Enter note edit mode
                edit_mode = EDIT_NOTE;
                ui_show_edit_note(edit_step, seq_get_note(edit_step));
            } else if (edit_mode == EDIT_NOTE) {
                // Return to step select mode
                edit_mode = EDIT_SELECT_STEP;
                ui_show_edit_step(edit_step, seq_get_note(edit_step));
            } else {
                // Playing mode: toggle fine/coarse BPM
                encoder_step = (encoder_step == 1) ? 10 : 1;
            }
        }

        // Encoder rotation handling
        int encoder_delta = io_encoder_poll_delta();
        if (encoder_delta != 0) {
            if (edit_mode == EDIT_SELECT_STEP) {
                // Change selected step
                int new_step = (int)edit_step + encoder_delta;
                if (new_step < 0) new_step = 0;
                if (new_step > 15) new_step = 15;
                edit_step = (uint32_t)new_step;
                ui_show_edit_step(edit_step, seq_get_note(edit_step));
                
            } else if (edit_mode == EDIT_NOTE) {
                // Change note value (limit to CV range: C2-C6, MIDI 36-84)
                uint8_t current_note = seq_get_note(edit_step);
                int new_note = (int)current_note + encoder_delta;
                if (new_note < 36) new_note = 36;   // C2
                if (new_note > 84) new_note = 84;   // C6
                seq_set_note(edit_step, (uint8_t)new_note);
                ui_show_edit_note(edit_step, (uint8_t)new_note);
                
            } else if (edit_mode == PATTERN_SELECT) {
                // Change pattern slot
                int new_slot = (int)pattern_slot + encoder_delta;
                if (new_slot < 0) new_slot = 0;
                if (new_slot > 9) new_slot = 9;
                pattern_slot = (uint8_t)new_slot;
                ui_show_pattern_select(pattern_slot);
                
            } else {
                // Playing mode: change BPM
                uint32_t current_bpm = seq_get_bpm();
                int new_bpm = (int)current_bpm + encoder_delta * encoder_step;
                if (new_bpm < 20) new_bpm = 20;
                if (new_bpm > 300) new_bpm = 300;
                
                seq_set_bpm((uint32_t)new_bpm);
                clock_set_bpm((uint32_t)new_bpm);
                ui_show_bpm((uint32_t)new_bpm, pattern_slot);
            }
        }

        // Save/Load button handling (only in pattern select mode)
        if (edit_mode == PATTERN_SELECT) {
            if (io_poll_save_button()) {
                seq_save_pattern(pattern_slot);
                io_blink_led_start();  // Visual feedback
                // Return to normal mode after save
                edit_mode = EDIT_NONE;
                ui_clear();
                ui_show_bpm(seq_get_bpm(), pattern_slot);
                ui_show_steps(16, seq_get_steps());
            }
            if (io_poll_load_button()) {
                seq_load_pattern(pattern_slot);
                io_blink_led_start();  // Visual feedback
                // Return to normal mode after load
                edit_mode = EDIT_NONE;
                ui_clear();
                ui_show_bpm(seq_get_bpm(), pattern_slot);
                ui_show_steps(16, seq_get_steps());
            }
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
            ui_show_bpm(seq_get_bpm(), pattern_slot);

            // Blink LED every 4 steps (quarter note)
            if (seq_current_step() % 4 == 0) {
                io_blink_led_start();
            }
        }

        tight_loop_contents();
    }
}