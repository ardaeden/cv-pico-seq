#pragma once

#include <cstdint>

// Configure clock interval based on BPM and PPQN.
void clock_set_bpm(uint32_t bpm);
void clock_set_ppqn(uint32_t ppqn);

// Launch the timing core (core1) that generates ticks.
void clock_launch_core1();

// Check and clear a pending tick produced by core1.
bool clock_consume_tick();

// Return the current clock interval in microseconds (microseconds per tick).
uint32_t clock_get_interval_us();
