#include "ui.h"

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "sequencer.h"  // For seq_get_gate_enabled()

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
    // '0'-'9'
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9
    // ':'
    {0x00,0x36,0x36,0x00,0x00},
    // 'A'-'Z'
    {0x7E,0x09,0x09,0x09,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x7F,0x20,0x18,0x20,0x7F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z
    // '#'
    {0x14,0x7F,0x14,0x7F,0x14},
    // '/'
    {0x60,0x30,0x18,0x0C,0x06},
    // '>'
    {0x41,0x22,0x14,0x08,0x00},
    // '<'
    {0x08,0x14,0x22,0x41,0x00}
};

// Map chars we use to indices in font5x7
static int char_to_font_index(char c) {
    if (c == ' ') return 0;
    if (c >= '0' && c <= '9') return 1 + (c - '0');
    if (c == ':') return 11;
    if (c >= 'A' && c <= 'Z') return 12 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 12 + (c - 'a'); // treat lowercase as uppercase
    if (c == '#') return 38;
    if (c == '/') return 39;
    if (c == '>') return 40;
    if (c == '<') return 41;
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

static void ui_draw_text(int x, int page, const char *str) {
    int pos = x;
    while (*str) {
        ui_draw_char(pos, page, *str);
        pos += 6;
        str++;
    }
}

static void set_pixel(int x, int y) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    int page = y >> 3;
    int bit = y & 7;
    fb[page * 128 + x] |= (1u << bit);
}

static void clear_pixel(int x, int y) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    int page = y >> 3;
    int bit = y & 7;
    fb[page * 128 + x] &= ~(1u << bit);
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

void ui_boot_animation() {
    // Sequencer-style wave animation: 16 steps fill in, then pulse
    const int sq = 12;
    const int spacing = 4;
    
    // Phase 1: Steps fill in sequentially (like sequencer ticks)
    for (int step = 0; step < 16; step++) {
        ssd1306_clear_fb();
        
        // Draw all steps up to current
        for (int i = 0; i <= step; i++) {
            int row = (i < 8) ? 0 : 1;
            int col = i % 8;
            
            int bottom_y = 64 - sq;
            int top_y = bottom_y - sq - 8;
            int step_y = (row == 0) ? top_y : bottom_y;
            int x = col * (sq + spacing);
            
            // Fill square
            for (int dy = 0; dy < sq; dy++) {
                int y = step_y + dy;
                int page = y / 8;
                int bit = y % 8;
                for (int dx = 0; dx < sq; dx++) {
                    if (page < 8 && x + dx < 128) {
                        fb[page * 128 + x + dx] |= (1 << bit);
                    }
                }
            }
        }
        
        ssd1306_update();
        sleep_ms(70);
    }
    
    // Phase 2: Pulsing effect - all squares pulse 3 times
    for (int pulse = 0; pulse < 3; pulse++) {
        // Fill all
        ssd1306_clear_fb();
        for (int step = 0; step < 16; step++) {
            int row = (step < 8) ? 0 : 1;
            int col = step % 8;
            
            int bottom_y = 64 - sq;
            int top_y = bottom_y - sq - 8;
            int step_y = (row == 0) ? top_y : bottom_y;
            int x = col * (sq + spacing);
            
            for (int dy = 0; dy < sq; dy++) {
                int y = step_y + dy;
                int page = y / 8;
                int bit = y % 8;
                for (int dx = 0; dx < sq; dx++) {
                    if (page < 8 && x + dx < 128) {
                        fb[page * 128 + x + dx] |= (1 << bit);
                    }
                }
            }
        }
        ssd1306_update();
        sleep_ms(150);
        
        // Clear all
        ssd1306_clear_fb();
        ssd1306_update();
        sleep_ms(150);
    }
    
    // Phase 3: "CV-PICO-SEQ" text fade in
    ssd1306_clear_fb();
    const char* text = "CV-PICO-SEQ";
    int text_len = 11;
    int char_width = 6;
    int start_x = (128 - (text_len * char_width)) / 2;
    
    for (int i = 0; i < text_len; i++) {
        ui_draw_char(start_x + i * char_width, 3, text[i]);
    }
    ssd1306_update();
    sleep_ms(400);
    
    ssd1306_clear_fb();
    ssd1306_update();
}

void ui_clear() {
    ssd1306_clear_fb();
    ssd1306_update();
}

void ui_show_bpm(uint32_t bpm, uint8_t pattern_slot, bool blink_slot) {
    char numbuf[16];
    int numlen = snprintf(numbuf, sizeof(numbuf), "%u", (unsigned)bpm);
    if (numlen <= 0) return;
    
    // Larger BPM display at top-left using 2x scale
    // Clear entire top area for BPM (2 pages height, full width)
    for (int i = 0; i < 128; ++i) {
        fb[0 * 128 + i] = 0x00;
        fb[1 * 128 + i] = 0x00;
    }
    
    int x = 0;
    // Draw label "BPM:" with 2x scale
    const char *label = "BPM:";
    for (const char *p = label; *p; ++p) {
        draw_scaled_char(x, 0, *p, 2);
        x += 12;  // 5*2 + 2 spacing
    }
    // Draw BPM number with 2x scale
    for (int i = 0; i < numlen; ++i) {
        draw_scaled_char(x, 0, numbuf[i], 2);
        x += 12;
    }
    
    // Draw pattern slot on right side (P:0-9) - skip if blinking
    if (!blink_slot) {
        char slot_buf[8];
        snprintf(slot_buf, sizeof(slot_buf), "P:%d", pattern_slot);
        int slot_x = 128 - (strlen(slot_buf) * 12);  // Right align
        for (const char *p = slot_buf; *p; ++p) {
            draw_scaled_char(slot_x, 0, *p, 2);
            slot_x += 12;
        }
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

// Draw a filled circle (7x7 pixels)
static void draw_small_circle(int cx, int cy) {
    set_pixel(cx - 1, cy - 2);
    set_pixel(cx, cy - 2);
    set_pixel(cx + 1, cy - 2);
    
    set_pixel(cx - 2, cy - 1);
    set_pixel(cx - 1, cy - 1);
    set_pixel(cx, cy - 1);
    set_pixel(cx + 1, cy - 1);
    set_pixel(cx + 2, cy - 1);
    
    set_pixel(cx - 2, cy);
    set_pixel(cx - 1, cy);
    set_pixel(cx, cy);
    set_pixel(cx + 1, cy);
    set_pixel(cx + 2, cy);
    
    set_pixel(cx - 2, cy + 1);
    set_pixel(cx - 1, cy + 1);
    set_pixel(cx, cy + 1);
    set_pixel(cx + 1, cy + 1);
    set_pixel(cx + 2, cy + 1);
    
    set_pixel(cx - 1, cy + 2);
    set_pixel(cx, cy + 2);
    set_pixel(cx + 1, cy + 2);
}

// Draw a filled circle with inverse (clear pixels instead of set)
static void draw_small_circle_inverse(int cx, int cy) {
    clear_pixel(cx - 1, cy - 2);
    clear_pixel(cx, cy - 2);
    clear_pixel(cx + 1, cy - 2);
    
    clear_pixel(cx - 2, cy - 1);
    clear_pixel(cx - 1, cy - 1);
    clear_pixel(cx, cy - 1);
    clear_pixel(cx + 1, cy - 1);
    clear_pixel(cx + 2, cy - 1);
    
    clear_pixel(cx - 2, cy);
    clear_pixel(cx - 1, cy);
    clear_pixel(cx, cy);
    clear_pixel(cx + 1, cy);
    clear_pixel(cx + 2, cy);
    
    clear_pixel(cx - 2, cy + 1);
    clear_pixel(cx - 1, cy + 1);
    clear_pixel(cx, cy + 1);
    clear_pixel(cx + 1, cy + 1);
    clear_pixel(cx + 2, cy + 1);
    
    clear_pixel(cx - 1, cy + 2);
    clear_pixel(cx, cy + 2);
    clear_pixel(cx + 1, cy + 2);
}

void ui_show_steps(uint32_t current_step, uint32_t steps) {
    if (steps == 0) return;
    const int sq = 12;
    const int spacing = 4;
    const int cols = 8;
    const int total_w = cols * sq + (cols - 1) * spacing; // 124
    const int left = (128 - total_w) / 2;
    // Move to bottom: screen is 64 pixels high, 2 rows of squares with gap
    const int bottom_y = 64 - sq;  // Bottom row at very bottom
    const int top_y = bottom_y - sq - 8;  // Top row above with 8px gap

    // Clear grid area
    clear_region(left - 1, top_y - 1, total_w + 2, (sq * 2) + 8 + 2);

    for (int i = 0; i < (int)steps && i < 16; ++i) {
        int col = i % cols;
        int row = i / cols;
        int x = left + col * (sq + spacing);
        int y = (row == 0) ? top_y : bottom_y;
        
        bool is_current = (i == (int)current_step && current_step < steps);
        bool gate_enabled = seq_get_gate_enabled(i);
        
        if (is_current) {
            fill_rect(x + 2, y + 2, sq - 4, sq - 4);
        } else {
            draw_rect_outline(x, y, sq, sq);
        }
        
        if (gate_enabled) {
            if (is_current) {
                clear_region(x + 3, y + 3, 6, 6);
            } else {
                fill_rect(x + 3, y + 3, 6, 6);
            }
        }
    }

    ssd1306_update();
}

// Helper: Convert MIDI note to name (e.g., 48 -> "C3")
static void note_to_string(uint8_t note, char *buf) {
    const char *notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (note / 12) - 1;  // MIDI: C-1=0, C0=12, C1=24, C2=36, C3=48, etc.
    int semitone = note % 12;
    sprintf(buf, "%s%d", notes[semitone], octave);
}

void ui_show_edit_step(uint32_t selected_step, uint8_t note) {
    ssd1306_clear_fb();
    
    // Title
    ui_draw_text(0, 0, "STEP SELECT");
    
    // Step and Note on same line
    char buf[32];
    char note_str[8];
    note_to_string(note, note_str);
    sprintf(buf, "Step:%02d  Note:%s", selected_step + 1, note_str);
    ui_draw_text(0, 2, buf);
    
    // Draw step grid with highlight and gate indicators (larger squares)
    const int cols = 8;
    const int sq = 12;
    const int spacing = 4;
    const int total_w = cols * sq + (cols - 1) * spacing;
    const int left = (128 - total_w) / 2;
    const int bottom_y = 64 - sq;
    const int top_y = bottom_y - sq - 8;
    
    for (int i = 0; i < 16; ++i) {
        int col = i % cols;
        int row = i / cols;
        int x = left + col * (sq + spacing);
        int step_y = (row == 0) ? top_y : bottom_y;
        
        bool is_selected = (i == (int)selected_step);
        bool gate_enabled = seq_get_gate_enabled(i);
        
        if (is_selected) {
            fill_rect(x + 2, step_y + 2, sq - 4, sq - 4);
        } else {
            draw_rect_outline(x, step_y, sq, sq);
        }
        
        if (gate_enabled) {
            if (is_selected) {
                clear_region(x + 3, step_y + 3, 6, 6);
            } else {
                fill_rect(x + 3, step_y + 3, 6, 6);
            }
        }
    }
    
    ssd1306_update();
}

void ui_show_edit_note(uint32_t step, uint8_t note) {
    ssd1306_clear_fb();
    
    // Title
    ui_draw_text(0, 0, "NOTE EDIT");
    
    // Step number (1-16)
    char buf[32];
    sprintf(buf, "Step: %02d", step + 1);
    ui_draw_text(0, 2, buf);
    
    // Note name (large)
    char note_str[8];
    note_to_string(note, note_str);
    sprintf(buf, ">> %s <<", note_str);
    ui_draw_text(20, 4, buf);
    
    // Gate status
    bool gate_on = seq_get_gate_enabled(step);
    sprintf(buf, "Gate: %s", gate_on ? "ON" : "OFF");
    ui_draw_text(0, 6, buf);
    
    ssd1306_update();
}

void ui_show_pattern_select(uint8_t slot) {
    ssd1306_clear_fb();
    
    // Title (centered)
    ui_draw_text(22, 0, "PATTERN SELECT");
    
    // Slot number large and centered (3x scale, centered vertically and horizontally)
    char slot_char = '0' + slot;
    draw_scaled_char(56, 24, slot_char, 3);  // Vertically centered
    
    // Instructions
    ui_draw_text(36, 7, "LOAD/SAVE");
    
    ssd1306_update();
}
