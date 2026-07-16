#include <stdbool.h>
#include <stdint.h>

#include "console.h"
#include "font.h"
#include "frame_buffer.h"
#include "serial.h"

static uint32_t g_cursor_x;
static uint32_t g_cursor_y;
static uint32_t g_glyph_width;
static uint32_t g_glyph_height;
static bool g_cursor_visible;

void console_init(void) {
  g_cursor_x = 0;
  g_cursor_y = 0;
  g_cursor_visible = false;

  // g_cursor_width, g_cursor_height はフォントの width, height に合わせる
  g_glyph_width = font_get_width();
  g_glyph_height = font_get_height();
}

static void newline(void) {
  g_cursor_x = 0;
  g_cursor_y++;
}

static void itoa(char *buf, uint64_t n, uint32_t base) {
  static const char digits[] = "0123456789abcdef";
  char tmp[64];
  uint32_t i = 0;

  if (n == 0) {
    buf[0] = '0';
    buf[1] = '\0';
    return;
  }

  // base でべき乗展開したときの係数を下位桁から求める
  while (n > 0) {
    tmp[i++] = digits[n % base];
    n /= base;
  }

  // 逆順に並べる
  for (uint32_t j = 0; j < i; j++) {
    buf[j] = tmp[i - j - 1];
  }
  buf[i] = '\0';
}

void console_putchar(uint32_t c, uint32_t fg, uint32_t bg) {
  uint32_t cursor_cols = fb_get_width() / g_glyph_width;
  uint32_t cursor_rows = fb_get_height() / g_glyph_height;

  if (c == '\n') {
    newline();
  } else {
    // 仮定：フォントの幅は32以下
    unsigned char *glyph = font_get_glyph(c);
    uint32_t bytesperline = (g_glyph_width + 7) / 8;

    for (uint32_t y = 0; y < g_glyph_height; y++) {
      uint32_t glyph_word = 0;
      for (uint32_t b = 0; b < bytesperline; b++) {
        glyph_word = (glyph_word << 8) | glyph[b];
      }
      uint32_t mask = 1 << (g_glyph_width - 1);
      for (uint32_t x = 0; x < g_glyph_width; x++) {
        uint32_t color = glyph_word & mask ? fg : bg;
        fb_put_pixel(g_cursor_x * g_glyph_width + x,
                     g_cursor_y * g_glyph_height + y, color);
        mask >>= 1;
      }
      glyph += bytesperline;
    }

    g_cursor_x++;
    if (g_cursor_x >= cursor_cols) {
      newline();
    }
  }

  if (g_cursor_y >= cursor_rows) {
    g_cursor_y = 0;
  }
}

void console_puts(const char *str, uint32_t fg, uint32_t bg) {
  while (*str) {
    console_putchar(*str, fg, bg);
    str++;
  }
}

void vsprintf(char *buf, const char *fmt, va_list args) {
  char tmp[64];
  char *p = buf;

  while (*fmt) {
    if (*fmt != '%') {
      *p++ = *fmt++;
      continue;
    }

    fmt++; // '%' を読み飛ばす

    // 'l' がついていたら long フラグを立てる
    bool is_long = false;
    if (*fmt == 'l') {
      is_long = true;
      fmt++;
    }

    switch (*fmt) {
    case 's': {
      const char *s = va_arg(args, const char *);
      while (*s) {
        *p++ = *s++;
      }
      break;
    }
    case 'd': {
      // n < 0 であることもあるため 符号付き整数 で受ける
      // フォーマット指定子が対応する型と va_arg
      // で読み取る型は一致させなければならない
      if (is_long) {
        long n = va_arg(args, long);
        if (n < 0) {
          *p++ = '-';
          itoa(tmp, (uint64_t)(-(int64_t)n), 10);
        } else {
          itoa(tmp, (uint64_t)n, 10);
        }
      } else {
        int n = va_arg(args, int);
        if (n < 0) {
          *p++ = '-';
          itoa(tmp, (uint64_t)(-(int64_t)n), 10);
        } else {
          itoa(tmp, (uint64_t)n, 10);
        }
      }
      for (char *t = tmp; *t; t++) {
        *p++ = *t;
      }
      break;
    }
    case 'x': {
      if (is_long) {
        unsigned long n = va_arg(args, unsigned long);
        itoa(tmp, n, 16);
      } else {
        unsigned int n = va_arg(args, unsigned int);
        itoa(tmp, n, 16);
      }
      for (char *t = tmp; *t; t++) {
        *p++ = *t;
      }
      break;
    }
    case 'c': {
      // va_arg に char を指定すると int に拡張される
      // int で受けてから必要に応じて char にキャストする
      *p++ = (char)va_arg(args, int);
      break;
    }
    case 'p': {
      uint64_t n = (uint64_t)va_arg(args, void *);
      *p++ = '0';
      *p++ = 'x';
      itoa(tmp, n, 16);
      for (char *t = tmp; *t; t++) {
        *p++ = *t;
      }
      break;
    }
    case '%': {
      *p++ = '%';
      break;
    }
    }
    fmt++;
  }
  *p = '\0';
}

void kprint(const char *fmt, ...) {
  static char buf[1024];
  va_list args;
  va_start(args, fmt);
  vsprintf(buf, fmt, args);
  va_end(args);
  console_puts(buf, KPRINT_FG, KPRINT_BG);
  serial_write_string(buf);
}

bool is_g_cursor_visible() { return g_cursor_visible; }

void blink_cursor() {
  g_cursor_visible = !g_cursor_visible;
  rect_t cursor_rect = {.x = g_cursor_x * g_glyph_width,
                        .y = g_cursor_y * g_glyph_height,
                        .width = g_glyph_width,
                        .height = g_glyph_height};
  if (g_cursor_visible) {
    fb_draw_rect(cursor_rect, KPRINT_FG);
  } else {
    fb_draw_rect(cursor_rect, KPRINT_BG);
  }
}

void hide_cursor() {
  if (g_cursor_visible) {
    blink_cursor();
  }
}
