#pragma once

#include <cstdint>

// Initialize the SSD1306 display (I2C0, SDA=GP4, SCL=GP5, addr=0x3C)
void ui_init();

// Immediately update displayed BPM value (non-blocking)
void ui_show_bpm(uint32_t bpm);
