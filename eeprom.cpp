#include "eeprom.h"

#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <cstring>

namespace {
constexpr uint8_t EEPROM_BASE_ADDR = 0x50;
constexpr uint SDA_PIN = 26;
constexpr uint SCL_PIN = 27;
constexpr uint8_t PATTERN_STORAGE_SIZE = 18;
constexpr uint8_t NUM_PATTERNS = 10;
constexpr uint8_t MAGIC_BYTE = 0xAA;
constexpr uint16_t MAGIC_ADDR = 1900;

bool initialized = false;
}

void eeprom_init() {
    i2c_init(i2c1, 400000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
    
    sleep_ms(50);
    
    uint8_t probe = 0x00;
    int result = i2c_write_blocking(i2c1, EEPROM_BASE_ADDR, &probe, 1, false);
    initialized = (result >= 0);
    
    if (initialized) {
        sleep_ms(10);
    }
}

bool eeprom_is_initialized() {
    return initialized;
}

void eeprom_write_pattern(uint8_t slot, const uint8_t* notes, uint16_t gate_mask) {
    if (!initialized || slot >= NUM_PATTERNS) return;
    
    uint16_t addr = slot * PATTERN_STORAGE_SIZE;
    
    for (int i = 0; i < 16; i++) {
        uint16_t byte_addr = addr + i;
        uint8_t i2c_addr = EEPROM_BASE_ADDR | ((byte_addr >> 8) & 0x07);
        uint8_t local_addr = byte_addr & 0xFF;
        
        uint8_t buf[2] = {local_addr, notes[i]};
        i2c_write_blocking(i2c1, i2c_addr, buf, 2, false);
        sleep_ms(5);
    }
    
    uint16_t gate_addr = addr + 16;
    uint8_t gate_i2c_addr = EEPROM_BASE_ADDR | ((gate_addr >> 8) & 0x07);
    uint8_t gate_local_addr = gate_addr & 0xFF;
    uint8_t buf1[2] = {gate_local_addr, (uint8_t)((gate_mask >> 8) & 0xFF)};
    i2c_write_blocking(i2c1, gate_i2c_addr, buf1, 2, false);
    sleep_ms(5);
    
    gate_addr++;
    gate_i2c_addr = EEPROM_BASE_ADDR | ((gate_addr >> 8) & 0x07);
    gate_local_addr = gate_addr & 0xFF;
    uint8_t buf2[2] = {gate_local_addr, (uint8_t)(gate_mask & 0xFF)};
    i2c_write_blocking(i2c1, gate_i2c_addr, buf2, 2, false);
    sleep_ms(5);
}

void eeprom_read_pattern(uint8_t slot, uint8_t* notes, uint16_t* gate_mask) {
    if (!initialized || slot >= NUM_PATTERNS) return;
    
    uint16_t addr = slot * PATTERN_STORAGE_SIZE;
    uint8_t i2c_addr = EEPROM_BASE_ADDR | ((addr >> 8) & 0x07);
    uint8_t local_addr = addr & 0xFF;
    
    i2c_write_blocking(i2c1, i2c_addr, &local_addr, 1, true);
    i2c_read_blocking(i2c1, i2c_addr, notes, 16, false);
    
    uint16_t gate_addr = addr + 16;
    uint8_t gate_i2c_addr = EEPROM_BASE_ADDR | ((gate_addr >> 8) & 0x07);
    uint8_t gate_local_addr = gate_addr & 0xFF;
    
    i2c_write_blocking(i2c1, gate_i2c_addr, &gate_local_addr, 1, true);
    
    uint8_t gate_data[2];
    i2c_read_blocking(i2c1, gate_i2c_addr, gate_data, 2, false);
    *gate_mask = ((uint16_t)gate_data[0] << 8) | gate_data[1];
}

bool eeprom_has_valid_data() {
    if (!initialized) return false;
    
    uint8_t i2c_addr = EEPROM_BASE_ADDR | ((MAGIC_ADDR >> 8) & 0x07);
    uint8_t local_addr = MAGIC_ADDR & 0xFF;
    
    i2c_write_blocking(i2c1, i2c_addr, &local_addr, 1, true);
    
    uint8_t magic = 0;
    i2c_read_blocking(i2c1, i2c_addr, &magic, 1, false);
    
    return (magic == MAGIC_BYTE);
}

void eeprom_mark_valid() {
    if (!initialized) return;
    
    uint8_t i2c_addr = EEPROM_BASE_ADDR | ((MAGIC_ADDR >> 8) & 0x07);
    uint8_t local_addr = MAGIC_ADDR & 0xFF;
    
    uint8_t buf[2];
    buf[0] = local_addr;
    buf[1] = MAGIC_BYTE;
    i2c_write_blocking(i2c1, i2c_addr, buf, 2, false);
    sleep_ms(10);
}
