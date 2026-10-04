#include <Treas/Hal.h>
#include "HalInternal.h"

#define HAL_PIT_INPUT_FREQUENCY 1193182UL
#define HAL_PIT_CHANNEL_0_DATA 0x40
#define HAL_PIT_COMMAND 0x43
#define HAL_PIT_RATE_GENERATOR 0x36

static VOID HalpWritePortByte(USHORT Port, UCHAR Value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

VOID HalInitializeTimer(ULONG Frequency)
{
    ULONG Divisor;

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
}
