#include "font.h"

extern char _binary_font_psf_start;

static PSF2_font *font;

void font_init(void) {
    font = (PSF2_font *)&_binary_font_psf_start;
}

unsigned char *font_get_glyph(uint32_t c) {
    return (unsigned char *)font +
        font->headersize +
        (c >= font->numglyph ? 0 : c) * font->bytesperglyph;
}

uint32_t font_get_width(void) {
    return font->width;
}

uint32_t font_get_height(void) {
    return font->height;
}