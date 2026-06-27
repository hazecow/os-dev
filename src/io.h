#pragma once

#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t c;
    __asm__ volatile (
        "in %[val], %[port]" 
        : [val] "=a"(c) 
        : [port] "d"(port)
    );
    return c;
}

static inline void outb(uint16_t port, uint8_t c) {
    __asm__ volatile (
        "out %[port], %[val]" 
        : 
        : [port] "d"(port), [val] "a"(c)
    );
}