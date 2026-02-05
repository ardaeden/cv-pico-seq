#include "pico/stdlib.h"

#include "clock.h"
#include "io.h"
#include "sequencer.h"
#include "ui.h"
#include <cstdio>

constexpr uint64_t BLINK_DURATION_US = 150000;
constexpr uint64_t LONG_PRESS_THRESHOLD_US = 500000;
constexpr uint64_t PAUSE_BLINK_INTERVAL_US = 500000;
constexpr uint32_t NO_POINTER = 0xFFFFFFFF;

int main() {
  stdio_init_all();
  io_init();
  io_encoder_init();
  seq_init();
  seq_init_flash();
  seq_load_pattern(0);

  clock_set_bpm(seq_get_bpm());
  clock_init();
  clock_launch_core1();

  enum EncoderMode { ENC_BPM_FINE, ENC_BPM_COARSE, ENC_OCTAVE, ENC_TRANSPOSE };
  EncoderMode enc_mode = ENC_BPM_FINE;

  TransportState current_tstate = TSTATE_STOP;

  ui_init();
  ui_boot_animation();
  ui_show_bpm(seq_get_bpm(), 0, clock_get_source(), current_tstate, false,
              enc_mode == ENC_BPM_COARSE, 0, seq_get_steps(), false,
              seq_get_global_octave(), enc_mode == ENC_OCTAVE,
              seq_get_global_transpose(), enc_mode == ENC_TRANSPOSE);
  // Hide pointer on boot (0xFFFFFFFF = no pointer)
  ui_show_steps(NO_POINTER, seq_get_steps());

  enum EditMode {
    EDIT_NONE,
    EDIT_SELECT_STEP,
    EDIT_NOTE,
    EDIT_VELOCITY,
    PATTERN_SELECT,
    SETTINGS,
    PATTERN_TOOLS,
    TOOLS_RANDOM_GATES,
    TOOLS_CLEAR
  };
  EditMode edit_mode = EDIT_NONE;
  uint32_t edit_step = 0;
  uint8_t pattern_slot = 0;
  uint8_t temp_pattern_slot = 0;
  int settings_option = 0;
  bool settings_edit_mode = false;
  const int NUM_SETTINGS = 4;

  int tools_selection = 0;
  bool tools_edit_mode = false;
  uint8_t temp_density = 50;
  bool clear_confirmed = false;

  bool blink_active = false;
  uint64_t blink_start_time = 0;
  uint8_t blink_slot = 0;

  bool step_button_down = false;
  uint32_t pause_blink_timer = 0;
  bool pause_icon_visible = true;
  uint64_t step_button_press_start_us = 0;
  bool step_button_was_modified = false;

  // Edit Button State Machine for Clean Long-Press
  bool edit_button_down = false;
  uint64_t edit_press_start_us = 0;
  bool edit_long_press_triggered = false;

  while (true) {
    io_update_led();

    if (blink_active) {
      uint64_t elapsed = time_us_64() - blink_start_time;
      if (elapsed >= BLINK_DURATION_US) {
        clear_region(40, 16, 48, 32);
        char slot_str[4];
        sprintf(slot_str, "%02d", blink_slot);
        draw_scaled_text(40, 24, slot_str, 3);
        ssd1306_update();
        blink_active = false;
      }
    }

    // --- TRANSPORT CONTROLS ---
    if (io_poll_play_toggle()) {
      bool was_playing = seq_is_playing();
      if (!was_playing) {
        // Start or Resume
        clock_gate_enable(true);
        clock_out_enable(true);
        seq_set_playing(true);
        if (current_tstate == TSTATE_STOP)
          clock_restart();
        else
          clock_resume();

        if (seq_current_step() % 4 == 0)
          io_blink_led_start();
        if (edit_mode == EDIT_NONE) {
          // Show pointer when playing
          ui_show_steps(seq_current_step(), seq_get_steps());
          current_tstate = TSTATE_PLAY;
          ui_show_bpm(seq_get_bpm(), pattern_slot, clock_get_source(),
                      current_tstate, seq_get_pending_pattern() >= 0,
                      enc_mode == ENC_BPM_COARSE, seq_current_step(),
                      seq_get_steps(), false, seq_get_global_octave(),
                      enc_mode == ENC_OCTAVE, seq_get_global_transpose(),
                      enc_mode == ENC_TRANSPOSE);
        }
      } else {
        // Pause
        seq_set_playing(false);
        clock_gate_enable(false);
        clock_out_enable(false);
        current_tstate = TSTATE_PAUSE;
        pause_blink_timer = time_us_64();
        pause_icon_visible = true;
        if (edit_mode == EDIT_NONE) {
          ui_show_bpm(seq_get_bpm(), pattern_slot, clock_get_source(),
                      current_tstate, false, enc_mode == ENC_BPM_COARSE,
                      seq_current_step(), seq_get_steps(), false,
                      seq_get_global_octave(), enc_mode == ENC_OCTAVE,
                      seq_get_global_transpose(), enc_mode == ENC_TRANSPOSE);
          // Show pointer when paused
          ui_show_steps(seq_current_step(), seq_get_steps());
        }
        if (was_playing && seq_has_dirty_patterns())
          seq_flush_all_patterns_to_eeprom();
      }
    }

    if (io_poll_stop_button()) {
      seq_stop();
      clock_gate_enable(false);
      clock_out_enable(false);
      if (seq_has_dirty_patterns())
        seq_flush_all_patterns_to_eeprom();
      current_tstate = TSTATE_STOP;
      if (edit_mode == EDIT_NONE) {
        ui_show_bpm(seq_get_bpm(), pattern_slot, clock_get_source(),
                    current_tstate, false, enc_mode == ENC_BPM_COARSE, 0,
                    seq_get_steps(), false, seq_get_global_octave(),
                    enc_mode == ENC_OCTAVE);
        // Hide pointer when stopped (0xFFFFFFFF = no pointer)
        ui_show_steps(NO_POINTER, seq_get_steps());
      }
    }

    // --- EDIT BUTTON LOGIC (Press-and-Hold / Release) ---
    bool edit_now = io_is_edit_button_pressed();
    if (edit_now && !edit_button_down) {
      edit_button_down = true;
      edit_press_start_us = time_us_64();
      edit_long_press_triggered = false;
    } else if (edit_now && edit_button_down && !edit_long_press_triggered) {
      if (time_us_64() - edit_press_start_us >
          LONG_PRESS_THRESHOLD_US) { // 500ms threshold
        edit_long_press_triggered = true;
        edit_mode = PATTERN_TOOLS;
        tools_selection = 0;
        tools_edit_mode = false;
        ui_show_pattern_tools(tools_selection, tools_edit_mode, temp_density);
      }
    } else if (!edit_now && edit_button_down) {
      // Button Released
      if (!edit_long_press_triggered) {
        // Short Press: Toggle Edit Modes
        if (edit_mode == EDIT_NONE || edit_mode == PATTERN_SELECT ||
            edit_mode == SETTINGS || edit_mode == PATTERN_TOOLS ||
            edit_mode == TOOLS_RANDOM_GATES || edit_mode == TOOLS_CLEAR) {
          edit_mode = EDIT_SELECT_STEP;
          edit_step = 0;
          ui_show_edit_step(edit_step, seq_get_note(edit_step));
        } else if (edit_mode == EDIT_SELECT_STEP) {
          edit_mode = EDIT_NONE;
          ui_clear();
          ui_show_bpm(seq_get_bpm(), pattern_slot, clock_get_source(),
                      current_tstate, false, enc_mode == ENC_BPM_COARSE,
                      current_tstate == TSTATE_STOP ? 0 : seq_current_step(),
                      seq_get_steps(),
                      current_tstate == TSTATE_PAUSE ? !pause_icon_visible
                                                     : false,
                      seq_get_global_octave(), enc_mode == ENC_OCTAVE,
                      seq_get_global_transpose(), enc_mode == ENC_TRANSPOSE);
          ui_show_steps(current_tstate != TSTATE_STOP ? seq_current_step()
                                                      : NO_POINTER,
                        seq_get_steps());
        } else {
          edit_mode = EDIT_SELECT_STEP;
          ui_show_edit_step(edit_step, seq_get_note(edit_step));
        }
      }
      edit_button_down = false;
    }

    // --- PATTERN BUTTON ACTIONS ---
    if (io_poll_pattern_select_button()) {
      if (edit_mode != PATTERN_SELECT) {
        edit_mode = PATTERN_SELECT;
        temp_pattern_slot = pattern_slot;
        ui_show_pattern_select(temp_pattern_slot);
      } else {
        edit_mode = EDIT_NONE;
        ui_clear();
        ui_show_bpm(seq_get_bpm(), pattern_slot, clock_get_source(),
                    current_tstate, false, enc_mode == ENC_BPM_COARSE,
                    current_tstate == TSTATE_STOP ? 0 : seq_current_step(),
                    seq_get_steps(),
                    current_tstate == TSTATE_PAUSE ? !pause_icon_visible
                                                   : false,
                    seq_get_global_octave(), enc_mode == ENC_OCTAVE,
                    seq_get_global_transpose(), enc_mode == ENC_TRANSPOSE);
        ui_show_steps(current_tstate != TSTATE_STOP ? seq_current_step()
                                                    : 0xFFFFFFFF,
                      seq_get_steps());
      }
    }

    // --- SAVE BUTTON ACTIONS (BACK in tools) ---
    if (io_poll_save_button()) {
      if (edit_mode == PATTERN_SELECT) {
        seq_save_pattern_ram_only(temp_pattern_slot);
        if (!seq_is_playing())
          seq_flush_all_patterns_to_eeprom();
        pattern_slot = temp_pattern_slot;
        clear_region(40, 16, 48, 32);
        ssd1306_update();
        blink_active = true;
        blink_start_time = time_us_64();
        blink_slot = temp_pattern_slot;
      } else if (edit_mode == SETTINGS) {
        edit_mode = EDIT_NONE;
        ui_clear();
        ui_show_bpm(seq_get_bpm(), pattern_slot, clock_get_source(),
                    current_tstate, false, enc_mode == ENC_BPM_COARSE,
                    current_tstate == TSTATE_STOP ? 0 : seq_current_step(),
                    seq_get_steps(),
                    current_tstate == TSTATE_PAUSE ? !pause_icon_visible
                                                   : false,
                    seq_get_global_octave(), enc_mode == ENC_OCTAVE,
                    seq_get_global_transpose(), enc_mode == ENC_TRANSPOSE);
        ui_show_steps(current_tstate != TSTATE_STOP ? seq_current_step()
                                                    : 0xFFFFFFFF,
                      seq_get_steps());
      } else if (edit_mode == PATTERN_TOOLS) {
        edit_mode = EDIT_NONE;
        ui_clear();
        ui_show_bpm(seq_get_bpm(), pattern_slot, clock_get_source(),
                    current_tstate, false, enc_mode == ENC_BPM_COARSE,
                    current_tstate == TSTATE_STOP ? 0 : seq_current_step(),
                    seq_get_steps(),
                    current_tstate == TSTATE_PAUSE ? !pause_icon_visible
                                                   : false,
                    seq_get_global_octave(), enc_mode == ENC_OCTAVE, seq_get_global_transpose(), enc_mode == ENC_TRANSPOSE);
        ui_show_steps(current_tstate != TSTATE_STOP ? seq_current_step()
                                                    : 0xFFFFFFFF,
                      seq_get_steps());
      } else if (edit_mode == TOOLS_RANDOM_GATES || edit_mode == TOOLS_CLEAR) {
        edit_mode = PATTERN_TOOLS;
        ui_show_pattern_tools(tools_selection);
      } else {
        edit_mode = SETTINGS;
        settings_option = 0;
        settings_edit_mode = false;
        ui_show_settings(settings_option, clock_get_source(),
                         clock_get_gate_length(), clock_get_ppqn(),
                         seq_get_load_mode(), settings_edit_mode);
      }
    }

    // --- ENCODER ACTIONS ---
    if (io_encoder_button_pressed()) {
      if (edit_mode == EDIT_SELECT_STEP) {
        edit_mode = EDIT_NOTE;
        ui_clear();
        ui_show_edit_note(edit_step, seq_get_note(edit_step),
                          seq_get_velocity(edit_step));
      } else if (edit_mode == EDIT_NOTE) {
        edit_mode = EDIT_VELOCITY;
        ui_show_edit_note(edit_step, seq_get_note(edit_step),
                          seq_get_velocity(edit_step), true);
      } else if (edit_mode == EDIT_VELOCITY) {
        edit_mode = EDIT_NOTE;
        ui_show_edit_note(edit_step, seq_get_note(edit_step),
                          seq_get_velocity(edit_step), false);
      } else if (edit_mode == PATTERN_SELECT) {
        if (seq_get_load_mode() == LOAD_WAIT_END && seq_is_playing())
          seq_queue_pattern(temp_pattern_slot);
        else
          seq_load_pattern(temp_pattern_slot);
        pattern_slot = temp_pattern_slot;
        edit_mode = EDIT_NONE;
        ui_clear();
        ui_show_bpm(seq_get_bpm(), pattern_slot, clock_get_source(),
                    current_tstate, false, enc_mode == ENC_BPM_COARSE,
                    current_tstate == TSTATE_STOP ? 0 : seq_current_step(),
                    seq_get_steps(),
                    current_tstate == TSTATE_PAUSE ? !pause_icon_visible
                                                   : false,
                    seq_get_global_octave(), enc_mode == ENC_OCTAVE, seq_get_global_transpose(), enc_mode == ENC_TRANSPOSE);
        ui_show_steps(current_tstate != TSTATE_STOP ? seq_current_step()
                                                    : 0xFFFFFFFF,
                      seq_get_steps());
      } else if (edit_mode == SETTINGS) {
        if (settings_option == 0) {
          clock_set_source(clock_get_source() == CLOCK_INTERNAL
                               ? CLOCK_EXTERNAL
                               : CLOCK_INTERNAL);
        } else if (settings_option == 3) {
          PatternLoadMode mode = seq_get_load_mode();
          mode = (mode == LOAD_INSTANT) ? LOAD_WAIT_END : LOAD_INSTANT;
          seq_set_load_mode(mode);
        } else if (settings_option == 1 || settings_option == 2) {
          settings_edit_mode = !settings_edit_mode;
        }
        ui_show_settings(settings_option, clock_get_source(),
                         clock_get_gate_length(), clock_get_ppqn(),
                         seq_get_load_mode(), settings_edit_mode);
      } else if (edit_mode == PATTERN_TOOLS) {
        if (tools_selection == 1) { // Chaos Gates Card
          tools_edit_mode = !tools_edit_mode;
        } else if (tools_selection == 2) { // CLEAR Card
          if (!tools_edit_mode) {
            tools_edit_mode = true;
            clear_confirmed = false;
          } else {
            seq_reset_pattern();
            // Visual Confirmation
            ui_show_message("CLEARED!");
            sleep_ms(600);

            // Reset and go to Step Edit
            clear_confirmed = true;
            tools_edit_mode = false;
            edit_mode = EDIT_SELECT_STEP;
            edit_step = 0;
            ui_clear();
            ui_show_edit_step(edit_step, seq_get_note(edit_step));
          }
        } else {
          tools_edit_mode = !tools_edit_mode;
        }
        if (edit_mode == PATTERN_TOOLS) {
          ui_show_pattern_tools(tools_selection, tools_edit_mode, temp_density);
        }
      } else if (edit_mode == EDIT_NONE) {
        if (enc_mode == ENC_BPM_FINE)
          enc_mode = ENC_BPM_COARSE;
        else if (enc_mode == ENC_BPM_COARSE)
          enc_mode = ENC_OCTAVE;
        else if (enc_mode == ENC_OCTAVE)
          enc_mode = ENC_TRANSPOSE;
        else
          enc_mode = ENC_BPM_FINE;

        ui_show_bpm(seq_get_bpm(), pattern_slot, clock_get_source(),
                    current_tstate, false, enc_mode == ENC_BPM_COARSE,
                    current_tstate == TSTATE_STOP ? 0 : seq_current_step(),
                    seq_get_steps(),
                    current_tstate == TSTATE_PAUSE ? !pause_icon_visible
                                                   : false,
                    seq_get_global_octave(), enc_mode == ENC_OCTAVE, seq_get_global_transpose(), enc_mode == ENC_TRANSPOSE);
      }
    }

    int encoder_delta = io_encoder_poll_delta();
    if (encoder_delta != 0) {
      if (edit_mode == EDIT_SELECT_STEP) {
        if (io_is_step_button_pressed()) {
          if (encoder_delta > 0)
            seq_set_tie(edit_step, !seq_get_tie(edit_step));
          else
            seq_set_tie(edit_step, false);
          step_button_was_modified = true;
          ui_show_edit_step(edit_step, seq_get_note(edit_step));
        } else {
          int ns = (int)edit_step + encoder_delta;
          if (ns < 0)
            ns = 0;
          if (ns > 31)
            ns = 31;
          edit_step = (uint32_t)ns;
          ui_show_edit_step(edit_step, seq_get_note(edit_step));
        }
      } else if (edit_mode == EDIT_NOTE) {
        uint8_t curr = seq_get_note(edit_step);
        uint8_t next = seq_get_next_note_in_scale(curr, encoder_delta);
        seq_set_note(edit_step, next);
        ui_show_edit_note(edit_step, next, seq_get_velocity(edit_step));
      } else if (edit_mode == EDIT_VELOCITY) {
        int nv = (int)seq_get_velocity(edit_step) + encoder_delta;
        if (nv < 0)
          nv = 0;
        if (nv > 4)
          nv = 4;
        seq_set_velocity(edit_step, (uint8_t)nv);
        ui_show_edit_note(edit_step, seq_get_note(edit_step), (uint8_t)nv,
                          true);
      } else if (edit_mode == PATTERN_SELECT) {
        int ns = (int)temp_pattern_slot + encoder_delta;
        if (ns < 0)
          ns = 24;
        if (ns > 24)
          ns = 0;
        temp_pattern_slot = (uint8_t)ns;
        ui_show_pattern_select(temp_pattern_slot);
      } else if (edit_mode == SETTINGS) {
        if (!settings_edit_mode) {
          settings_option += encoder_delta;
          if (settings_option < 0)
            settings_option = 0;
          if (settings_option >= NUM_SETTINGS)
            settings_option = NUM_SETTINGS - 1;
        } else {
          if (settings_option == 1) {
            int len = (int)clock_get_gate_length() + encoder_delta * 10;
            if (len < 10)
              len = 10;
            if (len > 90)
              len = 90;
            clock_set_gate_length((uint8_t)len);
          } else if (settings_option == 2) {
            static const uint32_t ppqns[] = {4, 8, 12, 16, 24, 48};
            uint32_t curr = clock_get_ppqn();
            int idx = 4;
            for (int i = 0; i < 6; i++)
              if (ppqns[i] == curr)
                idx = i;
            idx += encoder_delta;
            if (idx < 0)
              idx = 0;
            if (idx > 5)
              idx = 5;
            clock_set_ppqn(ppqns[idx]);
          }
        }
        ui_show_settings(settings_option, clock_get_source(),
                         clock_get_gate_length(), clock_get_ppqn(),
                         seq_get_load_mode(), settings_edit_mode);
      } else if (edit_mode == PATTERN_TOOLS) {
        if (tools_edit_mode) {
          if (tools_selection == 0) { // Scale
            int gs = seq_get_global_scale() + encoder_delta;
            int ns = seq_get_num_scales();
            if (gs < 0)
              gs = ns - 1;
            if (gs >= ns)
              gs = 0;
            seq_set_global_scale(gs);
          } else if (tools_selection == 1) { // Random Gates
            int d = (int)temp_density + encoder_delta * 5;
            if (d < 0)
              d = 0;
            if (d > 100)
              d = 100;
            temp_density = (uint8_t)d;
            seq_randomize_gates(temp_density);
          }
        } else {
          tools_selection += encoder_delta;
          if (tools_selection < 0)
            tools_selection = 2; // 3 options: 0, 1, 2
          if (tools_selection > 2)
            tools_selection = 0;
        }
        ui_show_pattern_tools(tools_selection, tools_edit_mode, temp_density);
      } else if (edit_mode == EDIT_NONE) {
        if (io_is_step_button_pressed()) {
          int ns = (int)seq_get_steps() + encoder_delta;
          if (ns < 1)
            ns = 1;
          if (ns > 32)
            ns = 32;
          seq_set_steps((uint32_t)ns);
          // Show current step if playing, hide pointer if stopped
          uint32_t display_step;
          if (current_tstate == TSTATE_PLAY) {
            display_step = seq_current_step();
            if (display_step >= (uint32_t)ns) {
              display_step = 0;
            }
          } else {
            display_step = 0xFFFFFFFF; // No pointer
          }
          ui_show_steps(display_step, (uint32_t)ns);
          step_button_was_modified = true;
        } else {
          if (clock_get_source() == CLOCK_INTERNAL) {
            if (enc_mode == ENC_OCTAVE) {
              uint8_t min_n, max_n;
              seq_get_note_range(min_n, max_n);

              int8_t current_oct = seq_get_global_octave();
              int8_t current_tr = seq_get_global_transpose();
              int8_t target_oct = current_oct + (int8_t)encoder_delta;

              // Boundary-Aware Transposition (DAC Range: 36-84)
              if (min_n + (target_oct * 12) + current_tr >= 36 &&
                  max_n + (target_oct * 12) + current_tr <= 84) {
                seq_set_global_octave(target_oct);
              }
            } else if (enc_mode == ENC_TRANSPOSE) {
              uint8_t min_n, max_n;
              seq_get_note_range(min_n, max_n);

              int8_t current_oct = seq_get_global_octave();
              int8_t current_tr = seq_get_global_transpose();
              int8_t target_tr = current_tr + (int8_t)encoder_delta;

              // Boundary-Aware Transposition (DAC Range: 36-84)
              if (min_n + (current_oct * 12) + target_tr >= 36 &&
                  max_n + (current_oct * 12) + target_tr <= 84 &&
                  target_tr >= -11 && target_tr <= 11) {
                seq_set_global_transpose(target_tr);
              }
            } else {
              int step = (enc_mode == ENC_BPM_COARSE) ? 10 : 1;
              int nb = (int)seq_get_bpm() + encoder_delta * step;
              if (nb < 20)
                nb = 20;
              if (nb > 300)
                nb = 300;
              seq_set_bpm((uint32_t)nb);
              clock_set_bpm((uint32_t)nb);
            }
            ui_show_bpm(seq_get_bpm(), pattern_slot, clock_get_source(),
                        current_tstate, false, enc_mode == ENC_BPM_COARSE,
                        current_tstate == TSTATE_STOP ? 0 : seq_current_step(),
                        seq_get_steps(),
                        current_tstate == TSTATE_PAUSE ? !pause_icon_visible
                                                       : false,
                        seq_get_global_octave(), enc_mode == ENC_OCTAVE,
                        seq_get_global_transpose(), enc_mode == ENC_TRANSPOSE);
          }
        }
      }
    }

    // --- STEP BUTTON RELEASE LOGIC ---
    if (io_is_step_button_pressed()) {
      if (!step_button_down) {
        step_button_down = true;
        step_button_press_start_us = time_us_64();
        step_button_was_modified = false;
      }
    } else if (step_button_down) {
      uint64_t hold = time_us_64() - step_button_press_start_us;
      step_button_down = false;
      if (!step_button_was_modified && hold < LONG_PRESS_THRESHOLD_US) {
        if (edit_mode == EDIT_SELECT_STEP || edit_mode == EDIT_NOTE ||
            edit_mode == EDIT_VELOCITY) {
          seq_toggle_gate(edit_step);
          if (edit_mode == EDIT_SELECT_STEP || edit_mode == EDIT_VELOCITY)
            ui_show_edit_step(edit_step, seq_get_note(edit_step));
          else
            ui_show_edit_note(edit_step, seq_get_note(edit_step),
                              seq_get_velocity(edit_step));
        }
      }
    }

    if (clock_consume_step()) {
      uint32_t cur = seq_current_step();
      if (edit_mode == EDIT_NONE) {
        ui_show_steps(cur, seq_get_steps());
        ui_show_bpm(seq_get_bpm(), pattern_slot, clock_get_source(),
                    current_tstate,
                    seq_get_pending_pattern() >= 0 && (cur % 4 < 2),
                    enc_mode == ENC_BPM_COARSE, cur, seq_get_steps(), false,
                    seq_get_global_octave(), enc_mode == ENC_OCTAVE,
                    seq_get_global_transpose(), enc_mode == ENC_TRANSPOSE);
      }
      if (cur % 4 == 0)
        io_blink_led_start();
    }

    // --- UI/BLINKING LOGIC (PAUSE STATE) ---
    if (current_tstate == TSTATE_PAUSE && edit_mode == EDIT_NONE) {
      uint64_t now = time_us_64();
      if (now - pause_blink_timer > PAUSE_BLINK_INTERVAL_US) { // 500ms blink
        pause_blink_timer = now;
        pause_icon_visible = !pause_icon_visible;

        ui_show_bpm(seq_get_bpm(), pattern_slot, clock_get_source(),
                    current_tstate, false, enc_mode == ENC_BPM_COARSE,
                    seq_current_step(), seq_get_steps(), !pause_icon_visible,
                    seq_get_global_octave(), enc_mode == ENC_OCTAVE,
                    seq_get_global_transpose(), enc_mode == ENC_TRANSPOSE);
      }
    }

    tight_loop_contents();
  }
  return 0;
}