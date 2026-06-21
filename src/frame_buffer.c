#include "frame_buffer.h"

static void *fb_address;
static uint32_t fb_width;
static uint32_t fb_height;
static uint32_t fb_pitch;

void fb_init(void *address, uint32_t width, uint32_t height, uint32_t pitch) {
    fb_address = address;
    fb_width = width;
    fb_height = height;
    fb_pitch = pitch;
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb_width || y >= fb_height) {
        return;
    }

    volatile uint32_t *fb = fb_address;
    fb[y * (fb_pitch / 4) + x] = color;
}