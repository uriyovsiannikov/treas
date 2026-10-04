BITS 64
DEFAULT REL

SECTION .text
GLOBAL UserMain

UserMain:
    lea rdi, [rel UserThreadTestEntry]
    lea rsi, [rel UserThreadTestMessage]
    mov eax, 4
    syscall
    cmp rax, -2
    je UserThreadTestFailure

    mov eax, 3
    syscall
    xor edi, edi
    mov eax, 2
    syscall

UserThreadTestFailure:
    mov edi, 9
    mov eax, 2
    syscall

UserThreadTestEntry:
    mov esi, UserThreadTestMessageEnd - UserThreadTestMessage
    mov eax, 1
    syscall
    ret

UserThreadTestMessage:
    db 'thread-ok', 10
UserThreadTestMessageEnd:
