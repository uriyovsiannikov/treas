BITS 64
DEFAULT ABS

SECTION .rodata
GLOBAL KiExceptionStubTable
KiExceptionStubTable:
%assign ExceptionVector 0
%rep 32
    dq KiExceptionVector %+ ExceptionVector
%assign ExceptionVector ExceptionVector + 1
%endrep

SECTION .text
EXTERN KiHandleException
EXTERN KiSystemCallDispatch
EXTERN KiTimerInterrupt
EXTERN KiScheduleFromInterrupt
EXTERN KiValidateUserReturnContext
EXTERN KiTerminateInitialUserProcess
EXTERN KiUserProcessExited
EXTERN MmSwitchToKernelAddressSpace
GLOBAL KiExceptionCommon
GLOBAL KiSystemCallEntry
GLOBAL KiTimerInterruptEntry

%assign ExceptionVector 0
%rep 32
GLOBAL KiExceptionVector %+ ExceptionVector
KiExceptionVector %+ ExceptionVector:
%if ExceptionVector = 8 || ExceptionVector = 10 || ExceptionVector = 11 || \
    ExceptionVector = 12 || ExceptionVector = 13 || ExceptionVector = 14 || \
    ExceptionVector = 17 || ExceptionVector = 21 || ExceptionVector = 29 || \
    ExceptionVector = 30
    push qword ExceptionVector
%else
    push qword 0
    push qword ExceptionVector
%endif
    jmp KiExceptionCommon
%assign ExceptionVector ExceptionVector + 1
%endrep

KiExceptionCommon:
    cli
    test byte [rsp + 24], 3
    jz KiExceptionKernelEntry
    swapgs
KiExceptionKernelEntry:
    mov rdi, rsp
    mov rsp, KiBugCheckStackTop
    and rsp, -16
    call KiHandleException

KiExceptionHalt:
    cli
    hlt
    jmp KiExceptionHalt

KiTimerInterruptEntry:
    cld
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov r12, rsp
    test byte [r12 + 128], 3
    jz KiTimerInterruptKernelEntry
    swapgs
KiTimerInterruptKernelEntry:
    and rsp, -16
    call KiTimerInterrupt
    mov rsp, r12
    test byte [r12 + 128], 3
    jz KiTimerInterruptReturn

    mov rdi, r12
    and rsp, -16
    sub rsp, 16
    lea rsi, [rsp]
    lea rdx, [rsp + 8]
    call KiScheduleFromInterrupt
    mov rax, [rsp]
    mov edx, [rsp + 8]
    mov rsp, rax
    test edx, edx
    jnz KiRestoreInterruptFrame

KiRestoreCooperativeContext:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    popfq
    ret

KiTimerInterruptReturn:
KiRestoreInterruptFrame:
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
    jz KiRestoreInterruptFrameKernel
    swapgs
KiRestoreInterruptFrameKernel:
    iretq

KiSystemCallEntry:
    cld
    swapgs
    mov r10, rsp
    mov r11, rcx
    mov rsp, [gs:8]
    and rsp, -16
    sub rsp, 32
    mov [rsp], r10
    mov [rsp + 8], r11
    mov rdx, rsi
    mov rsi, rdi
    mov edi, eax
    call KiSystemCallDispatch
    cmp rax, -1
    je KiSystemCallExit

    mov [rsp + 16], rax
    mov rdi, [rsp + 8]
    mov rsi, [rsp]
    call KiValidateUserReturnContext
    test al, al
    jz KiSystemCallFault

    mov rcx, [rsp + 8]
    mov rax, [rsp + 16]
    mov r11, 0x202
    mov rsp, [rsp]
    swapgs
    o64 sysret

KiSystemCallFault:
    mov rsp, [gs:8]
    and rsp, -16
    call MmSwitchToKernelAddressSpace
    mov edi, 127
    call KiTerminateInitialUserProcess

KiSystemCallExit:
    mov rsp, [gs:8]
    and rsp, -16
    call MmSwitchToKernelAddressSpace
    call KiUserProcessExited

KiSystemCallHalt:
    cli
    hlt
    jmp KiSystemCallHalt

SECTION .bss
ALIGNB 16
KiBugCheckStack:    resb 4096
KiBugCheckStackTop:
