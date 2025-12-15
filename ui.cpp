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
        return;
    }

    ssd1306_init_chip();
    ssd1306_clear_fb();
    ssd1306_update();
}

void ui_show_bpm(uint32_t bpm) {
    char numbuf[16];
    int numlen = snprintf(numbuf, sizeof(numbuf), "%u", (unsigned)bpm);
    if (numlen <= 0) return;
    // Small BPM display at top-left using 5x7 font
    // Erase only page 0 area where BPM is displayed (do not clear whole fb)
    for (int i = 0; i < 128; ++i) fb[0 * 128 + i] = 0x00;
    int x = 0;
    // Draw label "BPM:" (font contains B, P, M and ':')
    const char *label = "BPM:";
    for (const char *p = label; *p; ++p) {
        ui_draw_char(x, 0, *p);
        x += 6;
    }
    // Small gap before number (already accounted by char spacing)
    for (int i = 0; i < numlen; ++i) {
        ui_draw_char(x, 0, numbuf[i]);
        x += 6;
    }
    ssd1306_update();
}

// Helper: clear rectangular region (inclusive) in pixel coords
static void clear_region(int x0, int y0, int w, int h) {
    if (w <= 0 || h <= 0) return;
    int x1 = x0 + w - 1;
    int y1 = y0 + h - 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= 128) x1 = 127;
    if (y1 >= 64) y1 = 63;
    for (int y = y0; y <= y1; ++y) {
        int page = y >> 3;
        int bit = y & 7;
        for (int x = x0; x <= x1; ++x) {
            fb[page * 128 + x] &= ~(1u << bit);
        }
    }
}

static void draw_rect_outline(int x0, int y0, int w, int h) {
    for (int x = x0; x < x0 + w; ++x) {
        set_pixel(x, y0);
        set_pixel(x, y0 + h - 1);
    }
    for (int y = y0; y < y0 + h; ++y) {
        set_pixel(x0, y);
        set_pixel(x0 + w - 1, y);
    }
}

static void fill_rect(int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; ++y) {
        for (int x = x0; x < x0 + w; ++x) {
            set_pixel(x, y);
        }
    }
}

void ui_show_steps(uint32_t current_step, uint32_t steps) {
    if (steps == 0) return;
    const int sq = 12;
    const int spacing = 4;
    const int cols = 8;
    const int total_w = cols * sq + (cols - 1) * spacing; // 124
    const int left = (128 - total_w) / 2;
    const int top_y = 20;
    const int bottom_y = top_y + sq + 8;

    // Clear grid area
    clear_region(left - 1, top_y - 1, total_w + 2, (sq * 2) + 8 + 2);

    for (int i = 0; i < (int)steps && i < 16; ++i) {
        int col = i % cols;
        int row = i / cols;
        int x = left + col * (sq + spacing);
        int y = (row == 0) ? top_y : bottom_y;
        draw_rect_outline(x, y, sq, sq);
    }

    if (current_step < steps && current_step < 16) {
        int col = current_step % cols;
        int row = current_step / cols;
        int x = left + col * (sq + spacing);
        int y = (row == 0) ? top_y : bottom_y;
        fill_rect(x + 2, y + 2, sq - 4, sq - 4);
    }

    ssd1306_update();
}
