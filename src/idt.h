#pragma once

#include <stdint.h>

// IDT エントリ (16バイト)
typedef struct {
    uint16_t offset_low;    // ISR アドレス [15:0]
    uint16_t kernel_cs_sel; // コードセグメントセレクタ
    uint8_t  ist;           // IST（今は0固定）
    uint8_t  type_attr;     // Type + DPL + P フラグ
    uint16_t offset_mid;    // ISR アドレス [31:16]
    uint32_t offset_high;   // ISR アドレス [63:32]
    uint32_t reserved;      // 予約済み（0固定）
} __attribute__((packed)) idt_entry_t;

// IDTR レジスタのレイアウト
typedef struct {
    uint16_t limit;     // IDT のサイズ - 1
    uintptr_t base;     // IDT の先頭アドレス
} __attribute__((packed)) idtr_t;

#define IDT_ENTRIES 256

// type_attr フィールドの定数
// P=1, DPL=0, zero=0, Type=0xE (Interrupt Gate)
#define IDT_TYPE_INTERRUPT_GATE 0x8e

void idt_set_gate(uint8_t vector, uintptr_t isr_addr, uint16_t kernel_cs_sel, uint8_t type_attr);
void idt_init(void);