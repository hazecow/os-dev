section .text
global gdt_flush

; void gdt_flush(uint64_t gdtr_addr)
; rdi = &gdtr
gdt_flush:
    lgdt [rdi]          ; GDTR をロード

    ; 先に DS/ES/FS/GS/SS を Kernel Data セレクタに更新
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; CS は far return で更新する
    ; スタックに RIP と CS を積んで retfq
    pop  rax            ; 戻りアドレスを退避
    push qword 0x08     ; 新しい CS (Kernel Code)
    push rax            ; 戻りアドレス
    retfq               ; far return → CS が 0x08 に切り替わる