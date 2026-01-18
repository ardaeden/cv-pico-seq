#include "ui.h"

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "sequencer.h"
#include <cstdlib>

#include <cstdio>
#include <cstring>

static const char *velo_names[] = {"pp", "p", "mf", "f", "ff"};

static const int SDA_PIN = 4;
static const int SCL_PIN = 5;
static const uint8_t SSD1306_ADDR = 0x3C;

static uint8_t fb[128 * 8];

static int32_t ui_edit_step_prev_step = -1;
static uint8_t ui_edit_step_prev_note = 0;
static uint32_t ui_edit_step_prev_gate = 0xFFFFFFFF;
static uint8_t ui_edit_note_prev_note = 255;
static bool ui_edit_note_prev_gate = false;
static uint32_t ui_edit_note_prev_step = 255;
static uint8_t ui_edit_note_prev_velocity = 255;
static int8_t ui_pattern_select_prev_slot = -1;

static void fill_rect(int x0, int y0, int w, int h);

static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00},                                 // 1
    {0x42, 0x61, 0x51, 0x49, 0x46},                                 // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31},                                 // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10},                                 // 4
    {0x27, 0x45, 0x45, 0x45, 0x39},                                 // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30},                                 // 6
    {0x01, 0x71, 0x09, 0x05, 0x03},                                 // 7
    {0x36, 0x49, 0x49, 0x49, 0x36},                                 // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E},                                 // 9
    {0x00, 0x36, 0x36, 0x00, 0x00},                                 // :
    {0x7E, 0x09, 0x09, 0x09, 0x7E},                                 // A
    {0x7F, 0x49, 0x49, 0x49, 0x36},                                 // B
    {0x3E, 0x41, 0x41, 0x41, 0x22},                                 // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C},                                 // D
    {0x7F, 0x49, 0x49, 0x49, 0x41},                                 // E
    {0x7F, 0x09, 0x09, 0x09, 0x01},                                 // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A},                                 // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F},                                 // H
    {0x00, 0x41, 0x7F, 0x41, 0x00},                                 // I
    {0x20, 0x40, 0x41, 0x3F, 0x01},                                 // J
    {0x7F, 0x08, 0x14, 0x22, 0x41},                                 // K
    {0x7F, 0x40, 0x40, 0x40, 0x40},                                 // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},                                 // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F},                                 // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E},                                 // O
    {0x7F, 0x09, 0x09, 0x09, 0x06},                                 // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E},                                 // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46},                                 // R
    {0x46, 0x49, 0x49, 0x49, 0x31},                                 // S
    {0x01, 0x01, 0x7F, 0x01, 0x01},                                 // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F},                                 // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F},                                 // V
    {0x7F, 0x20, 0x18, 0x20, 0x7F},                                 // W
    {0x63, 0x14, 0x08, 0x14, 0x63},                                 // X
    {0x07, 0x08, 0x70, 0x08, 0x07},                                 // Y
    {0x61, 0x51, 0x49, 0x45, 0x43},                                 // Z
    {0x14, 0x7F, 0x14, 0x7F, 0x14},                                 // #
    {0x60, 0x30, 0x18, 0x0C, 0x06},                                 // /
    {0x41, 0x22, 0x14, 0x08, 0x00},                                 // 40: >
    {0x08, 0x14, 0x22, 0x41, 0x00},                                 // 41: <
    {0x20, 0x54, 0x54, 0x54, 0x38},                                 // 42: a
    {0x7C, 0x04, 0x04, 0x04, 0x78},                                 // 43: n
    {0x38, 0x44, 0x44, 0x44, 0x38}                                  // 44: o
};

static int char_to_font_index(char c) {
  if (c == ' ')
    return 0;
  if (c >= '0' && c <= '9')
    return 1 + (c - '0');
  if (c == ':')
    return 11;
  if (c >= 'A' && c <= 'Z')
    return 12 + (c - 'A');
  if (c == 'a')
    return 42;
  if (c == 'n')
    return 43;
  if (c == 'o')
    return 44;
  if (c >= 'b' && c <= 'z') // Fallback for other lowercase
    return 12 + (c - 'a');
  if (c == '#')
    return 38;
  if (c == '/')
    return 39;
  if (c == '>')
    return 40;
  if (c == '<')
    return 41;
  return 0;
}

static void ssd1306_write_command(uint8_t cmd) {
  uint8_t buf[2] = {0x00, cmd};
  i2c_write_blocking(i2c0, SSD1306_ADDR, buf, 2, false);
}

static void ssd1306_init_chip() {
  // Basic init sequence for SSD1306 128x64
  ssd1306_write_command(0xAE); // display off
  ssd1306_write_command(0x20);
  ssd1306_write_command(0x00);
  ssd1306_write_command(0xB0);
  ssd1306_write_command(0xC8);
  ssd1306_write_command(0x00);
  ssd1306_write_command(0x10);
  ssd1306_write_command(0x40);
  ssd1306_write_command(0x81);
  ssd1306_write_command(0x7F);
  ssd1306_write_command(0xA1);
  ssd1306_write_command(0xA6);
  ssd1306_write_command(0xA8);
  ssd1306_write_command(0x3F);
  ssd1306_write_command(0xA4);
  ssd1306_write_command(0xD3);
  ssd1306_write_command(0x00);
  ssd1306_write_command(0xD5);
  ssd1306_write_command(0xF0);
  ssd1306_write_command(0xD9);
  ssd1306_write_command(0x22);
  ssd1306_write_command(0xDA);
  ssd1306_write_command(0x12);
  ssd1306_write_command(0xDB);
  ssd1306_write_command(0x20);
  ssd1306_write_command(0x8D);
  ssd1306_write_command(0x14);
  ssd1306_write_command(0xAF); // display on
}

static void ssd1306_clear_fb() { memset(fb, 0x00, sizeof(fb)); }

void ssd1306_update() {
  for (uint8_t page = 0; page < 8; ++page) {
    ssd1306_write_command(0xB0 | page);
    ssd1306_write_command(0x00);
    ssd1306_write_command(0x10);

    uint8_t buf[129];
    buf[0] = 0x40;
    memcpy(&buf[1], &fb[page * 128], 128);
    i2c_write_blocking(i2c0, SSD1306_ADDR, buf, 129, false);
  }
}

static void ui_draw_char(int x, int page, char c) {
  int idx = char_to_font_index(c);
  const uint8_t *glyph = font5x7[idx];
  if (x < 0 || x + 6 > 128)
    return;
  uint8_t *dst = &fb[page * 128 + x];
  for (int i = 0; i < 5; ++i)
    dst[i] = glyph[i];
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

static void ui_draw_text_inverted(int x, int page, const char *str) {
  int pos = x;
  while (*str) {
    int idx = char_to_font_index(*str);
    const uint8_t *glyph = font5x7[idx];
    if (pos >= 0 && pos + 6 <= 128) {
      uint8_t *dst = &fb[page * 128 + pos];
      for (int i = 0; i < 5; ++i)
        dst[i] = ~glyph[i];
      dst[5] = 0xFF;
    }
    pos += 6;
    str++;
  }
}

static void set_pixel(int x, int y) {
  if (x < 0 || x >= 128 || y < 0 || y >= 64)
    return;
  int page = y >> 3;
  int bit = y & 7;
  fb[page * 128 + x] |= (1u << bit);
}

static void clear_pixel(int x, int y) {
  if (x < 0 || x >= 128 || y < 0 || y >= 64)
    return;
  int page = y >> 3;
  int bit = y & 7;
  fb[page * 128 + x] &= ~(1u << bit);
}

void draw_scaled_char(int x0, int y0, char c, int scale) {
  int idx = char_to_font_index(c);
  const uint8_t *glyph = font5x7[idx];
  // glyph: 5 columns, 7 rows (LSB top)
  for (int col = 0; col < 5; ++col) {
    uint8_t colbits = glyph[col];
    for (int row = 0; row < 7; ++row) {
      bool bit = (colbits >> row) & 1u;
      if (!bit)
        continue;
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
  i2c_init(i2c0, 1000000);
  gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(SDA_PIN);
  gpio_pull_up(SCL_PIN);

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
  // Brand: aAOn
  // Subtitle: CV Sequencer

  // 1. Line expansion from center
  for (int w = 0; w < 128; w += 8) {
    ssd1306_clear_fb();
    int x0 = 64 - w / 2;
    for (int x = x0; x < x0 + w; x++) {
      set_pixel(x, 32);
    }
    ssd1306_update();
    sleep_ms(10);
  }

  // 2. Line splits and moves to top/bottom
  for (int y = 0; y < 32; y += 4) {
    ssd1306_clear_fb();
    for (int x = 0; x < 128; x++) {
      set_pixel(x, 32 - y);
      set_pixel(x, 32 + y);
    }
    ssd1306_update();
    sleep_ms(15);
  }

  // 3. "aAOn" Fades in with scale pulse
  const char *brand = "aAOn";
  for (int scale = 1; scale <= 4; scale++) {
    ssd1306_clear_fb();
    // Re-draw border lines
    for (int x = 0; x < 128; x++) {
      set_pixel(x, 0);
      set_pixel(x, 63);
    }

    int brand_w = (5 * scale + 2) * 4;
    int brand_x = (128 - brand_w) / 2;
    int brand_y = (64 - (7 * scale)) / 2;

    for (int i = 0; i < 4; i++) {
      draw_scaled_char(brand_x + i * (5 * scale + 2), brand_y, brand[i], scale);
    }
    ssd1306_update();
    sleep_ms(50);
  }

  // 4. Glitch Effect
  for (int g = 0; g < 5; g++) {
    for (int i = 0; i < 20; i++) {
      set_pixel(rand() % 128, rand() % 64);
    }
    ssd1306_update();
    sleep_ms(30);
  }

  // 5. Add "CV Sequencer" subtitle
  ssd1306_clear_fb();
  // Border lines
  for (int x = 0; x < 128; x++) {
    set_pixel(x, 0);
    set_pixel(x, 63);
  }

  // aAOn at scale 4
  int scale = 4;
  int brand_w = (5 * scale + 2) * 4;
  int brand_x = (128 - brand_w) / 2;
  int brand_y = 10;
  for (int i = 0; i < 4; i++) {
    draw_scaled_char(brand_x + i * (5 * scale + 2), brand_y, brand[i], scale);
  }

  // CV Sequencer at scale 1
  const char *sub = "CV SEQUENCER";
  int sub_w = strlen(sub) * 6;
  int sub_x = (128 - sub_w) / 2;
  ui_draw_text(sub_x, 6, sub);

  // Firmware Version
  char ver_buf[16];
  snprintf(ver_buf, sizeof(ver_buf), "%s", FIRMWARE_VERSION_STR);
  int ver_w = strlen(ver_buf) * 6;
  int ver_x = (128 - ver_w) / 2;
  ui_draw_text(ver_x, 7, ver_buf);

  ssd1306_update();
  sleep_ms(1500);

  ssd1306_clear_fb();
  ssd1306_update();
}

void ui_clear() {
  ssd1306_clear_fb();
  ssd1306_update();

  ui_edit_step_prev_step = -1;
  ui_edit_step_prev_note = 0;
  ui_edit_step_prev_gate = 0xFFFF;
  ui_edit_note_prev_note = 255;
  ui_edit_note_prev_gate = false;
  ui_edit_note_prev_step = 255;
  ui_pattern_select_prev_slot = -1;
}

void ui_show_bpm(uint32_t bpm, uint8_t pattern_slot, ClockSource clock_source,
                 TransportState tstate, bool blink_slot, bool bpm_inverted) {
  // Clear only the top page for BPM (1 page height, full width)
  for (int i = 0; i < 128; ++i) {
    fb[0 * 128 + i] = 0x00;
  }

  // Draw Transport Icons (Top-Left)
  int icon_x = 0;
  int icon_y = 1; // Slight offset from top
  switch (tstate) {
  case TSTATE_STOP:
    fill_rect(icon_x, icon_y, 6, 6);
    break;
  case TSTATE_PLAY:
    // Right pointing triangle
    for (int i = 0; i < 4; ++i) {
      for (int j = i; j < 7 - i; ++j) {
        set_pixel(icon_x + i, icon_y + j);
      }
    }
    break;
  case TSTATE_PAUSE:
    fill_rect(icon_x, icon_y, 2, 6);
    fill_rect(icon_x + 4, icon_y, 2, 6);
    break;
  }

  // Draw Tempo/Source (next to icon)
  int text_padding = 10;
  if (clock_source == CLOCK_INTERNAL) {
    char numbuf[16];
    snprintf(numbuf, sizeof(numbuf), "%u", (unsigned)bpm);
    if (bpm_inverted) {
      ui_draw_text_inverted(text_padding, 0, numbuf);
    } else {
      ui_draw_text(text_padding, 0, numbuf);
    }
  } else {
    ui_draw_text_inverted(text_padding, 0, "SLAVE");
  }

  // Draw pattern slot on right side (P:0-9) - skip if blinking
  if (!blink_slot) {
    char slot_buf[8];
    snprintf(slot_buf, sizeof(slot_buf), "P:%d", pattern_slot);
    int slot_x = 128 - (strlen(slot_buf) * 6); // Right align
    ui_draw_text(slot_x, 0, slot_buf);
  }

  ssd1306_update();
}

// Helper: clear rectangular region (inclusive) in pixel coords
void clear_region(int x0, int y0, int w, int h) {
  if (w <= 0 || h <= 0)
    return;
  int x1 = x0 + w - 1;
  int y1 = y0 + h - 1;
  if (x0 < 0)
    x0 = 0;
  if (y0 < 0)
    y0 = 0;
  if (x1 >= 128)
    x1 = 127;
  if (y1 >= 64)
    y1 = 63;
  for (int y = y0; y <= y1; ++y) {
    int page = y >> 3;
    int bit = y & 7;
    for (int x = x0; x <= x1; ++x) {
      fb[page * 128 + x] &= ~(1u << bit);
    }
  }
}

// Helper: draw scaled text
static void draw_scaled_text(int x, int y, const char *text, int scale) {
  for (const char *p = text; *p; ++p) {
    draw_scaled_char(x, y, *p, scale);
    x += (5 * scale) + 2;
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

static void draw_rect_outline_dither(int x0, int y0, int w, int h) {
  for (int x = x0; x < x0 + w; ++x) {
    if ((x + y0) % 3 == 0)
      set_pixel(x, y0);
    if ((x + y0 + h - 1) % 2 == 0)
      set_pixel(x, y0 + h - 1);
  }
  for (int y = y0; y < y0 + h; ++y) {
    if ((x0 + y) % 3 == 0)
      set_pixel(x0, y);
    if ((x0 + w - 1 + y) % 2 == 0)
      set_pixel(x0 + w - 1, y);
  }
}

static void fill_rect_dither(int x0, int y0, int w, int h) {
  for (int y = y0; y < y0 + h; ++y) {
    for (int x = x0; x < x0 + w; ++x) {
      if ((x + y) % 3 == 0) {
        set_pixel(x, y);
      }
    }
  }
}

void ui_show_steps(uint32_t current_step, uint32_t steps) {
  if (steps == 0)
    return;

  // 4 rows of 8 steps = 32 steps
  // Each row split into two groups of 4 (XXXX XXXX)
  // Making boxes even wider and narrowing the gaps
  const int sq_w = 13;
  const int sq_h = 8;
  const int spacing_x = 2;
  const int spacing_y = 5;
  const int group_gap = 8;
  const int cols = 8;

  // Total width: 8 boxes + 6 small gaps + 1 large gap
  const int total_w =
      (8 * sq_w) + (6 * spacing_x) + group_gap; // 104 + 12 + 8 = 124
  const int left = (128 - total_w) / 2;

  // Move grid 2px further down for balance
  const int start_y = 15;

  // Clear grid area
  clear_region(0, start_y - 2, 128, 51);

  for (int i = 0; i < 32; ++i) {
    int col = i % cols;
    int row = i / cols;

    // Calculate X with group gap
    int x = left + col * (sq_w + spacing_x);
    if (col >= 4) {
      x += (group_gap - spacing_x);
    }

    int y = start_y + row * (sq_h + spacing_y);

    bool is_active = (i < (int)steps);
    bool is_current = (i == (int)current_step);
    bool gate_enabled = seq_get_gate_enabled(i);

    if (is_active) {
      if (is_current) {
        fill_rect(x, y, sq_w, sq_h);
      } else {
        draw_rect_outline(x, y, sq_w, sq_h);
      }

      if (gate_enabled) {
        if (is_current) {
          clear_region(x + 5, y + 2, 3, 4);
        } else {
          fill_rect(x + 5, y + 2, 3, 4);
        }
      }
    } else {
      // Dithered Ghost Step
      if (is_current) {
        fill_rect_dither(x, y, sq_w, sq_h);
      } else {
        draw_rect_outline_dither(x, y, sq_w, sq_h);
      }

      if (gate_enabled) {
        if (is_current) {
          clear_region(x + 5, y + 2, 3, 4);
        } else {
          fill_rect_dither(x + 5, y + 2, 3, 4);
        }
      }
    }
  }

  ssd1306_update();
}

// Helper: Convert MIDI note to name (e.g., 48 -> "C3")
static void note_to_string(uint8_t note, char *buf) {
  const char *notes[] = {"C",  "C#", "D",  "D#", "E",  "F",
                         "F#", "G",  "G#", "A",  "A#", "B"};
  int octave = (note / 12) - 1; // MIDI: C-1=0, C0=12, C1=24, C2=36, C3=48, etc.
  int semitone = note % 12;
  sprintf(buf, "%s%d", notes[semitone], octave);
}

void ui_show_edit_step(uint32_t selected_step, uint8_t note) {
  // Paging logic: Page 1 (0-15), Page 2 (16-31)
  uint32_t page = selected_step / 16;
  uint32_t start_idx = page * 16;

  const int cols = 8;
  const int sq = 13;
  const int spacing = 2;
  const int group_gap = 6;
  const int total_w = (8 * sq) + (6 * spacing) + group_gap;
  const int left = (128 - total_w) / 2;
  const int bottom_y = 64 - sq;
  const int top_y = bottom_y - sq - 8;

  auto get_step_x = [&](int col) {
    int x = left + col * (sq + spacing);
    if (col >= 4)
      x += (group_gap - spacing);
    return x;
  };

  bool first_draw = (ui_edit_step_prev_step == -1);
  // Also force redraw if page changed
  bool page_changed =
      (!first_draw && ((uint32_t)ui_edit_step_prev_step / 16 != page));

  if (first_draw || page_changed) {
    ssd1306_clear_fb();
    char header[32];
    sprintf(header, "STEP EDIT P:%d", page + 1);
    ui_draw_text(0, 0, header);

    uint32_t total_steps = seq_get_steps();

    for (int i = 0; i < 16; ++i) {
      uint32_t global_idx = start_idx + i;
      int col = i % cols;
      int row = i / cols;
      int x = get_step_x(col);
      int step_y = (row == 0) ? top_y : bottom_y;
      bool is_selected = (global_idx == selected_step);
      bool is_active = (global_idx < total_steps);
      bool gate_enabled = seq_get_gate_enabled(global_idx);

      if (is_active) {
        if (is_selected) {
          draw_rect_outline(x, step_y, sq, sq);
          if (gate_enabled) {
            fill_rect(x + 3, step_y + 3, 7, 7);
          }
        } else {
          draw_rect_outline(x + 2, step_y + 2, sq - 4, sq - 4);
          if (gate_enabled) {
            fill_rect(x + 4, step_y + 4, 5, 5);
          }
        }
      } else {
        // Ghost steps
        if (is_selected) {
          draw_rect_outline_dither(x, step_y, sq, sq);
          if (gate_enabled) {
            fill_rect_dither(x + 3, step_y + 3, 7, 7);
          }
        } else {
          draw_rect_outline_dither(x + 2, step_y + 2, sq - 4, sq - 4);
          if (gate_enabled) {
            fill_rect_dither(x + 4, step_y + 4, 5, 5);
          }
        }
      }
    }
  } else {
    clear_region(0, 16, 128, 8);

    uint32_t current_gate_mask = seq_get_gate_mask();
    uint32_t total_steps = seq_get_steps();

    if (ui_edit_step_prev_step != (int32_t)selected_step) {
      // Clear old selected step visual (on current page)
      int old_local_idx = ui_edit_step_prev_step % 16;
      uint32_t old_global_idx = (uint32_t)ui_edit_step_prev_step;
      int old_col = old_local_idx % cols;
      int old_row = old_local_idx / cols;
      int old_x = get_step_x(old_col);
      int old_y = (old_row == 0) ? top_y : bottom_y;
      clear_region(old_x, old_y, sq, sq);
      bool old_gate = seq_get_gate_enabled(old_global_idx);
      bool old_active = (old_global_idx < total_steps);

      if (old_active) {
        draw_rect_outline(old_x + 2, old_y + 2, sq - 4, sq - 4);
        if (old_gate) {
          fill_rect(old_x + 4, old_y + 4, 5, 5);
        }
      } else {
        draw_rect_outline_dither(old_x + 2, old_y + 2, sq - 4, sq - 4);
        if (old_gate) {
          fill_rect_dither(old_x + 4, old_y + 4, 5, 5);
        }
      }

      // Draw new selected step visual
      int new_local_idx = selected_step % 16;
      int new_col = new_local_idx % cols;
      int new_row = new_local_idx / cols;
      int new_x = get_step_x(new_col);
      int new_y = (new_row == 0) ? top_y : bottom_y;
      clear_region(new_x, new_y, sq, sq);
      bool new_gate = seq_get_gate_enabled(selected_step);
      bool new_active = (selected_step < total_steps);

      if (new_active) {
        draw_rect_outline(new_x, new_y, sq, sq);
        if (new_gate) {
          fill_rect(new_x + 3, new_y + 3, 7, 7);
        }
      } else {
        draw_rect_outline_dither(new_x, new_y, sq, sq);
        if (new_gate) {
          fill_rect_dither(new_x + 3, new_y + 3, 7, 7);
        }
      }
    } else if (current_gate_mask != ui_edit_step_prev_gate) {
      int local_idx = selected_step % 16;
      int col = local_idx % cols;
      int row = local_idx / cols;
      int x = get_step_x(col);
      int y = (row == 0) ? top_y : bottom_y;
      clear_region(x, y, sq, sq);
      bool gate = seq_get_gate_enabled(selected_step);
      bool active = (selected_step < total_steps);

      if (active) {
        draw_rect_outline(x, y, sq, sq);
        if (gate) {
          fill_rect(x + 3, y + 3, 7, 7);
        }
      } else {
        draw_rect_outline_dither(x, y, sq, sq);
        if (gate) {
          fill_rect_dither(x + 3, y + 3, 7, 7);
        }
      }
    }

    ui_edit_step_prev_gate = current_gate_mask;
  }

  char buf[32];
  char note_str[8];
  note_to_string(note, note_str);
  uint8_t velo_idx = seq_get_velocity(selected_step);
  sprintf(buf, "%02d %s [%s]", selected_step + 1, note_str,
          velo_names[velo_idx > 4 ? 4 : velo_idx]);
  ui_draw_text(0, 2, buf);

  if (!first_draw || page_changed) {
    ui_edit_step_prev_step = selected_step;
    ui_edit_step_prev_note = note;
  }

  ssd1306_update();
}

static uint8_t ui_edit_note_prev_mode = 255; // 0=note, 1=velo

void ui_show_edit_note(uint32_t step, uint8_t note, uint8_t velocity,
                       bool edit_velocity) {
  bool gate_on = seq_get_gate_enabled(step);
  uint8_t current_mode = edit_velocity ? 1 : 0;
  bool first_draw = (ui_edit_note_prev_note == 255);
  bool mode_changed = (ui_edit_note_prev_mode != current_mode);

  if (first_draw) {
    ssd1306_clear_fb();
  }

  // Line 1: Step number (left) and Gate status (right)
  if (first_draw || ui_edit_note_prev_step != step ||
      ui_edit_note_prev_gate != gate_on) {
    clear_region(0, 10, 128, 20);
    char step_buf[8];
    sprintf(step_buf, "%02d", step + 1);
    draw_scaled_text(0, 10, step_buf, 2);

    char gate_buf[8];
    sprintf(gate_buf, "%s", gate_on ? "ON" : "OFF");
    int gate_x = 128 - (strlen(gate_buf) * (5 * 2 + 2));
    draw_scaled_text(gate_x, 10, gate_buf, 2);
  }

  // Line 2: Note (left) and Velocity (right) - with inverted highlighting
  if (first_draw || ui_edit_note_prev_note != note ||
      ui_edit_note_prev_velocity != velocity || mode_changed) {
    clear_region(0, 36, 128,
                 22); // Increased to cover padding (14 + 2*2 + margin)

    char note_str[8];
    note_to_string(note, note_str);

    // Note on the left
    draw_scaled_text(0, 38, note_str, 2);
    if (!edit_velocity) {
      // Invert after drawing with padding
      int note_width = strlen(note_str) * 12;
      int pad = 2; // padding in pixels
      for (int cy = -pad; cy < 14 + pad; cy++) {
        for (int cx = -pad; cx < note_width + pad; cx++) {
          int px = 0 + cx;
          int py = 38 + cy;
          if (px >= 0 && px < 128 && py >= 0 && py < 64) {
            int page = py >> 3;
            int bit = py & 7;
            fb[page * 128 + px] ^= (1u << bit);
          }
        }
      }
    }

    // Velocity on the right
    const char *velo_str = velo_names[velocity > 4 ? 4 : velocity];
    int velo_x = 128 - (strlen(velo_str) * 12);
    draw_scaled_text(velo_x, 38, velo_str, 2);
    if (edit_velocity) {
      // Invert after drawing with padding
      int velo_width = strlen(velo_str) * 12;
      int pad = 2; // padding in pixels
      for (int cy = -pad; cy < 14 + pad; cy++) {
        for (int cx = -pad; cx < velo_width + pad; cx++) {
          int px = velo_x + cx;
          int py = 38 + cy;
          if (px >= 0 && px < 128 && py >= 0 && py < 64) {
            int page = py >> 3;
            int bit = py & 7;
            fb[page * 128 + px] ^= (1u << bit);
          }
        }
      }
    }
  }

  ui_edit_note_prev_note = note;
  ui_edit_note_prev_gate = gate_on;
  ui_edit_note_prev_step = step;
  ui_edit_note_prev_velocity = velocity;
  ui_edit_note_prev_mode = current_mode;

  ssd1306_update();
}

void ui_show_velocity_hud(uint8_t velocity) {
  // HUD at Page 1 (between header and info) to avoid grid/note clashes
  int x0 = 0, y0 = 10, w = 128, h = 8;
  clear_region(x0, y0, w, h);
  char buf[32];
  sprintf(buf, "VELO: %s", velo_names[velocity > 4 ? 4 : velocity]);
  // Center the text
  int tx = (128 - (strlen(buf) * 6)) / 2;
  ui_draw_text(tx, 1, buf);
  ssd1306_update();
}

void ui_show_pattern_select(uint8_t slot) {
  bool first_draw = (ui_pattern_select_prev_slot == -1);

  if (first_draw) {
    clear_region(0, 0, 128, 64);
    ui_draw_text(22, 0, "PATTERN SELECT");
    ui_draw_text(36, 7, "LOAD/SAVE");
  }

  if (first_draw || ui_pattern_select_prev_slot != (int8_t)slot) {
    clear_region(48, 16, 32, 32);
    char slot_char = '0' + slot;
    draw_scaled_char(56, 24, slot_char, 3);
  }

  ui_pattern_select_prev_slot = slot;
  ssd1306_update();
}

void ui_show_settings(int current_option, ClockSource clock_source) {
  ssd1306_clear_fb();
  ui_draw_text(36, 0, "SETTINGS");

  // For now we only have one option: CLOCK
  char buf[32];
  sprintf(buf, "%sCLOCK:", (current_option == 0) ? ">> " : "   ");
  ui_draw_text(0, 3, buf);

  const char *source_str =
      (clock_source == CLOCK_INTERNAL) ? "INTERNAL" : "EXTERNAL";
  sprintf(buf, "   %s", source_str);
  ui_draw_text(0, 5, buf);

  // Firmware Version (bottom-right)
  ui_draw_text(128 - (strlen(FIRMWARE_VERSION_STR) * 6), 7,
               FIRMWARE_VERSION_STR);

  ssd1306_update();
}
