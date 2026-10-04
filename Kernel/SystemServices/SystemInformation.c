#include <Treas/PhysicalMemory.h>
#include <Treas/SystemService.h>
#include <Treas/Timer.h>
#include <Treas/UserSystemInformation.h>
#include <Treas/VirtualMemory.h>

ULONGLONG KipQuerySystemInformationService(ULONG_PTR Argument1,
                                           ULONGLONG Argument2)
{
    PTREAS_SYSTEM_INFORMATION Information;

    if (Argument2 != sizeof(TREAS_SYSTEM_INFORMATION) ||
        !MmIsUserRangeWritable((ULONGLONG)Argument1,
                               sizeof(TREAS_SYSTEM_INFORMATION))) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    Information = (PTREAS_SYSTEM_INFORMATION)Argument1;
    Information->Size = sizeof(TREAS_SYSTEM_INFORMATION);
    Information->Version = TREAS_SYSTEM_INFORMATION_VERSION;
    Information->ManagedMemoryBytes = MmGetManagedPageCount() * MM_PAGE_SIZE;
    Information->FreeMemoryBytes = MmGetFreePageCount() * MM_PAGE_SIZE;
    Information->TimerTicks = KiQueryTimerTickCount();
    Information->TimerFrequency = KI_SYSTEM_TIME_FREQUENCY;
    Information->ProcessorCount = 1;
    return sizeof(TREAS_SYSTEM_INFORMATION);
}
