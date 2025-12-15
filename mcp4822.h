#pragma once

#include <cstdint>

// Initialize MCP4822 driver on SPI0 with given CS pin.
void mcp4822_init(unsigned int cs_pin);

// Set MCP4822 channel output.
// channel: 0 = A, 1 = B
// gain: 0 = 1X, 1 = 2X (matches sample code semantics)
// value: 0..4095
void mcp4822_set_voltage(uint8_t channel, uint8_t gain, uint16_t value);
