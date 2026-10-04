BITS 64
DEFAULT REL

SECTION .text
GLOBAL UserMain

UserMain:
    lea rdi, [rel UserThreadPreemptTestEntry]
    lea rsi, [rel UserThreadPreemptTestMessage]
    mov eax, 4
    syscall
    cmp rax, -2
    je UserThreadPreemptTestFailure

    mov ebx, 1000
UserThreadPreemptTestYieldLoop:
    mov eax, 3
    syscall
    dec ebx
    jnz UserThreadPreemptTestYieldLoop

    xor edi, edi
    mov eax, 2
    syscall

UserThreadPreemptTestFailure:
    mov edi, 9
    mov eax, 2
    syscall

UserThreadPreemptTestEntry:
    mov ecx, 50000000
UserThreadPreemptTestSpinLoop:
    dec ecx
    jnz UserThreadPreemptTestSpinLoop
    mov esi, UserThreadPreemptTestMessageEnd - UserThreadPreemptTestMessage
    mov eax, 1
    syscall
    ret

UserThreadPreemptTestMessage:
    db 'preempt-ok', 10
UserThreadPreemptTestMessageEnd:
