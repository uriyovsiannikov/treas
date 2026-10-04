#include <Treas/Hal.h>
#include <Treas/Timer.h>

static volatile ULONGLONG KipTimerTickCount;

VOID KiTimerInterrupt(VOID)
{
    KipTimerTickCount++;
    HalAcknowledgeTimerInterrupt();
}

ULONGLONG KiQueryTimerTickCount(VOID)
{
    return KipTimerTickCount;
}
