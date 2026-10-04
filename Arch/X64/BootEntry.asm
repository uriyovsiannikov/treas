; Multiboot2 and QEMU PVH boot entry, initially in 32-bit protected mode.
BITS 32
DEFAULT ABS

SECTION .note.Xen note alloc noexec nowrite align=4
    dd 4, 4, 18
    db 'Xen', 0
    dd KiStart

SECTION .text.boot
GLOBAL KiStart
EXTERN KiKernelMain

KiStart:
    cli
    mov esp, KiBootStackTop
    mov [KiBootInformation], ebx

    mov eax, KiPdptTable
    or eax, 0x3
    mov [KiPml4Table], eax
    mov eax, KiPageDirectory0
    or eax, 0x3
    mov [KiPdptTable], eax
    mov eax, KiPageDirectory1
    or eax, 0x3
    mov [KiPdptTable + 8], eax
    mov eax, KiPageDirectory2
    or eax, 0x3
    mov [KiPdptTable + 16], eax
    mov eax, KiPageDirectory3
    or eax, 0x3
    mov [KiPdptTable + 24], eax

    mov ecx, 2048
    mov edi, KiPageDirectory0
    mov eax, 0x83
KiMap2M:
    mov [edi], eax
    mov dword [edi + 4], 0
    add eax, 0x200000
    add edi, 8
    loop KiMap2M

    mov eax, KiPml4Table
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    lgdt [KiGdt64Pointer]
    jmp 0x08:KiLongModeStart

BITS 64
KiLongModeStart:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, KiBootStackTop
    xor rbp, rbp
    mov edi, [KiBootInformation]
    call KiKernelMain

KiHalt:
    cli
    hlt
    jmp KiHalt

SECTION .data.boot
ALIGN 8
GLOBAL KiGdt64
GLOBAL KiGdtTss
GLOBAL KiBootStackTop
KiGdt64:
    dq 0
    dq 0x00AF9A000000FFFF
    dq 0x00CF92000000FFFF
    dq 0x00CFF2000000FFFF
    dq 0x00AFFA000000FFFF
KiGdtTss:
    dq 0
    dq 0
KiGdt64Pointer:
    dw $ - KiGdt64 - 1
    dq KiGdt64

SECTION .bss
ALIGN 4096
KiPml4Table:        resb 4096
KiPdptTable:        resb 4096
KiPageDirectory0:   resb 4096
KiPageDirectory1:   resb 4096
KiPageDirectory2:   resb 4096
KiPageDirectory3:   resb 4096
KiBootInformation:  resd 1
ALIGNB 16
KiBootStack:        resb 4096
KiBootStackTop:
