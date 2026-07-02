bits 64
section .text

; C ハンドラの宣言（interrupt.c で定義）
extern interrupt_handler

; エラーコードなしスタブを生成するマクロ
%macro ISR_NO_ERROR 1
global isr_stub_%1
isr_stub_%1:
    push qword 0     ; ダミーエラーコード
    push qword %1    ; ベクタ番号
    jmp isr_common
%endmacro

; エラーコードありスタブを生成するマクロ
%macro ISR_ERROR 1
global isr_stub_%1
isr_stub_%1:
    push qword %1    ; ベクタ番号
    jmp isr_common
%endmacro



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
    call interrupt_handler

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



; isr_stub を追加するときは以下に追加する

ISR_NO_ERROR 0      ; #DE, division error
ISR_NO_ERROR 6      ; #UD, invalid opcode
ISR_ERROR    8      ; #DF, double fault
ISR_ERROR    13     ; #GP, general protection fault
ISR_ERROR    14     ; #PF, page fault
ISR_NO_ERROR 255    ; spurious interrupt
ISR_NO_ERROR 32     ; LAPIC timer