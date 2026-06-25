bits 64
section .text

; C ハンドラの宣言（isr.c で定義）
extern isr_handler

; isr.h で参照できるようエクスポート
global isr_stub_0
global isr_stub_6
global isr_stub_8
global isr_stub_13
global isr_stub_14

; エラーコードなし
isr_stub_0:
    push qword 0        ; ダミーエラーコード
    push qword 0        ; ベクタ番号
    jmp isr_common

; エラーコードなし
isr_stub_6:
    push qword 0        ; ダミーエラーコード
    push qword 6        ; ベクタ番号
    jmp isr_common

; エラーコードあり（CPU が積む）
isr_stub_8:
    push qword 8        ; ベクタ番号
    jmp isr_common

; エラーコードあり（CPU が積む）
isr_stub_13:
    push qword 13        ; ベクタ番号
    jmp isr_common

; エラーコードあり（CPU が積む）
isr_stub_14:
    push qword 14        ; ベクタ番号
    jmp isr_common

isr_common:
    ; 汎用レジスタをすべて積む（System V AMD64 ABI）
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; RSP を第一引数として C ハンドラに渡す
    ; レジスタ情報を積み上げて作成した interrupt stack frame へのポインタを渡す
    mov rdi, rsp
    call isr_handler

    ; レジスタを復元
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; ベクタ番号とエラーコードをスタックから除去（ポップはしない）
    add rsp, 16

    ; CPU が ISR 呼び出し前にスタックに積んだ RIP, CS, RFLAGS を復元して 
    ; 割り込み発生時の処理に return する
    iretq