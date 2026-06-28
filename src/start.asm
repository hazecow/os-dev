section .text
global kernel_main

extern g_kernel_stack
extern kmain

kernel_main:
    mov rsp, g_kernel_stack + 16 * 1024
    call kmain