BITS 64
DEFAULT ABS

SECTION .text
GLOBAL KiSwitchContext
GLOBAL KipThreadStartup
EXTERN KipThreadStartupBody

; KiSwitchContext(OldStackPointer, NewStackPointer, NewContextType)
KiSwitchContext:
    pushfq
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov [rdi], rsp
    mov rsp, rsi
    test edx, edx
    jnz KiRestoreInterruptContext
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    popfq
    ret

KiRestoreInterruptContext:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    test byte [rsp + 8], 3
    jz KiRestoreInterruptContextKernel
    swapgs
KiRestoreInterruptContextKernel:
    iretq

KipThreadStartup:
    cld
    sub rsp, 8
    call KipThreadStartupBody
KipThreadHalt:
    cli
    hlt
    jmp KipThreadHalt
