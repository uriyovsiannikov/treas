#include <Treas/Process.h>
#include <Treas/ProcessStartup.h>
#include <Treas/SystemService.h>
#include <Treas/Thread.h>
#include <Treas/UserThread.h>
#include <Treas/VirtualMemory.h>

ULONGLONG KipExitProcessService(ULONG_PTR Argument1, ULONGLONG Argument2)
{
    (void)Argument2;
    if (Argument1 > 127) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    PsSetInitialProcessExitStatus((ULONG)Argument1);
    return ~0ULL;
}

ULONGLONG KipYieldThreadService(ULONG_PTR Argument1, ULONGLONG Argument2)
{
    (void)Argument1;
    (void)Argument2;
    KeYieldThread();
    return 0;
}

ULONGLONG KipCreateThreadService(ULONG_PTR Argument1, ULONGLONG Argument2)
{
    ULONGLONG StackPointer;
    ULONGLONG StackPages[MI_USER_THREAD_STACK_PAGE_COUNT];
    ULONG StackSlot;
    ULONG ThreadId;
    PMM_ADDRESS_SPACE AddressSpace = KiGetInitialProcessAddressSpace();

    if (!MmIsUserAddressValid((ULONGLONG)Argument1, FALSE, TRUE) ||
        !MiCreateUserThreadStack(AddressSpace, &StackPointer,
                                 &StackSlot, StackPages)) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    if (!KeCreateUserThread(AddressSpace, (ULONGLONG)Argument1, Argument2,
                            StackPointer, StackSlot, StackPages, &ThreadId)) {
        MiDestroyUserThreadStack(AddressSpace, StackSlot, StackPages,
                                 MI_USER_THREAD_STACK_PAGE_COUNT);
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    return ThreadId;
}

ULONGLONG KipExitThreadService(ULONG_PTR Argument1, ULONGLONG Argument2)
{
    (void)Argument1;
    (void)Argument2;
    if (KeCurrentThreadIsPrimaryUser()) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    KeTerminateCurrentUserThread();
    return 0;
}
