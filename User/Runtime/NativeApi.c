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

PVOID TreAllocateVirtualMemory(ULONGLONG Size, ULONG Protection)
{
    TREAS_VIRTUAL_MEMORY_REQUEST Request;
    ULONGLONG Result;

    Request.BaseAddress = 0;
    Request.RegionSize = Size;
    Request.Operation = TREAS_VM_OPERATION_ALLOCATE;
    Request.Protection = Protection;
    Result = TrepSystemCall(TREAS_SYSTEM_CALL_MANAGE_VIRTUAL_MEMORY,
                            (ULONGLONG)(ULONG_PTR)&Request,
                            sizeof(Request));
    return Result == 0 ? (PVOID)(ULONG_PTR)Request.BaseAddress : 0;
}

LONG TreFreeVirtualMemory(PVOID BaseAddress)
{
    TREAS_VIRTUAL_MEMORY_REQUEST Request;
    ULONGLONG Result;

    Request.BaseAddress = (ULONGLONG)(ULONG_PTR)BaseAddress;
    Request.RegionSize = 0;
    Request.Operation = TREAS_VM_OPERATION_RELEASE;
    Request.Protection = 0;
    Result = TrepSystemCall(TREAS_SYSTEM_CALL_MANAGE_VIRTUAL_MEMORY,
                            (ULONGLONG)(ULONG_PTR)&Request,
                            sizeof(Request));
    return Result == 0 ? TREAS_STATUS_SUCCESS : TREAS_STATUS_UNSUCCESSFUL;
}

LONG TreProtectVirtualMemory(PVOID BaseAddress, ULONG Protection)
{
    TREAS_VIRTUAL_MEMORY_REQUEST Request;
    ULONGLONG Result;

    Request.BaseAddress = (ULONGLONG)(ULONG_PTR)BaseAddress;
    Request.RegionSize = 0;
    Request.Operation = TREAS_VM_OPERATION_PROTECT;
    Request.Protection = Protection;
    Result = TrepSystemCall(TREAS_SYSTEM_CALL_MANAGE_VIRTUAL_MEMORY,
                            (ULONGLONG)(ULONG_PTR)&Request,
                            sizeof(Request));
    return Result == 0 ? TREAS_STATUS_SUCCESS : TREAS_STATUS_UNSUCCESSFUL;
}

static LONG TrepExecuteFileRequest(PTREAS_FILE_REQUEST Request)
{
    ULONGLONG Result = TrepSystemCall(TREAS_SYSTEM_CALL_HOST_FILE,
                                      (ULONGLONG)(ULONG_PTR)Request,
                                      sizeof(*Request));

    return Result == 0 ? Request->Status : TREAS_STATUS_UNSUCCESSFUL;
}

LONG TreReadFile(ULONG Handle,
                 PVOID Buffer,
                 ULONGLONG Length,
                 ULONGLONG Offset)
{
    TREAS_FILE_REQUEST Request;
    LONG Status;

    Request.BufferAddress = (ULONGLONG)(ULONG_PTR)Buffer;
    Request.Offset = Offset;
    Request.Length = Length;
    Request.Result = 0;
    Request.Operation = TREAS_FILE_OPERATION_READ;
    Request.Handle = Handle;
    Request.Status = TREAS_STATUS_UNSUCCESSFUL;
    Request.Reserved = 0;
    Status = TrepExecuteFileRequest(&Request);
    return Status == TREAS_STATUS_SUCCESS
               ? (LONG)Request.Result
               : Status;
}

LONG TreWriteFile(ULONG Handle,
                  const VOID *Buffer,
                  ULONGLONG Length,
                  ULONGLONG Offset)
{
    TREAS_FILE_REQUEST Request;
    LONG Status;

    Request.BufferAddress = (ULONGLONG)(ULONG_PTR)Buffer;
    Request.Offset = Offset;
    Request.Length = Length;
    Request.Result = 0;
    Request.Operation = TREAS_FILE_OPERATION_WRITE;
    Request.Handle = Handle;
    Request.Status = TREAS_STATUS_UNSUCCESSFUL;
    Request.Reserved = 0;
    Status = TrepExecuteFileRequest(&Request);
    return Status == TREAS_STATUS_SUCCESS
               ? (LONG)Request.Result
               : Status;
}

LONG TreQueryFileSize(ULONG Handle, ULONGLONG *Size)
{
    TREAS_FILE_REQUEST Request;
    LONG Status;

    if (Size == 0) {
        return TREAS_STATUS_INVALID_PARAMETER;
    }
    Request.BufferAddress = 0;
    Request.Offset = 0;
    Request.Length = 0;
    Request.Result = 0;
    Request.Operation = TREAS_FILE_OPERATION_QUERY_SIZE;
    Request.Handle = Handle;
    Request.Status = TREAS_STATUS_UNSUCCESSFUL;
    Request.Reserved = 0;
    Status = TrepExecuteFileRequest(&Request);
    if (Status == TREAS_STATUS_SUCCESS) {
        *Size = Request.Result;
    }
    return Status;
}

LONG TreCloseFile(ULONG Handle)
{
    TREAS_FILE_REQUEST Request;

    Request.BufferAddress = 0;
    Request.Offset = 0;
    Request.Length = 0;
    Request.Result = 0;
    Request.Operation = TREAS_FILE_OPERATION_CLOSE;
    Request.Handle = Handle;
    Request.Status = TREAS_STATUS_UNSUCCESSFUL;
    Request.Reserved = 0;
    return TrepExecuteFileRequest(&Request);
}

static LONG TrepManageObject(PTREAS_OBJECT_REQUEST Request)
{
    ULONGLONG Result = TrepSystemCall(TREAS_SYSTEM_CALL_MANAGE_OBJECT,
                                      (ULONGLONG)(ULONG_PTR)Request,
                                      sizeof(*Request));

    return Result == 0 ? TREAS_STATUS_SUCCESS : TREAS_STATUS_UNSUCCESSFUL;
}

LONG TreCreateEvent(BOOLEAN ManualReset, BOOLEAN InitialState)
{
    TREAS_OBJECT_REQUEST Request;

    Request.Operation = TREAS_OBJECT_OPERATION_CREATE_EVENT;
    Request.Handle = 0;
    Request.DesiredAccess = TREAS_EVENT_MODIFY_STATE | TREAS_SYNCHRONIZE;
    Request.Flags = (ManualReset ? TREAS_EVENT_MANUAL_RESET : 0) |
                    (InitialState ? TREAS_EVENT_INITIAL_STATE : 0);
    return TrepManageObject(&Request) == TREAS_STATUS_SUCCESS
               ? (LONG)Request.Handle
               : TREAS_STATUS_UNSUCCESSFUL;
}

static LONG TrepManageEvent(ULONG Operation, ULONG Handle)
{
    TREAS_OBJECT_REQUEST Request;

    Request.Operation = Operation;
    Request.Handle = Handle;
    Request.DesiredAccess = 0;
    Request.Flags = 0;
    return TrepManageObject(&Request);
}

LONG TreSetEvent(ULONG Handle)
{
    return TrepManageEvent(TREAS_OBJECT_OPERATION_SET_EVENT, Handle);
}

LONG TreResetEvent(ULONG Handle)
{
    return TrepManageEvent(TREAS_OBJECT_OPERATION_RESET_EVENT, Handle);
}

LONG TreWaitForEvent(ULONG Handle)
{
    return TrepManageEvent(TREAS_OBJECT_OPERATION_WAIT, Handle);
}

LONG TreCloseHandle(ULONG Handle)
{
    return TrepManageEvent(TREAS_OBJECT_OPERATION_CLOSE, Handle);
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
