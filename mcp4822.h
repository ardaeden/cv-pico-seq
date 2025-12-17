#pragma once

#include <cstdint>

// Initialize MCP4822 driver on SPI0 with given CS pin.
void mcp4822_init(unsigned int cs_pin);

// Set MCP4822 channel output (fixed 2X gain for 0-4.096V range)
// channel: 0 = A, 1 = B
// value: 0..4095 (12-bit)
void mcp4822_set_voltage(uint8_t channel, uint16_t value);
