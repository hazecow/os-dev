#pragma once

#include <stdint.h>

#define IDT_ENTRIES 256

// type_attr フィールドの定数
// P=1, DPL=0, zero=0, Type=0xE (Interrupt Gate)
#define IDT_TYPE_INTERRUPT_GATE 0x8e

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

// isr_handler に入った時点のスタックレイアウトを反映
// フィールドの順序は isr.asm の push 順と逆になる
typedef struct {
    // isr_common で積んだ汎用レジスタ
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9,  r8;
    uint64_t rbp, rdi, rsi, rdx;
    uint64_t rcx, rbx, rax;

    // isr_stub_* で積んだ値
    uint64_t vector;
    uint64_t error_code;

    // CPU が自動で積んだ値
    uint64_t rip, cs, rflags;
} __attribute__((packed)) interrupt_frame_t;

// 割り込みハンドラの関数ポインタ型
typedef void (*interrupt_handler_t)(interrupt_frame_t *);

// IDT の初期化を行う
void interrupt_init(void);

// 割り込みハンドラを登録する
void interrupt_register(uint8_t vector, uintptr_t stub_addr, interrupt_handler_t handler);