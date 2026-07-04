#include <stdarg.h>

#include "panic.h"
#include "console.h"
#include "serial.h"


void panic(const char *fmt, ...) {
    kprint("[PANIC] ");
    static char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    console_puts(buf, KPRINT_FG, KPRINT_BG);
    serial_write_string(buf);
    console_puts("\n", KPRINT_FG, KPRINT_BG);
    serial_write_string("\n");
    for (;;) {
        asm ("hlt");
    }
}