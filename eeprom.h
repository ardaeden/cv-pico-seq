#pragma once

#include <cstdint>

void eeprom_init();

void eeprom_write_pattern(uint8_t slot, const uint8_t *notes,
                          const uint8_t *velocities, uint32_t gate_mask,
                          uint32_t tie_mask, uint8_t steps);

void eeprom_read_pattern(uint8_t slot, uint8_t *notes, uint8_t *velocities,
                         uint32_t *gate_mask, uint32_t *tie_mask,
                         uint8_t *steps);

bool eeprom_is_initialized();

bool eeprom_has_valid_data();

void eeprom_mark_valid();
