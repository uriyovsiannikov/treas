#include <Treas/ProcessStartup.h>
#include <Treas/SystemService.h>
#include <Treas/UserVirtualMemoryManager.h>

ULONGLONG KipManageVirtualMemoryService(ULONG_PTR Argument1,
                                        ULONGLONG Argument2)
{
    TREAS_VIRTUAL_MEMORY_REQUEST Request;
    PTREAS_VIRTUAL_MEMORY_REQUEST UserRequest;

    if (Argument2 != sizeof(Request) ||
        !MmIsUserRangeWritable((ULONGLONG)Argument1, sizeof(Request))) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    UserRequest = (PTREAS_VIRTUAL_MEMORY_REQUEST)Argument1;
    Request = *UserRequest;
    if (!MiManageUserVirtualMemory(KiGetInitialProcessAddressSpace(),
                                   &Request)) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    *UserRequest = Request;
    return 0;
}
