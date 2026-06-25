#pragma once

#include <stdint.h>

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

// isr.asm からエクスポートされるスタブ
extern void isr_stub_0(void);
extern void isr_stub_6(void);
extern void isr_stub_8(void);
extern void isr_stub_13(void);
extern void isr_stub_14(void);

// isr.c で定義する C ハンドラ（isr.asm の isr_common から呼ばれる）
void isr_handler(interrupt_frame_t *frame);