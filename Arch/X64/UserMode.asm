BITS 64
DEFAULT ABS

SECTION .text
GLOBAL KiEnterUserMode

KiEnterUserMode:
    cli
    cld
    mov r8, rdi
    mov r9, rsi
    mov r10, rdx
    mov r11, rcx
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax
    swapgs

    push qword 0x1B
    push r9
    push qword 0x202
    push qword 0x23
    push r8
    mov rdi, r10
    mov rsi, r11
    iretq
