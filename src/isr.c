#include "isr.h"
#include "console.h"

static const char *exception_names[] = {
    [0]  = "#DE Division Error",
    [6]  = "#UD Invalid Opcode",
    [8]  = "#DF Double Fault",
    [13] = "#GP General Protection Fault",
    [14] = "#PF Page Fault",
};

void isr_handler(interrupt_frame_t *frame) {
    const char *name = exception_names[frame->vector];

    kprint("\n=== EXCEPTION ===\n");
    kprint("Vector : %d (%s)\n", frame->vector, name ? name : "Unknown");
    kprint("Error  : 0x%x\n", frame->error_code);
    kprint("RIP    : %p\n", frame->rip);
    kprint("CS     : 0x%x\n", frame->cs);
    kprint("RFLAGS : 0x%x\n", frame->rflags);

    // 回復不能なので割り込みを無効化した上で CPU を停止
    __asm__ volatile ("cli; hlt");
}