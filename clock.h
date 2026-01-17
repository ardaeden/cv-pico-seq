#pragma once

#include <cstdint>

// Configure clock interval based on BPM and PPQN.
enum ClockSource { CLOCK_INTERNAL, CLOCK_EXTERNAL };

void clock_set_source(ClockSource source);
ClockSource clock_get_source();
void clock_set_bpm(uint32_t bpm);

// Launch the timing core (core1) that generates ticks.
void clock_launch_core1();

// Check and clear a pending tick produced by core1.
bool clock_consume_tick();

// Enable/disable gate output (call when play/pause)
void clock_gate_enable(bool enable);

// Enable/disable the 24 PPQN clock output on GPIO22
void clock_out_enable(bool enable);

// Set CV/DAC value (updated on next tick by core 1)
void clock_set_cv(uint16_t dac_value);

// Reset the internal us_counter to align with a start event
void clock_restart();
