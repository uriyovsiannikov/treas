#include <Treas/ObjectManager.h>
#include <Treas/SystemService.h>
#include <Treas/VirtualMemory.h>

ULONGLONG KipManageObjectService(ULONG_PTR Argument1, ULONGLONG Argument2)
{
    TREAS_OBJECT_REQUEST Request;
    PTREAS_OBJECT_REQUEST UserRequest;

    if (Argument2 != sizeof(Request) ||
        !MmIsUserRangeWritable((ULONGLONG)Argument1, sizeof(Request))) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    UserRequest = (PTREAS_OBJECT_REQUEST)Argument1;
    Request = *UserRequest;
    if (!ObManageUserObject(&Request)) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    *UserRequest = Request;
    return 0;
}
