#include "ui.h"

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include <cstring>
#include <cstdio>

// SSD1306 basic config
static const int SDA_PIN = 4;
static const int SCL_PIN = 5;
static const uint8_t SSD1306_ADDR = 0x3C;

// 128x64 framebuffer (pages of 8 rows)
static uint8_t fb[128 * 8]; // 1024 bytes

// Minimal 5x7 font for digits and few letters
static const uint8_t font5x7[][5] = {
    // ' ' (space)
    {0x00,0x00,0x00,0x00,0x00},
    // '0'
    {0x3E,0x51,0x49,0x45,0x3E},
    // '1'
    {0x00,0x42,0x7F,0x40,0x00},
    // '2'
    {0x42,0x61,0x51,0x49,0x46},
    // '3'
    {0x21,0x41,0x45,0x4B,0x31},
    // '4'
    {0x18,0x14,0x12,0x7F,0x10},
    // '5'
    {0x27,0x45,0x45,0x45,0x39},
    // '6'
    {0x3C,0x4A,0x49,0x49,0x30},
    // '7'
    {0x01,0x71,0x09,0x05,0x03},
    // '8'
    {0x36,0x49,0x49,0x49,0x36},
    // '9'
    {0x06,0x49,0x49,0x29,0x1E},
    // ':'
    {0x00,0x36,0x36,0x00,0x00},
    // 'B'
    {0x7F,0x49,0x49,0x49,0x36},
    // 'P'
    {0x7F,0x09,0x09,0x09,0x06},
    // 'M' approximated
    {0x7F,0x20,0x18,0x20,0x7F}
};

// Map chars we use to indices in font5x7
static int char_to_font_index(char c) {
    if (c == ' ') return 0;
    if (c >= '0' && c <= '9') return 1 + (c - '0');
    if (c == ':') return 11;
    if (c == 'B') return 12;
    if (c == 'P') return 13;
    if (c == 'M') return 14;
    return 0; // fallback to space
}

static void ssd1306_write_command(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    i2c_write_blocking(i2c0, SSD1306_ADDR, buf, 2, false);
}

static void ssd1306_init_chip() {
    // Basic init sequence for SSD1306 128x64
    ssd1306_write_command(0xAE); // display off
    ssd1306_write_command(0x20); ssd1306_write_command(0x00);
    ssd1306_write_command(0xB0);
    ssd1306_write_command(0xC8);
    ssd1306_write_command(0x00);
    ssd1306_write_command(0x10);
    ssd1306_write_command(0x40);
    ssd1306_write_command(0x81); ssd1306_write_command(0x7F);
    ssd1306_write_command(0xA1);
    ssd1306_write_command(0xA6);
    ssd1306_write_command(0xA8); ssd1306_write_command(0x3F);
    ssd1306_write_command(0xA4);
    ssd1306_write_command(0xD3); ssd1306_write_command(0x00);
    ssd1306_write_command(0xD5); ssd1306_write_command(0xF0);
    ssd1306_write_command(0xD9); ssd1306_write_command(0x22);
    ssd1306_write_command(0xDA); ssd1306_write_command(0x12);
    ssd1306_write_command(0xDB); ssd1306_write_command(0x20);
    ssd1306_write_command(0x8D); ssd1306_write_command(0x14);
    ssd1306_write_command(0xAF); // display on
}

static void ssd1306_clear_fb() {
    memset(fb, 0x00, sizeof(fb));
}

static void ssd1306_update() {
    // send framebuffer by pages
    for (uint8_t page = 0; page < 8; ++page) {
        ssd1306_write_command(0xB0 | page);
        ssd1306_write_command(0x00);
        ssd1306_write_command(0x10);

        uint8_t buf[129];
        buf[0] = 0x40; // control byte for data
        memcpy(&buf[1], &fb[page * 128], 128);
        i2c_write_blocking(i2c0, SSD1306_ADDR, buf, 129, false);
    }
}

static void ui_draw_char(int x, int page, char c) {
    int idx = char_to_font_index(c);
    const uint8_t *glyph = font5x7[idx];
    if (x < 0 || x + 6 > 128) return;
    uint8_t *dst = &fb[page * 128 + x];
    for (int i = 0; i < 5; ++i) dst[i] = glyph[i];
    dst[5] = 0x00;
}

static void set_pixel(int x, int y) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    int page = y >> 3;
    int bit = y & 7;
    fb[page * 128 + x] |= (1u << bit);
}

static void draw_scaled_char(int x0, int y0, char c, int scale) {
    int idx = char_to_font_index(c);
    const uint8_t *glyph = font5x7[idx];
    // glyph: 5 columns, 7 rows (LSB top)
    for (int col = 0; col < 5; ++col) {
        uint8_t colbits = glyph[col];
        for (int row = 0; row < 7; ++row) {
            bool bit = (colbits >> row) & 1u;
            if (!bit) continue;
            // draw filled scaled pixel block
            int px = x0 + col * scale;
            int py = y0 + row * scale;
            for (int dx = 0; dx < scale; ++dx) {
                for (int dy = 0; dy < scale; ++dy) {
                    set_pixel(px + dx, py + dy);
                }
            }
        }
    }
}

void ui_init() {
    // init i2c0 with default 400kHz
    i2c_init(i2c0, 400000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
    // quick probe to see if a device ACKs at the address
    uint8_t probe = 0x00;
    int probe_res = i2c_write_blocking(i2c0, SSD1306_ADDR, &probe, 1, false);
    if (probe_res < 0) {
        // printf("SSD1306 not detected at 0x%02x (i2c write error %d)\n", SSD1306_ADDR, probe_res);
        return;
    }

    ssd1306_init_chip();
    ssd1306_clear_fb();
    ssd1306_update();
}

void ui_show_bpm(uint32_t bpm) {
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%u", (unsigned)bpm);
    if (len <= 0) return;

    // full-screen big digits: choose scale so digits are large but fit
    const int scale = 6; // 5*6=30px high per digit row (7*6=42px tall)
    const int char_w = 5 * scale + scale; // include spacing
    int total_w = len * char_w;
    int start_x = (128 - total_w) / 2;
    int start_y = (64 - (7 * scale)) / 2;

    ssd1306_clear_fb();
    for (int i = 0; i < len; ++i) {
        draw_scaled_char(start_x + i * char_w, start_y, buf[i], scale);
    }
    ssd1306_update();
}
