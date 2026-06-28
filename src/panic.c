#include "panic.h"
#include "console.h"

void panic(const char *msg) {
    kprint("[PANIC] %s\n", msg);
    for (;;) {
        asm ("hlt");
    }
}