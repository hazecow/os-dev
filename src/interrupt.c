#include "interrupt.h"
#include "console.h"

#include <stddef.h>

#define KERNEL_CODE_SEL 0x08

static const char *exception_names[] = {
    [0]  = "#DE Division Error",
    [6]  = "#UD Invalid Opcode",
    [8]  = "#DF Double Fault",
    [13] = "#GP General Protection Fault",
    [14] = "#PF Page Fault",
};

static idt_entry_t idt[IDT_ENTRIES];
static idtr_t      idtr;
static interrupt_handler_t g_isr_table[IDT_ENTRIES];

// isr.asm からエクスポートされるスタブ
extern void isr_stub_0(void);
extern void isr_stub_6(void);
extern void isr_stub_8(void);
extern void isr_stub_13(void);
extern void isr_stub_14(void);

static void idt_set_gate(uint8_t vector, uintptr_t isr_addr, uint16_t kernel_cs_sel, uint8_t type_attr) {
    idt_entry_t *entry = &idt[vector];

    entry->offset_low    = (uint16_t)(isr_addr & 0xffff);
    entry->offset_mid    = (uint16_t)((isr_addr >> 16) & 0xffff);
    entry->offset_high   = (uint32_t)((isr_addr >> 32) & 0xffffffff);
    entry->ist           = 0;
    entry->kernel_cs_sel = kernel_cs_sel;
    entry->type_attr     = type_attr;
    entry->reserved      = 0;
}

static void set_interrupt_handler(uint8_t vector, interrupt_handler_t handler) {
    g_isr_table[vector] = handler;
}

static void exception_handler(interrupt_frame_t *frame) {
    const char *name = NULL;
    if (frame->vector < sizeof(exception_names) / sizeof(exception_names[0])) {
        name = exception_names[frame->vector];
    }
    kprint("\n=== EXCEPTION ===\n");
    kprint("Vector : %d (%s)\n", frame->vector, name ? name : "Unknown");
    kprint("Error  : 0x%x\n", frame->error_code);
    kprint("RIP    : %p\n", frame->rip);
    kprint("CS     : 0x%x\n", frame->cs);
    kprint("RFLAGS : 0x%x\n", frame->rflags);

    // 回復不能なので割り込みを無効化した上で CPU を停止
    __asm__ volatile ("cli; hlt");
}

void interrupt_init(void) {
    // 各ベクタにスタブを登録
    interrupt_register(0,  (uintptr_t)isr_stub_0,  exception_handler);
    interrupt_register(6,  (uintptr_t)isr_stub_6,  exception_handler);
    interrupt_register(8,  (uintptr_t)isr_stub_8,  exception_handler);
    interrupt_register(13, (uintptr_t)isr_stub_13, exception_handler);
    interrupt_register(14, (uintptr_t)isr_stub_14, exception_handler);

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base  = (uintptr_t)idt;

    // GCC Extended Inline Assembly
    // 副作用（CPU の IDTR レジスタの変更）がCの型システムからは見えないので
    // volatile をつけて不要な最適化を防止する
    __asm__ volatile (
        "lidt %[idtr]"      // %[idtr] は最初のオペランドのプレースホルダ
        :                   // 出力なし
        : [idtr] "m"(idtr)  // 入力: idtr をメモリオペランドとして渡す
    );
}

void interrupt_register(uint8_t vector, uintptr_t stub_addr, interrupt_handler_t handler) {
    idt_set_gate(vector, stub_addr, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT_GATE);
    set_interrupt_handler(vector, handler);
}

void interrupt_handler(interrupt_frame_t *frame) {
    if (g_isr_table[(uint8_t)frame->vector]) {
        g_isr_table[(uint8_t)frame->vector](frame);
    } else {
        // fallback
        exception_handler(frame);
    }
}