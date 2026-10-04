#include <Treas/Hal.h>
#include <Treas/SystemService.h>
#include <Treas/UserAbi.h>
#include <Treas/UserFile.h>
#include <Treas/VirtualMemory.h>

ULONGLONG KipWriteBufferService(ULONG_PTR Argument1, ULONGLONG Argument2)
{
    if (Argument2 > TREAS_MAX_SYSTEM_CALL_WRITE_SIZE ||
        !MmIsUserRangeValid((ULONGLONG)Argument1, Argument2)) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    HalWriteApplicationBuffer((const CHAR *)(ULONG_PTR)Argument1, Argument2);
    return Argument2;
}

ULONGLONG KipWriteErrorBufferService(ULONG_PTR Argument1, ULONGLONG Argument2)
{
    if (Argument2 > TREAS_MAX_SYSTEM_CALL_WRITE_SIZE ||
        !MmIsUserRangeValid((ULONGLONG)Argument1, Argument2)) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    HalWriteApplicationErrorBuffer((const CHAR *)(ULONG_PTR)Argument1,
                                   Argument2);
    return Argument2;
}

ULONGLONG KipReadBufferService(ULONG_PTR Argument1, ULONGLONG Argument2)
{
    if (Argument2 > TREAS_MAX_SYSTEM_CALL_READ_SIZE ||
        !MmIsUserRangeWritable((ULONGLONG)Argument1, Argument2)) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    return HalReadApplicationBuffer((CHAR *)(ULONG_PTR)Argument1, Argument2);
}

ULONGLONG KipHostFileService(ULONG_PTR Argument1, ULONGLONG Argument2)
{
    TREAS_FILE_REQUEST Request;
    PTREAS_FILE_REQUEST UserRequest;

    if (Argument2 != sizeof(Request) ||
        !MmIsUserRangeWritable((ULONGLONG)Argument1, sizeof(Request))) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    UserRequest = (PTREAS_FILE_REQUEST)Argument1;
    Request = *UserRequest;
    if (Request.Reserved != 0 ||
        Request.Length > TREAS_MAX_FILE_TRANSFER_SIZE ||
        ((Request.Operation == TREAS_FILE_OPERATION_READ) &&
         !MmIsUserRangeWritable(Request.BufferAddress, Request.Length)) ||
        ((Request.Operation == TREAS_FILE_OPERATION_WRITE) &&
         !MmIsUserRangeValid(Request.BufferAddress, Request.Length)) ||
        ((Request.Operation == TREAS_FILE_OPERATION_QUERY_SIZE ||
          Request.Operation == TREAS_FILE_OPERATION_CLOSE) &&
         Request.Length != 0)) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    HalExecuteHostFileRequest(&Request);
    *UserRequest = Request;
    return 0;
}
