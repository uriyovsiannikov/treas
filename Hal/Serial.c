#include <Treas/Hal.h>

#define COM2_PORT 0x2F8
#define COM3_PORT 0x3E8

static BOOLEAN HalpApplicationInputEnded;

static VOID HalpWritePortByte(USHORT Port, UCHAR Value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

static UCHAR HalpReadPortByte(USHORT Port)
{
    UCHAR Value;

    __asm__ volatile ("inb %1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

static VOID HalpInitializeSerialPort(USHORT BasePort,
                                    BOOLEAN ResetReceiveFifo)
{
    HalpWritePortByte(BasePort + 1, 0x00);
    HalpWritePortByte(BasePort + 3, 0x80);
    HalpWritePortByte(BasePort + 0, 0x01);
    HalpWritePortByte(BasePort + 1, 0x00);
    HalpWritePortByte(BasePort + 3, 0x03);
    HalpWritePortByte(BasePort + 2,
                      ResetReceiveFifo ? 0xC7 : 0xC1);
    HalpWritePortByte(BasePort + 4, 0x0B);
}

VOID HalInitializeSerial(VOID)
{
    HalpInitializeSerialPort(COM2_PORT, FALSE);
    HalpInitializeSerialPort(COM3_PORT, TRUE);
}

VOID HalWriteApplicationBuffer(const CHAR *Buffer, ULONGLONG Length)
{
    ULONGLONG Index;

    for (Index = 0; Index < Length; Index++) {
        while ((HalpReadPortByte(COM2_PORT + 5) & 0x20) == 0) {
            __asm__ volatile ("pause");
        }
        HalpWritePortByte(COM2_PORT, (UCHAR)Buffer[Index]);
    }
}

VOID HalWriteApplicationErrorBuffer(const CHAR *Buffer, ULONGLONG Length)
{
    ULONGLONG Index;

    for (Index = 0; Index < Length; Index++) {
        while ((HalpReadPortByte(COM3_PORT + 5) & 0x20) == 0) {
            __asm__ volatile ("pause");
        }
        HalpWritePortByte(COM3_PORT, (UCHAR)Buffer[Index]);
    }
}

VOID HalFlushApplicationStreams(VOID)
{
    while ((HalpReadPortByte(COM2_PORT + 5) & 0x40) == 0) {
        __asm__ volatile ("pause");
    }
    while ((HalpReadPortByte(COM3_PORT + 5) & 0x40) == 0) {
        __asm__ volatile ("pause");
    }
}

ULONGLONG HalReadApplicationBuffer(CHAR *Buffer, ULONGLONG Length)
{
    ULONGLONG ReadCount = 0;
    UCHAR Character;

    if (Length == 0) {
        return 0;
    }
    if (HalpApplicationInputEnded) {
        return 0;
    }

    while ((HalpReadPortByte(COM2_PORT + 5) & 0x01) == 0) {
        __asm__ volatile ("pause");
    }

    Character = HalpReadPortByte(COM2_PORT);
    if (Character == 0x04) {
        HalpApplicationInputEnded = TRUE;
        return 0;
    }
    Buffer[ReadCount++] = (CHAR)Character;

    while (ReadCount < Length &&
           (HalpReadPortByte(COM2_PORT + 5) & 0x01) != 0) {
        UCHAR Character = HalpReadPortByte(COM2_PORT);
        if (Character == 0x04) {
            HalpApplicationInputEnded = TRUE;
            break;
        }
        Buffer[ReadCount++] = (CHAR)Character;
    }

    return ReadCount;
}
