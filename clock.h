#pragma once

#include <cstdint>

// Configure clock interval based on BPM and PPQN.
void clock_set_bpm(uint32_t bpm);

// Launch the timing core (core1) that generates ticks.
void clock_launch_core1();

// Check and clear a pending tick produced by core1.
bool clock_consume_tick();

// Enable/disable gate output (call when play/pause)
void clock_gate_enable(bool enable);

// Set CV/DAC value (updated on next tick by core 1)
void clock_set_cv(uint16_t dac_value);
