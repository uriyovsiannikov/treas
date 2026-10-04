#include <Treas/Hal.h>
#include "HalInternal.h"

#define HAL_PIT_INPUT_FREQUENCY 1193182UL
#define HAL_PIT_CHANNEL_0_DATA 0x40
#define HAL_PIT_COMMAND 0x43
#define HAL_PIT_RATE_GENERATOR 0x36

static ULONG HalpTimerReferenceCount;
static ULONGLONG HalpPerformanceFrequency;

static VOID HalpWritePortByte(USHORT Port, UCHAR Value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

VOID HalInitializeTimer(ULONG Frequency)
{
    ULONG Divisor;
    ULONG MaximumFunction;
    ULONG Denominator;
    ULONG Numerator;
    ULONG CrystalFrequency;
    ULONG Edx;

    if (Frequency == 0 || Frequency > HAL_PIT_INPUT_FREQUENCY) {
        return;
    }

    Divisor = HAL_PIT_INPUT_FREQUENCY / Frequency;
    if (Divisor == 0 || Divisor > 0xFFFF) {
        return;
    }

    HalpWritePortByte(HAL_PIT_COMMAND, HAL_PIT_RATE_GENERATOR);
    HalpWritePortByte(HAL_PIT_CHANNEL_0_DATA, (UCHAR)(Divisor & 0xFF));
    HalpWritePortByte(HAL_PIT_CHANNEL_0_DATA, (UCHAR)(Divisor >> 8));
    HalpInitializePic();

    __asm__ volatile ("cpuid"
                      : "=a"(MaximumFunction), "=b"(Numerator),
                        "=c"(CrystalFrequency), "=d"(Edx)
                      : "a"(0), "c"(0));
    if (MaximumFunction >= 0x15) {
        __asm__ volatile ("cpuid"
                          : "=a"(Denominator), "=b"(Numerator),
                            "=c"(CrystalFrequency), "=d"(Edx)
                          : "a"(0x15), "c"(0));
        if (Denominator != 0 && Numerator != 0 && CrystalFrequency != 0) {
            HalpPerformanceFrequency =
                ((ULONGLONG)CrystalFrequency * Numerator) / Denominator;
        }
    }
    if (HalpPerformanceFrequency == 0 && MaximumFunction >= 0x16) {
        ULONG BaseMegahertz;

        __asm__ volatile ("cpuid"
                          : "=a"(BaseMegahertz), "=b"(Numerator),
                            "=c"(CrystalFrequency), "=d"(Edx)
                          : "a"(0x16), "c"(0));
        HalpPerformanceFrequency = (ULONGLONG)BaseMegahertz * 1000000ULL;
    }
    if (HalpPerformanceFrequency == 0) {
        HalpPerformanceFrequency = 1000000000ULL;
    }
}

VOID HalAcquireTimer(VOID)
{
    if (HalpTimerReferenceCount++ == 0) {
        HalpSetTimerMasked(FALSE);
    }
}

VOID HalReleaseTimer(VOID)
{
    if (HalpTimerReferenceCount != 0 && --HalpTimerReferenceCount == 0) {
        HalpSetTimerMasked(TRUE);
    }
}

ULONGLONG HalQueryPerformanceCounter(VOID)
{
    ULONG LowValue;
    ULONG HighValue;

    __asm__ volatile ("rdtsc" : "=a"(LowValue), "=d"(HighValue));
    return ((ULONGLONG)HighValue << 32) | LowValue;
}

ULONGLONG HalQueryPerformanceFrequency(VOID)
{
    return HalpPerformanceFrequency;
}
