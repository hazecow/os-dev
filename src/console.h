#pragma once

#include <stdint.h>
#include <stdarg.h>

#define KPRINT_FG 0xFFFFFF
#define KPRINT_BG 0x000000

void console_init(void);
void console_putchar(uint32_t c, uint32_t fg, uint32_t bg);
void console_puts(const char *str, uint32_t fg, uint32_t bg);
void vsprintf(char *buf, const char *fmt, va_list args);
void kprint(const char *fmt, ...);