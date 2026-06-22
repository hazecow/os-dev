#include <stdarg.h>

#include "console.h"
#include "font.h"
#include "frame_buffer.h"

#define KPRINT_FG 0xFFFFFF
#define KPRINT_BG 0x000000

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;
static void newline(void);
static void itoa(char *buf, uint64_t n, uint32_t base);
static void vsprintf(char *buf, const char *fmt, va_list args);

void console_init(void) {
    cursor_x = 0;
    cursor_y = 0;
}

void console_putchar(uint32_t c, uint32_t fg, uint32_t bg) {
    uint32_t glyph_width = font_get_width();
    uint32_t glyph_height = font_get_height();
    
    uint32_t cursor_cols = fb_get_width() / glyph_width;
    uint32_t cursor_rows = fb_get_height() / glyph_height;

    if (c == '\n') {
        newline();
    } else {
        // 仮定：フォントの幅は32以下
        unsigned char *glyph = font_get_glyph(c);
        uint32_t bytesperline = (glyph_width + 7) / 8;

        for (uint32_t y = 0; y < glyph_height; y++) {
            uint32_t glyph_word = 0;
            for (uint32_t b = 0; b < bytesperline; b++) {
                glyph_word = (glyph_word << 8) | glyph[b];
            }
            uint32_t mask = 1 << (glyph_width - 1);
            for (uint32_t x = 0; x < glyph_width; x++) {
                uint32_t color = glyph_word & mask ? fg : bg;
                fb_put_pixel(cursor_x * glyph_width + x, cursor_y * glyph_height + y, color);
                mask >>= 1;
            }
            glyph += bytesperline;
        }

        cursor_x++;
        if (cursor_x >= cursor_cols) {
            newline();
        }
    }

    if (cursor_y >= cursor_rows) {
        cursor_y = 0;
    }
}

void console_puts(const char *str, uint32_t fg, uint32_t bg) {
    while (*str) {
        console_putchar(*str, fg, bg);
        str++;
    }
}

static void vsprintf(char *buf, const char *fmt, va_list args) {
    char tmp[64];
    char *p = buf;

    while (*fmt) {
        if (*fmt != '%') {
            *p++ = *fmt++;
            continue;
        }

        fmt++; // '%' を読み飛ばす
        switch (*fmt) {
            case 's': {
                const char *s = va_arg(args, const char *);
                while (*s) {
                    *p++ = *s++;
                }
                break;
            }
            case 'd': {
                // n < 0 であることもあるため int (32bit) で受ける
                // フォーマット指定子が対応する型と va_arg で読み取る型は一致させなければならない
                int n = va_arg(args, int);
                if (n < 0) {
                    *p++ = '-';
                    itoa(tmp, (uint64_t)(-(int64_t)n), 10);
                } else {
                    itoa(tmp, (uint64_t)n, 10);
                }
                for (char *t = tmp; *t; t++) {
                    *p++ = *t;
                }
                break;
            }
            case 'x': {
                unsigned int n = va_arg(args, unsigned int);
                itoa(tmp, n, 16);
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
}

static void newline(void) {
    cursor_x = 0;
    cursor_y++;
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