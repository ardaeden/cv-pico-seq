#pragma once

#include <cstdint>

// Configure clock interval based on BPM and PPQN.
enum ClockSource { CLOCK_INTERNAL, CLOCK_EXTERNAL };

void clock_set_source(ClockSource source);
ClockSource clock_get_source();
void clock_set_bpm(uint32_t bpm);
void clock_set_gate_length(uint8_t percent);
uint8_t clock_get_gate_length();
void clock_set_ppqn(uint32_t ppqn);
uint32_t clock_get_ppqn();

// Launch the timing core (core1) that generates ticks.
void clock_init();
void clock_launch_core1();

// Check and clear a pending sequencer step advancement produced by core1.
bool clock_consume_step();

// Enable/disable gate output (call when play/pause)
void clock_gate_enable(bool enable);

// Enable/disable the 24 PPQN clock output on GPIO22
void clock_out_enable(bool enable);

// Reset internal tick counter (call when stopping or before playing)
void clock_reset();

// Set CV/DAC value (updated on next tick by core 1)
void clock_set_cv(uint16_t dac_value);

// Reset the internal us_counter to align with a start event
void clock_restart();

// Resume clock without resetting tick counters (for Pause -> Play)
void clock_resume();
