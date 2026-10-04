#include <Treas/Hal.h>
#include "HalInternal.h"

#define HAL_PIC_MASTER_COMMAND 0x20
#define HAL_PIC_MASTER_DATA 0x21
#define HAL_PIC_SLAVE_COMMAND 0xA0
#define HAL_PIC_SLAVE_DATA 0xA1
#define HAL_PIC_END_OF_INTERRUPT 0x20

static VOID HalpWritePortByte(USHORT Port, UCHAR Value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

static VOID HalpWaitForPortIo(VOID)
{
    HalpWritePortByte(0x80, 0);
}

VOID HalpInitializePic(VOID)
{
    UCHAR MasterMask;
    UCHAR SlaveMask;

    MasterMask = 0xFF;
    SlaveMask = 0xFF;
    HalpWritePortByte(HAL_PIC_MASTER_DATA, MasterMask);
    HalpWritePortByte(HAL_PIC_SLAVE_DATA, SlaveMask);

    HalpWritePortByte(HAL_PIC_MASTER_COMMAND, 0x11);
    HalpWaitForPortIo();
    HalpWritePortByte(HAL_PIC_SLAVE_COMMAND, 0x11);
    HalpWaitForPortIo();
    HalpWritePortByte(HAL_PIC_MASTER_DATA, 0x20);
    HalpWaitForPortIo();
    HalpWritePortByte(HAL_PIC_SLAVE_DATA, 0x28);
    HalpWaitForPortIo();
    HalpWritePortByte(HAL_PIC_MASTER_DATA, 0x04);
    HalpWaitForPortIo();
    HalpWritePortByte(HAL_PIC_SLAVE_DATA, 0x02);
    HalpWaitForPortIo();
    HalpWritePortByte(HAL_PIC_MASTER_DATA, 0x01);
    HalpWaitForPortIo();
    HalpWritePortByte(HAL_PIC_SLAVE_DATA, 0x01);
    HalpWaitForPortIo();

    HalpWritePortByte(HAL_PIC_MASTER_DATA, 0xFF);
    HalpWritePortByte(HAL_PIC_SLAVE_DATA, 0xFF);
}

VOID HalpSetTimerMasked(BOOLEAN Masked)
{
    HalpWritePortByte(HAL_PIC_MASTER_DATA, Masked ? 0xFF : 0xFE);
}

VOID HalAcknowledgeTimerInterrupt(VOID)
{
    HalpWritePortByte(HAL_PIC_MASTER_COMMAND, HAL_PIC_END_OF_INTERRUPT);
}
