#pragma once

#include <cstdint>

void eeprom_init();

void eeprom_write_pattern(uint8_t slot, const uint8_t* notes, uint16_t gate_mask);

void eeprom_read_pattern(uint8_t slot, uint8_t* notes, uint16_t* gate_mask);

bool eeprom_is_initialized();

bool eeprom_has_valid_data();

void eeprom_mark_valid();
