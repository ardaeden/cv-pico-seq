#include "eeprom.h"

#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <cstring>

namespace {
constexpr uint8_t EEPROM_ADDR = 0x50;
constexpr uint SDA_PIN = 26;
constexpr uint SCL_PIN = 27;
constexpr uint8_t PATTERN_SIZE = 16;
constexpr uint8_t PATTERN_STORAGE_SIZE = 18;
constexpr uint8_t NUM_PATTERNS = 10;
constexpr uint8_t MAGIC_BYTE = 0xAA;
constexpr uint8_t MAGIC_ADDR = 180;

bool initialized = false;
}

void eeprom_init() {
    i2c_init(i2c1, 100000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
    
    uint8_t probe = 0x00;
    int result = i2c_write_blocking(i2c1, EEPROM_ADDR, &probe, 1, false);
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
    
    uint8_t addr = slot * PATTERN_STORAGE_SIZE;
    
    uint8_t buf1[17];
    buf1[0] = addr;
    memcpy(&buf1[1], notes, 16);
    i2c_write_blocking(i2c1, EEPROM_ADDR, buf1, 17, false);
    sleep_ms(10);
    
    uint8_t buf2[3];
    buf2[0] = addr + 16;
    buf2[1] = (gate_mask >> 8) & 0xFF;
    buf2[2] = gate_mask & 0xFF;
    i2c_write_blocking(i2c1, EEPROM_ADDR, buf2, 3, false);
    sleep_ms(10);
}

void eeprom_read_pattern(uint8_t slot, uint8_t* notes, uint16_t* gate_mask) {
    if (!initialized || slot >= NUM_PATTERNS) return;
    
    uint8_t addr = slot * PATTERN_STORAGE_SIZE;
    
    i2c_write_blocking(i2c1, EEPROM_ADDR, &addr, 1, true);
    
    uint8_t data[18];
    i2c_read_blocking(i2c1, EEPROM_ADDR, data, 18, false);
    
    memcpy(notes, data, PATTERN_SIZE);
    *gate_mask = ((uint16_t)data[16] << 8) | data[17];
}

bool eeprom_has_valid_data() {
    if (!initialized) return false;
    
    i2c_write_blocking(i2c1, EEPROM_ADDR, &MAGIC_ADDR, 1, true);
    
    uint8_t magic = 0;
    i2c_read_blocking(i2c1, EEPROM_ADDR, &magic, 1, false);
    
    return (magic == MAGIC_BYTE);
}

void eeprom_mark_valid() {
    if (!initialized) return;
    
    uint8_t buf[2];
    buf[0] = MAGIC_ADDR;
    buf[1] = MAGIC_BYTE;
    i2c_write_blocking(i2c1, EEPROM_ADDR, buf, 2, false);
    sleep_ms(10);
}
