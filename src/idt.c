#include "idt.h"
#include "isr.h"

static idt_entry_t idt[IDT_ENTRIES];
static idtr_t      idtr;

void idt_set_gate(uint8_t vector, uintptr_t isr_addr, uint16_t kernel_cs_sel, uint8_t type_attr) {
    idt_entry_t *entry = &idt[vector];

    entry->offset_low    = (uint16_t)(isr_addr & 0xffff);
    entry->offset_mid    = (uint16_t)((isr_addr >> 16) & 0xffff);
    entry->offset_high   = (uint32_t)((isr_addr >> 32) & 0xffffffff);
    entry->ist           = 0;
    entry->kernel_cs_sel = kernel_cs_sel;
    entry->type_attr     = type_attr;
    entry->reserved      = 0;
}

void idt_init(void) {
    // 各ベクタにスタブを登録
    idt_set_gate(0,  (uintptr_t)isr_stub_0,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(6,  (uintptr_t)isr_stub_6,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(8,  (uintptr_t)isr_stub_8,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(13, (uintptr_t)isr_stub_13, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(14, (uintptr_t)isr_stub_14, 0x08, IDT_TYPE_INTERRUPT_GATE);

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base  = (uintptr_t)idt;

    // GCC Extended Inline Assembly
    // 副作用（CPU の IDTR レジスタの変更）がCの型システムからは見えないので
    // volatile をつけて不要な最適化を防止する
    __asm__ volatile (
        "lidt %0"   // %0 は最初のオペランドのプレースホルダ
        :           // 出力なし
        : "m"(idtr) // 入力: idtr をメモリオペランドとして渡す
    );
}