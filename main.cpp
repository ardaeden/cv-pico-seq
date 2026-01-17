#include "pico/stdlib.h"

#include "clock.h"
#include "io.h"
#include "sequencer.h"
#include "ui.h"

int main() {
  stdio_init_all();
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
  ui_show_steps(seq_get_steps(), seq_get_steps());

  constexpr uint8_t MIDI_BASE = 36;
  constexpr float DAC_PER_SEMITONE = 4096.0f / 48.0f;

  auto update_cv = [&](uint32_t step) {
    if (seq_get_gate_enabled(step)) {
      uint8_t midi_note = seq_get_note(step);
      int32_t semitones = (int32_t)midi_note - MIDI_BASE;
      int32_t dac_val = (int32_t)(semitones * DAC_PER_SEMITONE + 0.5f);
      if (dac_val < 0)
        dac_val = 0;
      if (dac_val > 0x0FFF)
        dac_val = 0x0FFF;
      clock_set_cv((uint16_t)dac_val);
    }
  };

  enum EditMode {
    EDIT_NONE,
    EDIT_SELECT_STEP,
    EDIT_NOTE,
    PATTERN_SELECT,
    SETTINGS
  };
  EditMode edit_mode = EDIT_NONE;
  uint32_t edit_step = 0;
  uint8_t pattern_slot = 0;
  uint8_t temp_pattern_slot = 0;
  int settings_option = 0;

  bool blink_active = false;
  uint64_t blink_start_time = 0;
  uint8_t blink_slot = 0;

  int encoder_step = 1;
  uint32_t tick_count = 0;
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
        // Sync start: Restart clock and advance to first step immediately
        clock_restart();
        seq_advance_step();
        tick_count = 1;

        uint32_t cur = seq_current_step();
        bool gate_on = seq_get_gate_enabled(cur);

        update_cv(cur);
        clock_gate_enable(gate_on);
        clock_out_enable(true);

        // LED and UI updates for the first step
        if (cur % 4 == 0) {
          io_blink_led_start();
        }
        if (edit_mode == EDIT_NONE) {
          ui_show_steps(cur, seq_get_steps());
          int8_t pending = seq_get_pending_pattern();
          ui_show_bpm(seq_get_bpm(), pattern_slot, pending >= 0);
        }
      } else {
        clock_gate_enable(false);
        clock_out_enable(false);
        if (was_playing && seq_has_dirty_patterns()) {
          seq_flush_all_patterns_to_eeprom();
        }
      }
    }

    if (io_poll_stop_button()) {
      seq_stop();
      clock_gate_enable(false);
      clock_out_enable(false);

      if (seq_has_dirty_patterns()) {
        seq_flush_all_patterns_to_eeprom();
      }

      if (edit_mode == EDIT_NONE) {
        ui_show_bpm(seq_get_bpm(), pattern_slot);
        ui_show_steps(seq_get_steps(), seq_get_steps());
      } else if (edit_mode == PATTERN_SELECT) {
        ui_show_pattern_select(temp_pattern_slot);
      } else if (edit_mode == SETTINGS) {
        edit_mode = EDIT_NONE;
        ui_clear();
        ui_show_bpm(seq_get_bpm(), pattern_slot);
        ui_show_steps(seq_current_step(), seq_get_steps());
      }
    }

    if (io_poll_edit_toggle()) {
      if (edit_mode == EDIT_NONE) {
        edit_mode = EDIT_SELECT_STEP;
        edit_step = 0;
        ui_show_edit_step(edit_step, seq_get_note(edit_step));
      } else if (edit_mode == EDIT_SELECT_STEP || edit_mode == EDIT_NOTE ||
                 edit_mode == SETTINGS) {
        edit_mode = EDIT_NONE;
        ui_clear();
        ui_show_bpm(seq_get_bpm(), pattern_slot);
        ui_show_steps(seq_is_playing() ? seq_current_step() : seq_get_steps(),
                      seq_get_steps());
      } else if (edit_mode == PATTERN_SELECT) {
        edit_mode = EDIT_SELECT_STEP;
        edit_step = 0;
        ui_show_edit_step(edit_step, seq_get_note(edit_step));
      }
    }

    if (io_poll_pattern_select_button()) {
      if (edit_mode == EDIT_NONE || edit_mode == EDIT_SELECT_STEP ||
          edit_mode == EDIT_NOTE || edit_mode == SETTINGS) {
        edit_mode = PATTERN_SELECT;
        temp_pattern_slot = pattern_slot;
        ui_show_pattern_select(temp_pattern_slot);
      } else if (edit_mode == PATTERN_SELECT) {
        edit_mode = EDIT_NONE;
        ui_clear();
        ui_show_bpm(seq_get_bpm(), pattern_slot);
        ui_show_steps(seq_is_playing() ? seq_current_step() : seq_get_steps(),
                      seq_get_steps());
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
        ui_show_steps(seq_is_playing() ? seq_current_step() : seq_get_steps(),
                      seq_get_steps());
      } else if (edit_mode == SETTINGS) {
        if (settings_option == 0) {
          ClockSource current = clock_get_source();
          clock_set_source(current == CLOCK_INTERNAL ? CLOCK_EXTERNAL
                                                     : CLOCK_INTERNAL);
          ui_show_settings(settings_option, clock_get_source());
        }
      } else {
        encoder_step = (encoder_step == 1) ? 10 : 1;
      }
    }

    int encoder_delta = io_encoder_poll_delta();
    if (encoder_delta != 0) {
      if (edit_mode == EDIT_SELECT_STEP) {
        int new_step = (int)edit_step + encoder_delta;
        if (new_step < 0)
          new_step = 0;
        if (new_step > 15)
          new_step = 15;
        edit_step = (uint32_t)new_step;
        ui_show_edit_step(edit_step, seq_get_note(edit_step));

      } else if (edit_mode == EDIT_NOTE) {
        uint8_t current_note = seq_get_note(edit_step);
        int new_note = (int)current_note + encoder_delta;
        if (new_note < 36)
          new_note = 36;
        if (new_note > 84)
          new_note = 84;
        seq_set_note(edit_step, (uint8_t)new_note);
        ui_show_edit_note(edit_step, (uint8_t)new_note);

      } else if (edit_mode == PATTERN_SELECT) {
        int new_slot = (int)temp_pattern_slot + encoder_delta;
        if (new_slot < 0)
          new_slot = 0;
        if (new_slot > 9)
          new_slot = 9;
        temp_pattern_slot = (uint8_t)new_slot;
        ui_show_pattern_select(temp_pattern_slot);

      } else if (edit_mode == SETTINGS) {
        // Future: Change settings_option
      } else {
        if (io_is_step_button_pressed()) {
          uint32_t current_steps = seq_get_steps();
          int new_steps = (int)current_steps + encoder_delta;
          if (new_steps < 1)
            new_steps = 1;
          if (new_steps > 16)
            new_steps = 16;
          seq_set_steps((uint32_t)new_steps);
          ui_show_steps(seq_is_playing() ? seq_current_step() : 16,
                        (uint32_t)new_steps);
        } else {
          uint32_t current_bpm = seq_get_bpm();
          int new_bpm = (int)current_bpm + encoder_delta * encoder_step;
          if (new_bpm < 20)
            new_bpm = 20;
          if (new_bpm > 300)
            new_bpm = 300;

          seq_set_bpm((uint32_t)new_bpm);
          clock_set_bpm((uint32_t)new_bpm);
          ui_show_bpm((uint32_t)new_bpm, pattern_slot);
        }
      }
    }

    if (edit_mode == EDIT_SELECT_STEP || edit_mode == EDIT_NOTE) {
      if (io_poll_step_button()) {
        seq_toggle_gate(edit_step);
        if (edit_mode == EDIT_SELECT_STEP) {
          ui_show_edit_step(edit_step, seq_get_note(edit_step));
        } else {
          ui_show_edit_note(edit_step, seq_get_note(edit_step));
        }
      }
      if (io_poll_save_button()) {
        edit_mode = SETTINGS;
        settings_option = 0;
        ui_show_settings(settings_option, clock_get_source());
      }
    } else if (edit_mode == PATTERN_SELECT) {
      if (io_poll_save_button()) {
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
    } else if (edit_mode == SETTINGS) {
      if (io_poll_save_button()) {
        edit_mode = EDIT_NONE;
        ui_clear();
        ui_show_bpm(seq_get_bpm(), pattern_slot);
        ui_show_steps(seq_is_playing() ? seq_current_step() : seq_get_steps(),
                      seq_get_steps());
      }
    } else if (edit_mode == EDIT_NONE) {
      if (io_poll_save_button()) {
        edit_mode = SETTINGS;
        settings_option = 0;
        ui_show_settings(settings_option, clock_get_source());
      }
    }

    if (clock_consume_tick()) {
      if (!seq_is_playing()) {
        tick_count = 0;
        tight_loop_contents();
        continue;
      }

      // Sync: Sequencer advances every 6 ticks (24 PPQN / 6 = 4 pulses per
      // quarter = 16th notes)
      if (tick_count % 6 == 0) {
        seq_advance_step();

        uint32_t cur = seq_current_step();
        bool gate_on = seq_get_gate_enabled(cur);

        update_cv(cur);
        clock_gate_enable(gate_on);

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
      } else {
        // For ticks 1-5 of the 16th note, we disable the gate trigger to avoid
        // 24 pulses per bar
        clock_gate_enable(false);
      }

      tick_count++;
      if (tick_count >= 24)
        tick_count = 0;
    }

    tight_loop_contents();
  }
}