#pragma once

#include <cstdint>

// Configure clock interval based on BPM.
void clock_set_bpm(uint32_t bpm);

// Launch the timing core (core1) that generates ticks.
void clock_launch_core1();

// Check and clear a pending tick produced by core1.
bool clock_consume_tick();
