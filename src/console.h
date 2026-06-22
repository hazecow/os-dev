#pragma once

#include <stdint.h>

void console_init(void);
void console_putchar(uint32_t c, uint32_t fg, uint32_t bg);