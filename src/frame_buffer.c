#include "frame_buffer.h"
#include "panic.h"

static void *g_fb_address;
static uint32_t g_fb_width;
static uint32_t g_fb_height;
static uint32_t g_fb_pitch;
static void *g_srcbuf_address;

void fb_init(void *fb_addr, uint32_t width, uint32_t height, uint32_t pitch) {
  g_fb_address = fb_addr;
  g_fb_width = width;
  g_fb_height = height;
  g_fb_pitch = pitch;
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
  if (x >= g_fb_width || y >= g_fb_height) {
    return;
  }

  volatile uint32_t *fb = g_fb_address;
  fb[y * (g_fb_pitch / 4) + x] = color;
}

uint32_t fb_get_width(void) { return g_fb_width; }

uint32_t fb_get_height(void) { return g_fb_height; }

void fb_draw_rect(rect_t rect, uint32_t color) {
  for (uint32_t x = rect.p.x; x < rect.p.x + rect.width; x++) {
    for (uint32_t y = rect.p.y; y < rect.p.y + rect.height; y++) {
      fb_put_pixel(x, y, color);
    }
  }
}

static uint32_t min(uint32_t x, uint32_t y) {
  if (x < y) {
    return x;
  }
  return y;
}

void fb_copy_rect(rect_t src, point_t dst) {
  if (src.p.x < 0 || src.p.x >= g_fb_width || src.p.y < 0 ||
      src.p.y >= g_fb_height) {
    panic("fb_copy_rect: src is located outside of the frame buffer");
  }

  // src を srcbuf にコピーする
  volatile uint32_t *srcbuf = g_srcbuf_address;
  volatile uint32_t *fb = g_fb_address;
  uint32_t src_w = min(src.p.x + src.width, g_fb_width) - src.p.x;
  uint32_t src_h = min(src.p.y + src.height, g_fb_height) - src.p.y;
  for (uint32_t xx = 0; xx < src_w; xx++) {
    for (uint32_t yy = 0; yy < src_h; yy++) {
      srcbuf[yy * (g_fb_pitch / 4) + xx] =
          fb[(src.p.y + yy) * (g_fb_pitch / 4) + (src.p.x + xx)];
    }
  }

  // srcbuf を dst にコピーする
  for (uint32_t xx = 0; xx < src_w; xx++) {
    for (uint32_t yy = 0; yy < src_h; yy++) {
      srcbuf[yy * (g_fb_pitch / 4) + xx] =
          fb[(src.p.y + yy) * (g_fb_pitch / 4) + (src.p.x + xx)];
      fb[(dst.y + yy) * (g_fb_pitch / 4) + (dst.x + xx)] =
          srcbuf[yy * (g_fb_pitch / 4) + xx];
    }
  }
}

void srcbuf_init(void *srcbuf_addr) { g_srcbuf_address = srcbuf_addr; }
