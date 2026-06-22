#include "console.h"
#include "font.h"
#include "frame_buffer.h"

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;
static void newline(void);

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

static void newline(void) {
    cursor_x = 0;
    cursor_y++;
}