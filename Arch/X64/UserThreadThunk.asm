BITS 64
DEFAULT REL

SECTION .rodata
GLOBAL KiUserThreadExitThunkStart
GLOBAL KiUserThreadExitThunkEnd

; mov eax, TREAS_SYSTEM_CALL_EXIT_THREAD; syscall; ud2
KiUserThreadExitThunkStart:
    db 0xB8, 0x05, 0x00, 0x00, 0x00
    db 0x0F, 0x05
    db 0x0F, 0x0B
KiUserThreadExitThunkEnd:
