#include "pico/stdlib.h"

#include "clock.h"
#include "io.h"
#include "sequencer.h"
#include "ui.h"

int main() {
    io_init();
    io_encoder_init();
    seq_init();
    seq_init_flash();
    seq_load_pattern(0);

    clock_set_bpm(seq_get_bpm());
    clock_launch_core1();

    ui_init();
    ui_boot_animation();
    ui_show_bpm(seq_get_bpm(), 0);
    ui_show_steps(16, seq_get_steps());

    constexpr uint8_t MIDI_BASE = 36;
    constexpr float DAC_PER_SEMITONE = 4096.0f / 48.0f;

    enum EditMode { EDIT_NONE, EDIT_SELECT_STEP, EDIT_NOTE, PATTERN_SELECT };
    EditMode edit_mode = EDIT_NONE;
    uint32_t edit_step = 0;
    uint8_t pattern_slot = 0;
    uint8_t temp_pattern_slot = 0;
    
    bool blink_active = false;
    uint64_t blink_start_time = 0;
    uint8_t blink_slot = 0;
    
    int encoder_step = 1;
    while (true) {
        io_update_led();
        
        if (blink_active) {
            uint64_t elapsed = time_us_64() - blink_start_time;
            if (elapsed >= 150000) {
                clear_region(48, 16, 32, 32);
                char slot_char = '0' + blink_slot;
                draw_scaled_char(56, 24, slot_char, 3);
                ssd1306_update();
                blink_active = false;
            }
        }

        if (io_poll_play_toggle()) {
            bool was_playing = seq_is_playing();
            bool is_playing = seq_toggle_play();
            
            if (is_playing) {
                uint32_t start_step = seq_current_step() + 1;
                if (start_step >= seq_get_steps()) start_step = 0;
                bool first_gate = seq_get_gate_enabled(start_step);
                clock_gate_enable(first_gate);
            } else {
                clock_gate_enable(false);
                if (was_playing && seq_has_dirty_patterns()) {
                    seq_flush_all_patterns_to_eeprom();
                }
            }
        }

        if (io_poll_stop_button()) {
            seq_stop();
            clock_gate_enable(false);
            
            if (seq_has_dirty_patterns()) {
                seq_flush_all_patterns_to_eeprom();
            }
            
            if (edit_mode == EDIT_NONE) {
                ui_show_bpm(seq_get_bpm(), pattern_slot);
                ui_show_steps(16, seq_get_steps());
            } else if (edit_mode == PATTERN_SELECT) {
                ui_show_pattern_select(temp_pattern_slot);
            }
        }

        if (io_poll_edit_toggle()) {
            if (edit_mode == EDIT_NONE) {
                edit_mode = EDIT_SELECT_STEP;
                edit_step = 0;
                ui_show_edit_step(edit_step, seq_get_note(edit_step));
            } else if (edit_mode == EDIT_SELECT_STEP || edit_mode == EDIT_NOTE) {
                edit_mode = EDIT_NONE;
                ui_clear();
                ui_show_bpm(seq_get_bpm(), pattern_slot);
                ui_show_steps(16, seq_get_steps());
            } else if (edit_mode == PATTERN_SELECT) {
                edit_mode = EDIT_SELECT_STEP;
                edit_step = 0;
                ui_show_edit_step(edit_step, seq_get_note(edit_step));
            }
        }

        if (io_poll_save_button()) {
            if (edit_mode == EDIT_NONE) {
                edit_mode = PATTERN_SELECT;
                temp_pattern_slot = pattern_slot;
                ui_show_pattern_select(temp_pattern_slot);
            } else if (edit_mode == EDIT_SELECT_STEP || edit_mode == EDIT_NOTE) {
                edit_mode = PATTERN_SELECT;
                temp_pattern_slot = pattern_slot;
                ui_show_pattern_select(temp_pattern_slot);
            } else if (edit_mode == PATTERN_SELECT) {
                edit_mode = EDIT_NONE;
                if (!seq_is_playing()) {
                    ui_clear();
                    ui_show_bpm(seq_get_bpm(), pattern_slot);
                    ui_show_steps(16, seq_get_steps());
                } else {
                    ui_clear();
                    ui_show_bpm(seq_get_bpm(), pattern_slot);
                    ui_show_steps(seq_current_step(), seq_get_steps());
                }
            }
        }

        if (io_encoder_button_pressed()) {
            if (edit_mode == EDIT_SELECT_STEP) {
                edit_mode = EDIT_NOTE;
                ui_clear();
                ui_show_edit_note(edit_step, seq_get_note(edit_step));
            } else if (edit_mode == EDIT_NOTE) {
                edit_mode = EDIT_SELECT_STEP;
                ui_clear();
                ui_show_edit_step(edit_step, seq_get_note(edit_step));
            } else if (edit_mode == PATTERN_SELECT) {
                if (seq_is_playing()) {
                    seq_queue_pattern(temp_pattern_slot);
                } else {
                    seq_load_pattern(temp_pattern_slot);
                }
                pattern_slot = temp_pattern_slot;
                edit_mode = EDIT_NONE;
                ui_clear();
                ui_show_bpm(seq_get_bpm(), pattern_slot);
                ui_show_steps(seq_is_playing() ? seq_current_step() : 16, seq_get_steps());
            } else {
                encoder_step = (encoder_step == 1) ? 10 : 1;
            }
        }

        int encoder_delta = io_encoder_poll_delta();
        if (encoder_delta != 0) {
            if (edit_mode == EDIT_SELECT_STEP) {
                int new_step = (int)edit_step + encoder_delta;
                if (new_step < 0) new_step = 0;
                if (new_step > 15) new_step = 15;
                edit_step = (uint32_t)new_step;
                ui_show_edit_step(edit_step, seq_get_note(edit_step));
                
            } else if (edit_mode == EDIT_NOTE) {
                uint8_t current_note = seq_get_note(edit_step);
                int new_note = (int)current_note + encoder_delta;
                if (new_note < 36) new_note = 36;
                if (new_note > 84) new_note = 84;
                seq_set_note(edit_step, (uint8_t)new_note);
                ui_show_edit_note(edit_step, (uint8_t)new_note);
                
            } else if (edit_mode == PATTERN_SELECT) {
                int new_slot = (int)temp_pattern_slot + encoder_delta;
                if (new_slot < 0) new_slot = 0;
                if (new_slot > 9) new_slot = 9;
                temp_pattern_slot = (uint8_t)new_slot;
                ui_show_pattern_select(temp_pattern_slot);
                
            } else {
                uint32_t current_bpm = seq_get_bpm();
                int new_bpm = (int)current_bpm + encoder_delta * encoder_step;
                if (new_bpm < 20) new_bpm = 20;
                if (new_bpm > 300) new_bpm = 300;
                
                seq_set_bpm((uint32_t)new_bpm);
                clock_set_bpm((uint32_t)new_bpm);
                ui_show_bpm((uint32_t)new_bpm, pattern_slot);
            }
        }

        if (edit_mode == EDIT_SELECT_STEP || edit_mode == EDIT_NOTE) {
            if (io_poll_load_button()) {
                seq_toggle_gate(edit_step);
                if (edit_mode == EDIT_SELECT_STEP) {
                    ui_show_edit_step(edit_step, seq_get_note(edit_step));
                } else {
                    ui_show_edit_note(edit_step, seq_get_note(edit_step));
                }
            }
        } else if (edit_mode == PATTERN_SELECT) {
            if (io_poll_load_button()) {
                seq_save_pattern_ram_only(temp_pattern_slot);
                if (!seq_is_playing()) {
                    seq_flush_all_patterns_to_eeprom();
                }
                pattern_slot = temp_pattern_slot;
                
                clear_region(48, 16, 32, 32);
                ssd1306_update();
                blink_active = true;
                blink_start_time = time_us_64();
                blink_slot = temp_pattern_slot;
            }
        }

        if (clock_consume_tick()) {
            if (!seq_is_playing()) {
                tight_loop_contents();
                continue;
            }

            seq_advance_step();

            uint32_t cur = seq_current_step();
            uint8_t midi_note = seq_get_note(cur);
            
            int32_t semitones = (int32_t)midi_note - MIDI_BASE;
            int32_t dac_val = (int32_t)(semitones * DAC_PER_SEMITONE + 0.5f);
            
            if (dac_val < 0) dac_val = 0;
            if (dac_val > 0x0FFF) dac_val = 0x0FFF;
            
            clock_set_cv((uint16_t)dac_val);
            
            uint32_t next_step = (cur + 1) % seq_get_steps();
            bool next_gate_enabled = seq_get_gate_enabled(next_step);
            clock_gate_enable(next_gate_enabled);

            if (edit_mode == EDIT_NONE) {
                ui_show_steps(seq_current_step(), seq_get_steps());
                
                int8_t pending = seq_get_pending_pattern();
                bool blink = false;
                if (pending >= 0) {
                    blink = (seq_current_step() % 4 < 2);
                }
                ui_show_bpm(seq_get_bpm(), pattern_slot, blink);
            }

            if (seq_current_step() % 4 == 0) {
                io_blink_led_start();
            }
        }

        tight_loop_contents();
    }
}