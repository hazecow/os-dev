#pragma once

#include <stdint.h>

void fb_init(void *address, uint32_t width, uint32_t height, uint32_t pitch);
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t fb_get_width(void);
uint32_t fb_get_height(void);