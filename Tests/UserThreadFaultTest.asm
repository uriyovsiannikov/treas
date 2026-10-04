BITS 64
DEFAULT REL

SECTION .text
GLOBAL UserMain

UserMain:
    lea rdi, [rel UserThreadFaultTestEntry]
    xor esi, esi
    mov eax, 4
    syscall
    cmp rax, -2
    je UserThreadFaultTestFailure

    mov eax, 3
    syscall
    xor edi, edi
    mov eax, 2
    syscall

UserThreadFaultTestFailure:
    mov edi, 9
    mov eax, 2
    syscall

UserThreadFaultTestEntry:
    ud2
