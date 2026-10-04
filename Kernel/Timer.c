#include <Treas/Hal.h>
#include <Treas/Timer.h>

static ULONGLONG KipStartCounter;
static ULONGLONG KipCounterFrequency;

VOID KiInitializeClock(VOID)
{
    KipCounterFrequency = HalQueryPerformanceFrequency();
    KipStartCounter = HalQueryPerformanceCounter();
}

VOID KiTimerInterrupt(VOID)
{
    HalAcknowledgeTimerInterrupt();
}

ULONGLONG KiQueryTimerTickCount(VOID)
{
    ULONGLONG Elapsed = HalQueryPerformanceCounter() - KipStartCounter;
    ULONGLONG Seconds;
    ULONGLONG Remainder;

    if (KipCounterFrequency == 0) {
        return 0;
    }
    Seconds = Elapsed / KipCounterFrequency;
    Remainder = Elapsed % KipCounterFrequency;
    return Seconds * KI_SYSTEM_TIME_FREQUENCY +
           (Remainder * KI_SYSTEM_TIME_FREQUENCY) / KipCounterFrequency;
}
