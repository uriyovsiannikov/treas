BITS 64
DEFAULT REL

SECTION .text
GLOBAL UserMain
EXTERN TreUserMain
EXTERN TreExitProcess

UserMain:
    cld
    call TreUserMain
    mov edi, eax
    call TreExitProcess
    ud2

SECTION .note.GNU-stack noalloc noexec nowrite progbits
