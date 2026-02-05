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
constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;

static uint8_t SSD1306_ADDR = 0x3C;

static uint8_t fb[SCREEN_WIDTH * (SCREEN_HEIGHT / 8)];

static int32_t ui_edit_step_prev_step = -1;
static uint32_t ui_edit_step_prev_gate = 0xFFFFFFFF;
static uint32_t ui_edit_step_prev_tie_mask = 0xFFFFFFFF;
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
    {0x38, 0x44, 0x44, 0x44, 0x38},                                 // 44: o
    {0x7C, 0x14, 0x14, 0x14, 0x08},                                 // 45: p
    {0x7C, 0x04, 0x18, 0x04, 0x78},                                 // 46: m
    {0x08, 0x7E, 0x09, 0x01, 0x02},                                 // 47: f
    {0x23, 0x13, 0x08, 0x64, 0x62},                                 // 48: %
    {0x08, 0x08, 0x3E, 0x08, 0x08},                                 // 49: +
    {0x08, 0x08, 0x08, 0x08, 0x08},                                 // 50: -
    {0x38, 0x44, 0x44, 0x44, 0x20},                                 // 51: c
    {0x04, 0x3F, 0x44, 0x40, 0x00}                                  // 52: t
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
  if (c == 'p')
    return 45;
  if (c == 'm')
    return 46;
  if (c == 'f')
    return 47;
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
  if (c == '%')
    return 48;
  if (c == '+')
    return 49;
  if (c == '-')
    return 50;
  if (c == 'c')
    return 51;
  if (c == 't')
    return 52;
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

    uint8_t buf[SCREEN_WIDTH + 1];
    buf[0] = 0x40;
    memcpy(&buf[1], &fb[page * SCREEN_WIDTH], SCREEN_WIDTH);
    i2c_write_blocking(i2c0, SSD1306_ADDR, buf, SCREEN_WIDTH + 1, false);
  }
}

static void ui_draw_char(int x, int page, char c) {
  int idx = char_to_font_index(c);
  const uint8_t *glyph = font5x7[idx];
  if (x < 0 || x + 6 > SCREEN_WIDTH)
    return;
  uint8_t *dst = &fb[page * SCREEN_WIDTH + x];
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
    if (pos >= 0 && pos + 6 <= SCREEN_WIDTH) {
      uint8_t *dst = &fb[page * SCREEN_WIDTH + pos];
      for (int i = 0; i < 5; ++i)
        dst[i] = ~glyph[i];
      dst[5] = 0xFF;
    }
    pos += 6;
    str++;
  }
}

static void set_pixel(int x, int y) {
  if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
    return;
  int page = y >> 3;
  int bit = y & 7;
  fb[page * SCREEN_WIDTH + x] |= (1u << bit);
}

static void clear_pixel(int x, int y) {
  if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
    return;
  int page = y >> 3;
  int bit = y & 7;
  fb[page * SCREEN_WIDTH + x] &= ~(1u << bit);
}

static void draw_scaled_char(int x0, int y0, char c, int scale) {
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
  const char *brand = "aAOn";
  const char *sub = "CV SEQUENCER";
  char ver_buf[16];
  snprintf(ver_buf, sizeof(ver_buf), "%s", FIRMWARE_VERSION_STR);

  // Phase 1: Noise Sweep (Digital Rain style)
  for (int frame = 0; frame < 20; frame++) {
    for (int i = 0; i < 150; i++) {
      int x = rand() % SCREEN_WIDTH;
      int y = rand() % SCREEN_HEIGHT;
      set_pixel(x, y);
    }
    ssd1306_update();
    sleep_ms(15);
    if (frame % 2 == 0) {
      // Partially clear some bits for "raining" effect
      for (int i = 0; i < 100; i++) {
        clear_pixel(rand() % SCREEN_WIDTH, rand() % SCREEN_HEIGHT);
      }
    }
  }

  // Phase 2: Logo Snap Reveal
  ssd1306_clear_fb();
  int scale = 3;
  int brand_w = (5 * scale + 2) * 4;
  int brand_x = (SCREEN_WIDTH - brand_w) / 2;
  int brand_y = 6;

  for (int i = 0; i < 4; i++) {
    // Letters snap in from random heights
    int start_y = (rand() % 60) - 20;
    for (int step = 0; step < 5; step++) {
      int cur_y = start_y + (brand_y - start_y) * (step + 1) / 5;

      // Clear vertical strip for this character
      clear_region(brand_x + i * (5 * scale + 2), 0, (5 * scale + 2),
                   SCREEN_HEIGHT);
      draw_scaled_char(brand_x + i * (5 * scale + 2), cur_y, brand[i], scale);
      ssd1306_update();
      sleep_ms(10);
    }
  }

  // Phase 3: Digital Glitch (Brief horizontal shifts)
  for (int g = 0; g < 8; g++) {
    int line = rand() % 8;
    int shift = (rand() % 16) - 8;

    // Copy a page to a temp buffer, shift it, and write back
    uint8_t temp[SCREEN_WIDTH];
    memcpy(temp, &fb[line * SCREEN_WIDTH], SCREEN_WIDTH);

    for (int x = 0; x < SCREEN_WIDTH; x++) {
      int src_x = (x - shift + SCREEN_WIDTH) % SCREEN_WIDTH;
      fb[line * SCREEN_WIDTH + x] = temp[src_x];
    }

    ssd1306_update();
    sleep_ms(20);

    // Restore
    memcpy(&fb[line * 128], temp, 128);
    ssd1306_update();
  }

  // Phase 4: Info Reveal (Typewriter style)
  int sub_w = strlen(sub) * 6;
  int sub_x = (SCREEN_WIDTH - sub_w) / 2;
  int sub_y_page = 5;

  for (size_t i = 1; i <= strlen(sub); i++) {
    char temp_sub[16];
    strncpy(temp_sub, sub, i);
    temp_sub[i] = '\0';
    ui_draw_text(sub_x, sub_y_page, temp_sub);
    ssd1306_update();
    sleep_ms(30);
  }

  // Version Reveal
  int ver_w = strlen(ver_buf) * 6;
  int ver_x = (SCREEN_WIDTH - ver_w) / 2;
  ui_draw_text(ver_x, 7, ver_buf);
  ssd1306_update();

  sleep_ms(1200);

  // Exit with a vertical split
  for (int h = 0; h < 32; h += 4) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      clear_pixel(x, 32 - h);
      clear_pixel(x, 32 + h);
    }
    ssd1306_update();
    sleep_ms(15);
  }

  ssd1306_clear_fb();
  ssd1306_update();
}

void ui_clear() {
  ssd1306_clear_fb();
  ssd1306_update();

  ui_edit_step_prev_step = -1;
  ui_edit_step_prev_gate = 0xFFFF;
  ui_edit_note_prev_note = 255;
  ui_edit_note_prev_gate = false;
  ui_edit_note_prev_step = 255;
  ui_pattern_select_prev_slot = -1;
}

void ui_show_bpm(uint32_t bpm, uint8_t pattern_slot, ClockSource clock_source,
                 TransportState tstate, bool blink_slot, bool bpm_inverted,
                 uint32_t current_step, uint32_t total_steps, bool blink_icon,
                 int8_t global_octave, bool octave_inverted,
                 int8_t global_transpose, bool transpose_inverted) {
  // Clear only the top page for BPM (1 page height, full width)
  for (int i = 0; i < SCREEN_WIDTH; ++i) {
    fb[0 * SCREEN_WIDTH + i] = 0x00;
  }

  // Draw Transport Icons (Top-Left) - skip if blinking
  if (!blink_icon) {
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
  }

  // Draw Tempo/Source (next to icon)
  int text_padding = 10;
  int bpm_width = 0;
  if (clock_source == CLOCK_INTERNAL) {
    char numbuf[16];
    snprintf(numbuf, sizeof(numbuf), "%u", (unsigned)bpm);
    bpm_width = strlen(numbuf) * 6;
    if (bpm_inverted) {
      ui_draw_text_inverted(text_padding, 0, numbuf);
    } else {
      ui_draw_text(text_padding, 0, numbuf);
    }
  } else {
    ui_draw_text_inverted(text_padding, 0, "SLAVE");
    bpm_width = 5 * 6;
  }

  // Draw Octave (Fixed Position)
  ui_draw_text(40, 0, "O:");
  char oct_buf[8];
  snprintf(oct_buf, sizeof(oct_buf), "%+d", global_octave);
  if (octave_inverted) {
    ui_draw_text_inverted(52, 0, oct_buf);
  } else {
    ui_draw_text(52, 0, oct_buf);
  }

  // Draw Transpose (Fixed Position)
  ui_draw_text(72, 0, "T:");
  char tr_buf[8];
  snprintf(tr_buf, sizeof(tr_buf), "%+02d", global_transpose);
  if (transpose_inverted) {
    ui_draw_text_inverted(84, 0, tr_buf);
  } else {
    ui_draw_text(84, 0, tr_buf);
  }

  // Draw pattern slot (right aligned)
  if (!blink_slot) {
    char slot_buf[8];
    snprintf(slot_buf, sizeof(slot_buf), "P:%d", pattern_slot);
    ui_draw_text(110, 0, slot_buf);
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
  if (x1 >= SCREEN_WIDTH)
    x1 = SCREEN_WIDTH - 1;
  if (y1 >= SCREEN_HEIGHT)
    y1 = SCREEN_HEIGHT - 1;
  for (int y = y0; y <= y1; ++y) {
    int page = y >> 3;
    int bit = y & 7;
    for (int x = x0; x <= x1; ++x) {
      fb[page * SCREEN_WIDTH + x] &= ~(1u << bit);
    }
  }
}

static void invert_region(int x0, int y0, int w, int h) {
  if (w <= 0 || h <= 0)
    return;
  int x1 = x0 + w - 1;
  int y1 = y0 + h - 1;
  if (x0 < 0)
    x0 = 0;
  if (y0 < 0)
    y0 = 0;
  if (x1 >= SCREEN_WIDTH)
    x1 = SCREEN_WIDTH - 1;
  if (y1 >= SCREEN_HEIGHT)
    y1 = SCREEN_HEIGHT - 1;
  for (int y = y0; y <= y1; ++y) {
    int page = y >> 3;
    int bit = y & 7;
    for (int x = x0; x <= x1; ++x) {
      fb[page * SCREEN_WIDTH + x] ^= (1u << bit);
    }
  }
}

// Helper: draw scaled text
void draw_scaled_text(int x, int y, const char *text, int scale) {
  for (const char *p = text; *p; ++p) {
    draw_scaled_char(x, y, *p, scale);
    x += (5 * scale) + 2;
  }
}

static void draw_centered_text(int y, const char *text, int scale) {
  int char_w = (5 * scale) + 2;
  int total_w = strlen(text) * char_w;
  int x = (SCREEN_WIDTH - total_w) / 2;
  draw_scaled_text(x, y, text, scale);
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

static void draw_rect_custom(int x0, int y0, int w, int h, bool left,
                             bool right, bool top, bool bottom) {
  if (top) {
    for (int x = x0; x < x0 + w; ++x)
      set_pixel(x, y0);
  }
  if (bottom) {
    for (int x = x0; x < x0 + w; ++x)
      set_pixel(x, y0 + h - 1);
  }
  if (left) {
    for (int y = y0; y < y0 + h; ++y)
      set_pixel(x0, y);
  }
  if (right) {
    for (int y = y0; y < y0 + h; ++y)
      set_pixel(x0 + w - 1, y);
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

// Display 32-step grid (current_step in [0..steps-1]).
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
  const int left = (SCREEN_WIDTH - total_w) / 2;

  // Move grid 2px further down for balance
  const int start_y = 15;

  // Clear grid area
  clear_region(0, start_y - 2, SCREEN_WIDTH, 51);

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
    // Use 0xFFFFFFFF as special value to indicate "no pointer"
    bool is_current = (current_step != 0xFFFFFFFF) && (i == (int)current_step);
    bool gate_enabled = seq_get_gate_enabled(i);
    bool tied = seq_get_tie(i);
    bool prev_tied = (i > 0 && (i % 8 != 0) && seq_get_tie(i - 1));

    if (is_active) {
      bool draw_left = !prev_tied;
      bool draw_right = !tied || (i % 8 == 7);

      if (is_current) {
        fill_rect(x, y, sq_w, sq_h);
      } else {
        draw_rect_custom(x, y, sq_w, sq_h, draw_left, draw_right, true, true);
      }

      if (gate_enabled) {
        if (is_current) {
          clear_region(x + 5, y + 2, 3, 4);
        } else {
          fill_rect(x + 5, y + 2, 3, 4);
        }
      }

      // Draw tie visual (hollow bridge)
      if (tied && i < 31 && (i + 1) < (int)steps && (i % 8 != 7)) {
        // Calculate next X to bridge any gap correctly (including group gap)
        int next_col = (i + 1) % cols;
        int next_x = left + next_col * (sq_w + spacing_x);
        if (next_col >= 4) {
          next_x += (group_gap - spacing_x);
        }

        int bridge_x_start = x + sq_w - 1;
        int bridge_x_end = next_x;

        for (int bx = bridge_x_start + 1; bx < bridge_x_end; ++bx) {
          set_pixel(bx, y);
          set_pixel(bx, y + sq_h - 1);
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

static void ui_draw_edit_step_single(uint32_t global_idx, bool is_selected,
                                     uint32_t selected_step,
                                     uint32_t total_steps, int x, int next_x,
                                     int step_y, int sq, int spacing) {
  bool is_active = (global_idx < total_steps);
  bool gate_enabled = seq_get_gate_enabled(global_idx);
  bool tied = seq_get_tie(global_idx);
  bool prev_tied =
      (global_idx > 0 && (global_idx % 8 != 0) && seq_get_tie(global_idx - 1));
  bool next_selected = (global_idx + 1 == selected_step);

  if (is_active) {
    bool draw_left = !prev_tied;
    bool draw_right = !tied || (global_idx % 8 == 7);

    if (is_selected) {
      draw_rect_custom(x, step_y, sq, sq, draw_left, draw_right, true, true);
      if (gate_enabled) {
        fill_rect(x + 3, step_y + 3, 7, 7);
      }
    } else {
      draw_rect_custom(x + 2, step_y + 2, sq - 4, sq - 4, draw_left, draw_right,
                       true, true);
      if (gate_enabled) {
        fill_rect(x + 4, step_y + 4, 5, 5);
      }
    }

    // Bridge gaps for ties in edit view (hollow bridge/funnel)
    if (tied && global_idx < 31 && (global_idx + 1) < total_steps &&
        (global_idx % 8 != 7)) {

      // Calculate pixel-perfect start/end X based on current and next box
      // boundaries
      int bridge_x_start = is_selected ? (x + sq - 1) : (x + sq - 3);
      int bridge_x_end = next_selected ? next_x : (next_x + 2);

      // Determine bridge Y boundaries
      int current_y_top = is_selected ? step_y : step_y + 2;
      int current_y_bot = is_selected ? step_y + sq - 1 : step_y + sq - 3;
      int next_y_top = next_selected ? step_y : step_y + 2;
      int next_y_bot = next_selected ? step_y + sq - 1 : step_y + sq - 3;

      int bridge_y_top =
          (current_y_top > next_y_top) ? current_y_top : next_y_top;
      int bridge_y_bot =
          (current_y_bot < next_y_bot) ? current_y_bot : next_y_bot;

      // Draw bridge lines - ensuring they touch the boundaries
      for (int bx = bridge_x_start + 1; bx < bridge_x_end; ++bx) {
        set_pixel(bx, bridge_y_top);
        set_pixel(bx, bridge_y_bot);
      }

      // Funnel: vertical segments to close the larger box gap at the transition
      // points We draw these at the exact boundary of the boxes

      // Left side (current box is taller)
      if (current_y_top < bridge_y_top) {
        for (int vy = current_y_top; vy <= bridge_y_top; ++vy)
          set_pixel(bridge_x_start, vy);
      }
      if (current_y_bot > bridge_y_bot) {
        for (int vy = bridge_y_bot; vy <= current_y_bot; ++vy)
          set_pixel(bridge_x_start, vy);
      }

      // Right side (next box is taller)
      if (next_y_top < bridge_y_top) {
        for (int vy = next_y_top; vy <= bridge_y_top; ++vy)
          set_pixel(bridge_x_end, vy);
      }
      if (next_y_bot > bridge_y_bot) {
        for (int vy = bridge_y_bot; vy <= next_y_bot; ++vy)
          set_pixel(bridge_x_end, vy);
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

void ui_show_edit_step(uint32_t selected_step, uint8_t note) {
  // Paging logic: Page 1 (0-15), Page 2 (16-31)
  uint32_t page = selected_step / 16;
  uint32_t start_idx = page * 16;

  const int cols = 8;
  const int sq = 13;
  const int spacing = 2;
  const int group_gap = 6;
  const int total_w = (8 * sq) + (6 * spacing) + group_gap;
  const int left = (SCREEN_WIDTH - total_w) / 2;
  const int bottom_y = SCREEN_HEIGHT - sq;
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
    sprintf(header, "PATTERN EDIT PAGE:%d", page + 1);
    ui_draw_text(0, 0, header);

    uint32_t total_steps = seq_get_steps();

    for (int i = 0; i < 16; ++i) {
      uint32_t global_idx = start_idx + i;
      int col = i % cols;
      int row = i / cols;
      int x = get_step_x(col);
      int step_y = (row == 0) ? top_y : bottom_y;
      int next_x = get_step_x(col + 1);
      ui_draw_edit_step_single(global_idx, global_idx == selected_step,
                               selected_step, total_steps, x, next_x, step_y,
                               sq, spacing);
    }
  } else {
    clear_region(0, 16, 128, 8);

    uint32_t current_gate_mask = seq_get_gate_mask();
    uint32_t total_steps = seq_get_steps();

    if (ui_edit_step_prev_step != (int32_t)selected_step) {
      // 1. Redraw old selected step as non-selected
      int old_local_idx = ui_edit_step_prev_step % 16;
      uint32_t old_global_idx = (uint32_t)ui_edit_step_prev_step;
      int old_col = old_local_idx % cols;
      int old_row = old_local_idx / cols;
      int old_x = get_step_x(old_col);
      int old_y = (old_row == 0) ? top_y : bottom_y;
      clear_region(old_x, old_y, sq, sq);
      int old_next_x = get_step_x(old_local_idx + 1);
      ui_draw_edit_step_single(old_global_idx, false, selected_step,
                               total_steps, old_x, old_next_x, old_y, sq,
                               spacing);

      // If old was tied or prev was tied, we might need to redraw neighbors
      if (old_local_idx > 0 && seq_get_tie(old_global_idx - 1)) {
        int prev_x = get_step_x(old_local_idx - 1);
        ui_draw_edit_step_single(old_global_idx - 1, false, selected_step,
                                 total_steps, prev_x, old_x, old_y, sq,
                                 spacing);
      }

      // 2. Redraw new selected step
      int new_local_idx = selected_step % 16;
      int new_col = new_local_idx % cols;
      int new_row = new_local_idx / cols;
      int new_x = get_step_x(new_col);
      int new_y = (new_row == 0) ? top_y : bottom_y;
      clear_region(new_x, new_y, sq, sq);
      int new_next_x = get_step_x(new_local_idx + 1);
      ui_draw_edit_step_single(selected_step, true, selected_step, total_steps,
                               new_x, new_next_x, new_y, sq, spacing);

      // If new is tied or prev is tied, redraw neighbors
      if (new_local_idx > 0 && seq_get_tie(selected_step - 1)) {
        int prev_x = get_step_x(new_local_idx - 1);
        ui_draw_edit_step_single(selected_step - 1, false, selected_step,
                                 total_steps, prev_x, new_x, new_y, sq,
                                 spacing);
      }
    } else if (current_gate_mask != ui_edit_step_prev_gate ||
               seq_get_tie_mask() != ui_edit_step_prev_tie_mask) {
      // Redraw selected step and its possible tie neighbors
      int local_idx = selected_step % 16;
      int col = local_idx % cols;
      int row = local_idx / cols;
      int x = get_step_x(col);
      int y = (row == 0) ? top_y : bottom_y;
      clear_region(x, y, sq, sq);
      int next_x = get_step_x(local_idx + 1);
      ui_draw_edit_step_single(selected_step, true, selected_step, total_steps,
                               x, next_x, y, sq, spacing);

      if (local_idx > 0 && seq_get_tie(selected_step - 1)) {
        int prev_x = get_step_x(local_idx - 1);
        ui_draw_edit_step_single(selected_step - 1, false, selected_step,
                                 total_steps, prev_x, x, y, sq, spacing);
      }
    }

    ui_edit_step_prev_gate = current_gate_mask;
    ui_edit_step_prev_tie_mask = seq_get_tie_mask();
  }

  char buf[32];
  char note_str[8];
  note_to_string(note, note_str);
  uint8_t velo_idx = seq_get_velocity(selected_step);
  bool tie = seq_get_tie(selected_step);
  const char *v_str = velo_names[velo_idx > 4 ? 4 : velo_idx];
  sprintf(buf, "%02d %s %s %s", selected_step + 1, note_str, v_str,
          tie ? "TIE" : "");
  ui_draw_text(0, 2, buf);

  if (!first_draw || page_changed) {
    ui_edit_step_prev_step = selected_step;
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

void ui_show_pattern_select(uint8_t slot) {
  bool first_draw = (ui_pattern_select_prev_slot == -1);

  if (first_draw) {
    clear_region(0, 0, 128, 64);
    ui_draw_text(22, 0, "PATTERN SELECT");
    ui_draw_text(36, 7, "LOAD/SAVE");
  }

  if (first_draw || ui_pattern_select_prev_slot != (int8_t)slot) {
    clear_region(40, 16, 48, 32);
    char slot_str[4];
    sprintf(slot_str, "%02d", slot);
    draw_scaled_text(40, 24, slot_str, 3);
  }

  ui_pattern_select_prev_slot = slot;
  ssd1306_update();
}

void ui_show_settings(int current_option, ClockSource clock_source,
                      uint8_t gate_length, uint32_t ppqn,
                      PatternLoadMode load_mode, bool edit_mode) {
  ssd1306_clear_fb();
  ui_draw_text(36, 0, "SETTINGS");

  const int NUM_ITEMS = 4;
  const int VISIBLE_ITEMS = 3;
  static int top_item = 0;

  // Adjust scroll window
  if (current_option < top_item) {
    top_item = current_option;
  } else if (current_option >= top_item + VISIBLE_ITEMS) {
    top_item = current_option - VISIBLE_ITEMS + 1;
  }

  char buf[32];
  for (int i = 0; i < VISIBLE_ITEMS; i++) {
    int item_idx = top_item + i;
    if (item_idx >= NUM_ITEMS)
      break;

    int y_row = 2 + (i * 2); // Row index (0, 2, 4, 6 etc)
    int y_px = y_row * 8;

    const char *label = "";
    char val_buf[16] = "";

    switch (item_idx) {
    case 0:
      label = "CLOCK:";
      sprintf(val_buf, "%s",
              (clock_source == CLOCK_INTERNAL) ? "INTERNAL" : "EXTERNAL");
      break;
    case 1:
      label = "GATE LEN:";
      sprintf(val_buf, "%d%%", gate_length);
      break;
    case 2:
      label = "PPQN:";
      sprintf(val_buf, "%d", ppqn);
      break;
    case 3:
      label = "LOAD:";
      sprintf(val_buf, "%s", (load_mode == LOAD_INSTANT) ? "INST" : "WAIT");
      break;
    }

    // Draw row
    sprintf(buf, "%s%s ", (current_option == item_idx) ? ">> " : "   ", label);
    ui_draw_text(0, y_row, buf);
    int val_x = strlen(buf) * 6;
    ui_draw_text(val_x, y_row, val_buf);

    // Highlight if editing
    if (current_option == item_idx && edit_mode) {
      invert_region(val_x - 1, y_px - 1, strlen(val_buf) * 6 + 1, 9);
    }
  }

  // Firmware Version (bottom-right)
  ui_draw_text(128 - (strlen(FIRMWARE_VERSION_STR) * 6), 7,
               FIRMWARE_VERSION_STR);

  ssd1306_update();
}

static void draw_page_indicator(int count, int selected) {
  int dot_w = 4;
  int spacing = 6;
  int total_w = (count * dot_w) + ((count - 1) * spacing);
  int x0 = (128 - total_w) / 2;
  int y = 60;
  for (int i = 0; i < count; i++) {
    int x = x0 + i * (dot_w + spacing);
    if (i == selected) {
      fill_rect(x, y, dot_w, dot_w);
    } else {
      draw_rect_outline(x, y, dot_w, dot_w);
    }
  }
}

void ui_show_pattern_tools(int current_option, bool edit_mode,
                           uint8_t density) {
  ssd1306_clear_fb();

  // Page Indicators at the bottom - 3 cards now
  draw_page_indicator(3, current_option);

  if (current_option == 0) { // SCALE CARD
    draw_centered_text(2, "GLOBAL SCALE", 1);
    const char *scale_name = seq_get_scale_name(seq_get_global_scale());

    draw_rect_outline(5, 20, 118, 24);
    int font_scale = 1;
    int char_w = (5 * font_scale) + 2;
    int total_w = strlen(scale_name) * char_w;
    int tx = (128 - total_w) / 2;
    int ty = 28;

    if (edit_mode) {
      draw_scaled_text(tx, ty, scale_name, font_scale);
      invert_region(7, 22, 114, 20);
    } else {
      draw_scaled_text(tx, ty, scale_name, font_scale);
    }

  } else if (current_option == 1) { // RANDOM GATES CARD
    draw_scaled_text(0, 2, "CHAOS GATES", 1);
    uint32_t mask = seq_get_gate_mask();

    char den_buf[8];
    sprintf(den_buf, "%%%d", density);
    int den_tx = 128 - (strlen(den_buf) * 7);
    draw_scaled_text(den_tx, 2, den_buf, 1);
    if (edit_mode)
      invert_region(den_tx - 1, 1, (strlen(den_buf) * 7) + 1, 9);

    // Larger boxes (9x6) and centered grid
    int gx = 20, gy = 20;
    for (int i = 0; i < 32; i++) {
      int row = i / 8;
      int col = i % 8;
      if (mask & (1u << i)) {
        fill_rect(gx + col * 11, gy + row * 8, 9, 6);
      } else {
        draw_rect_outline(gx + col * 11, gy + row * 8, 9, 6);
      }
    }

  } else if (current_option == 2) { // CLEAR CARD
    draw_centered_text(2, "RESET PATTERN", 1);
    draw_rect_outline(54, 15, 20, 20);
    draw_scaled_text(60, 18, "!", 2);

    if (edit_mode) {
      draw_centered_text(45, "CONFIRM CLEAR?", 1);
      invert_region(0, 44, 128, 10);
    } else {
      draw_centered_text(45, "DANGER ZONE", 1);
    }
  }

  ssd1306_update();
}

void ui_show_message(const char *msg) {
  ssd1306_clear_fb();
  // Draw message somewhat centered vertically
  // Using scale 2 for visibility
  int scale = 2;
  int char_w = (5 * scale) + 2;
  int total_w = strlen(msg) * char_w;
  int x = (SCREEN_WIDTH - total_w) / 2;
  int y = (SCREEN_HEIGHT - (7 * scale)) / 2;

  draw_scaled_text(x, y, msg, scale);
  ssd1306_update();
}
