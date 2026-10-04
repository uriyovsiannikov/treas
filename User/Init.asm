BITS 64
DEFAULT REL

SECTION .text
GLOBAL UserMain

UserMain:
    mov r12, rsi
    mov r13, rdi

    lea rdi, [rel UserPrefix]
    mov esi, UserPrefixEnd - UserPrefix
    mov eax, 1
    syscall

    xor ebx, ebx
UserArgumentLoop:
    cmp rbx, r13
    jae UserWriteNewLine

    mov rdi, [r12 + rbx * 8]
    xor esi, esi
UserArgumentLength:
    cmp byte [rdi + rsi], 0
    je UserWriteArgument
    inc rsi
    jmp UserArgumentLength

UserWriteArgument:
    mov eax, 1
    syscall
    inc rbx
    cmp rbx, r13
    jae UserArgumentLoop

    lea rdi, [rel UserSeparator]
    mov esi, 1
    mov eax, 1
    syscall
    jmp UserArgumentLoop

UserWriteNewLine:
    lea rdi, [rel UserNewLine]
    mov esi, 1
    mov eax, 1
    syscall

    mov edi, 0
    mov eax, 2
    syscall

UserHalt:
    jmp UserHalt

UserPrefix:
    db "argv: "
UserPrefixEnd:
UserSeparator:
    db " "
UserNewLine:
    db 10

SECTION .note.GNU-stack noalloc noexec nowrite progbits
