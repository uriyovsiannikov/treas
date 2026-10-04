#include <Treas/Hal.h>
#include <Treas/PhysicalMemory.h>
#include <Treas/ProcessStartup.h>
#include <Treas/SystemCall.h>
#include <Treas/Thread.h>
#include <Treas/Timer.h>
#include <Treas/UserThread.h>
#include <Treas/UserSystemInformation.h>
#include <Treas/VirtualMemory.h>

static ULONG KipInitialProcessExitCode;

ULONGLONG KiSystemCallDispatch(ULONG ServiceNumber,
                               ULONG_PTR Argument1,
                               ULONGLONG Argument2)
{
    if (ServiceNumber == TREAS_SYSTEM_CALL_WRITE_BUFFER) {
        if (Argument2 > TREAS_MAX_SYSTEM_CALL_WRITE_SIZE ||
            !MmIsUserRangeValid((ULONGLONG)Argument1, Argument2)) {
            return 0xFFFFFFFFFFFFFFFEULL;
        }

        HalWriteApplicationBuffer((const CHAR *)(ULONG_PTR)Argument1, Argument2);
        return Argument2;
    }

    if (ServiceNumber == TREAS_SYSTEM_CALL_WRITE_ERROR_BUFFER) {
        if (Argument2 > TREAS_MAX_SYSTEM_CALL_WRITE_SIZE ||
            !MmIsUserRangeValid((ULONGLONG)Argument1, Argument2)) {
            return 0xFFFFFFFFFFFFFFFEULL;
        }

        HalWriteApplicationErrorBuffer((const CHAR *)(ULONG_PTR)Argument1,
                                       Argument2);
        return Argument2;
    }

    if (ServiceNumber == TREAS_SYSTEM_CALL_READ_BUFFER) {
        if (Argument2 > TREAS_MAX_SYSTEM_CALL_READ_SIZE ||
            !MmIsUserRangeWritable((ULONGLONG)Argument1, Argument2)) {
            return 0xFFFFFFFFFFFFFFFEULL;
        }

        return HalReadApplicationBuffer((CHAR *)(ULONG_PTR)Argument1,
                                        Argument2);
    }

    if (ServiceNumber == TREAS_SYSTEM_CALL_EXIT) {
        if (Argument1 > 127) {
            return 0xFFFFFFFFFFFFFFFEULL;
        }
        KipInitialProcessExitCode = (ULONG)Argument1;
        return ~0ULL;
    }

    if (ServiceNumber == TREAS_SYSTEM_CALL_YIELD) {
        KeYieldThread();
        return 0;
    }

    if (ServiceNumber == TREAS_SYSTEM_CALL_CREATE_THREAD) {
        ULONGLONG StackPointer;
        ULONGLONG StackPages[MI_USER_THREAD_STACK_PAGE_COUNT];
        ULONG StackSlot;
        ULONG ThreadId;
        PMM_ADDRESS_SPACE AddressSpace = KiGetInitialProcessAddressSpace();

        if (!MmIsUserAddressValid((ULONGLONG)Argument1, FALSE, TRUE) ||
            !MiCreateUserThreadStack(AddressSpace, &StackPointer,
                                     &StackSlot, StackPages)) {
            return 0xFFFFFFFFFFFFFFFEULL;
        }
        if (!KeCreateUserThread(AddressSpace, (ULONGLONG)Argument1, Argument2,
                                StackPointer, StackSlot, StackPages,
                                &ThreadId)) {
            MiDestroyUserThreadStack(AddressSpace, StackSlot,
                                     StackPages, MI_USER_THREAD_STACK_PAGE_COUNT);
            return 0xFFFFFFFFFFFFFFFEULL;
        }
        return ThreadId;
    }

    if (ServiceNumber == TREAS_SYSTEM_CALL_EXIT_THREAD) {
        if (KeCurrentThreadIsPrimaryUser()) {
            return 0xFFFFFFFFFFFFFFFEULL;
        }
        KeTerminateCurrentUserThread();
        return 0;
    }

    if (ServiceNumber == TREAS_SYSTEM_CALL_QUERY_SYSTEM_INFORMATION) {
        PTREAS_SYSTEM_INFORMATION Information;

        if (Argument2 != sizeof(TREAS_SYSTEM_INFORMATION) ||
            !MmIsUserRangeWritable((ULONGLONG)Argument1,
                                   sizeof(TREAS_SYSTEM_INFORMATION))) {
            return 0xFFFFFFFFFFFFFFFEULL;
        }

        Information = (PTREAS_SYSTEM_INFORMATION)Argument1;
        Information->Size = sizeof(TREAS_SYSTEM_INFORMATION);
        Information->Version = TREAS_SYSTEM_INFORMATION_VERSION;
        Information->ManagedMemoryBytes = MmGetManagedPageCount() * MM_PAGE_SIZE;
        Information->FreeMemoryBytes = MmGetFreePageCount() * MM_PAGE_SIZE;
        Information->TimerTicks = KiQueryTimerTickCount();
        Information->TimerFrequency = KI_SYSTEM_TIMER_FREQUENCY;
        Information->ProcessorCount = 1;
        return sizeof(TREAS_SYSTEM_INFORMATION);
    }

    return 0xFFFFFFFFFFFFFFFEULL;
}

BOOLEAN KiValidateUserReturnContext(ULONGLONG InstructionPointer,
                                   ULONGLONG StackPointer)
{
    if (StackPointer <= MM_USER_ADDRESS_MIN ||
        StackPointer >= MM_USER_ADDRESS_LIMIT ||
        !MmIsUserAddressValid(InstructionPointer, FALSE, TRUE) ||
        !MmIsUserAddressValid(StackPointer - 1, TRUE, FALSE)) {
        return FALSE;
    }

    return TRUE;
}

VOID KiTerminateInitialUserProcess(ULONG ExitCode)
{
    KipInitialProcessExitCode = ExitCode > 127 ? 127 : ExitCode;
    HalFlushApplicationStreams();
    HalTerminateVirtualMachine(KipInitialProcessExitCode);
}

VOID KiUserProcessExited(VOID)
{
    KiTerminateInitialUserProcess(KipInitialProcessExitCode);
}
