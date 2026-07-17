#pragma once

#include <stdint.h>

typedef struct {
  uint32_t x, y;
} point_t;

typedef struct {
  point_t p;
  uint32_t width, height;
} rect_t;

void fb_init(void *fb_addr, uint32_t width, uint32_t height, uint32_t pitch);
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t fb_get_width(void);
uint32_t fb_get_height(void);
void fb_draw_rect(rect_t rect, uint32_t color);
void fb_copy_rect(rect_t src, point_t dst);
void srcbuf_init(void *srcbuf_addr);
