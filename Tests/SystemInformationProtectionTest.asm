BITS 64
DEFAULT REL

SECTION .text
GLOBAL UserMain

UserMain:
    lea rdi, [rel UserMain]
    mov esi, 40
    mov eax, 6
    syscall
    cmp rax, -2
    jne SystemInformationProtectionFailure

    xor edi, edi
    mov eax, 2
    syscall

SystemInformationProtectionFailure:
    mov edi, 9
    mov eax, 2
    syscall
