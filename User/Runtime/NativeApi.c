#include <Treas/UserApi.h>

static ULONGLONG TrepSystemCall(ULONG ServiceNumber,
                                ULONGLONG Argument1,
                                ULONGLONG Argument2)
{
    register ULONGLONG Result __asm__("rax") = ServiceNumber;
    register ULONGLONG FirstArgument __asm__("rdi") = Argument1;
    register ULONGLONG SecondArgument __asm__("rsi") = Argument2;

    __asm__ volatile ("syscall"
                      : "+a"(Result), "+D"(FirstArgument), "+S"(SecondArgument)
                      :
                      : "rcx", "r11", "rdx", "r8", "r9", "r10", "memory", "cc");
    return Result;
}

LONG TreWriteBuffer(const VOID *Buffer, ULONGLONG Length)
{
    ULONGLONG Result = TrepSystemCall(TREAS_SYSTEM_CALL_WRITE_BUFFER,
                                      (ULONGLONG)(ULONG_PTR)Buffer,
                                      Length);

    return Result == Length ? (LONG)Length : TREAS_STATUS_UNSUCCESSFUL;
}

LONG TreWriteErrorBuffer(const VOID *Buffer, ULONGLONG Length)
{
    ULONGLONG Result = TrepSystemCall(TREAS_SYSTEM_CALL_WRITE_ERROR_BUFFER,
                                      (ULONGLONG)(ULONG_PTR)Buffer,
                                      Length);

    return Result == Length ? (LONG)Length : TREAS_STATUS_UNSUCCESSFUL;
}

LONG TreReadBuffer(VOID *Buffer, ULONGLONG Length)
{
    ULONGLONG Result;

    if (Buffer == 0 || Length > TREAS_MAX_SYSTEM_CALL_READ_SIZE) {
        return TREAS_STATUS_UNSUCCESSFUL;
    }

    Result = TrepSystemCall(TREAS_SYSTEM_CALL_READ_BUFFER,
                            (ULONGLONG)(ULONG_PTR)Buffer,
                            Length);
    if (Result == 0xFFFFFFFFFFFFFFFEULL) {
        return TREAS_STATUS_UNSUCCESSFUL;
    }
    return (LONG)Result;
}

LONG TreQuerySystemInformation(PTREAS_SYSTEM_INFORMATION Information)
{
    ULONGLONG Result;

    if (Information == 0) {
        return TREAS_STATUS_UNSUCCESSFUL;
    }

    Result = TrepSystemCall(TREAS_SYSTEM_CALL_QUERY_SYSTEM_INFORMATION,
                            (ULONGLONG)(ULONG_PTR)Information,
                            sizeof(*Information));
    if (Result != sizeof(*Information) ||
        Information->Size != sizeof(*Information) ||
        Information->Version != TREAS_SYSTEM_INFORMATION_VERSION) {
        return TREAS_STATUS_UNSUCCESSFUL;
    }

    return TREAS_STATUS_SUCCESS;
}

LONG TreCreateThread(VOID (*StartRoutine)(PVOID), PVOID Context)
{
    ULONGLONG Result;

    if (StartRoutine == 0) {
        return TREAS_STATUS_UNSUCCESSFUL;
    }

    Result = TrepSystemCall(TREAS_SYSTEM_CALL_CREATE_THREAD,
                            (ULONGLONG)(ULONG_PTR)StartRoutine,
                            (ULONGLONG)(ULONG_PTR)Context);
    return Result == 0xFFFFFFFFFFFFFFFEULL
               ? TREAS_STATUS_UNSUCCESSFUL
               : (LONG)Result;
}

VOID TreExitThread(VOID)
{
    (VOID)TrepSystemCall(TREAS_SYSTEM_CALL_EXIT_THREAD, 0, 0);
}

VOID TreYieldThread(VOID)
{
    (VOID)TrepSystemCall(TREAS_SYSTEM_CALL_YIELD, 0, 0);
}

VOID TreExitProcess(ULONG ExitCode)
{
    if (ExitCode > 127) {
        ExitCode = 127;
    }
    (VOID)TrepSystemCall(TREAS_SYSTEM_CALL_EXIT, ExitCode, 0);
    for (;;) {
        __asm__ volatile ("pause");
    }
}
