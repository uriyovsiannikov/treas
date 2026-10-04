#include <Treas/UserApi.h>

LONG TreUserMain(ULONG ArgumentCount, const CHAR **Arguments)
{
    UCHAR *Buffer;
    UCHAR *LargerBuffer;
    UCHAR *VirtualBuffer;
    ULONG Index;

    (void)ArgumentCount;
    (void)Arguments;

    Buffer = TreAllocate(6000);
    if (Buffer == 0) {
        return 10;
    }
    for (Index = 0; Index < 6000; Index++) {
        if (Buffer[Index] != 0) {
            return 11;
        }
        Buffer[Index] = (UCHAR)(Index & 0xFF);
    }

    LargerBuffer = TreReallocate(Buffer, 12000);
    if (LargerBuffer == 0) {
        return 12;
    }
    for (Index = 0; Index < 6000; Index++) {
        if (LargerBuffer[Index] != (UCHAR)(Index & 0xFF)) {
            return 13;
        }
    }
    TreFree(LargerBuffer);

    VirtualBuffer = TreAllocateVirtualMemory(
        4096,
        TREAS_VM_PROTECT_READ | TREAS_VM_PROTECT_WRITE);
    if (VirtualBuffer == 0 || VirtualBuffer[0] != 0) {
        return 14;
    }
    VirtualBuffer[0] = 0x5A;
    if (TreProtectVirtualMemory(VirtualBuffer, TREAS_VM_PROTECT_READ) !=
            TREAS_STATUS_SUCCESS ||
        TreProtectVirtualMemory(VirtualBuffer,
                                TREAS_VM_PROTECT_READ |
                                TREAS_VM_PROTECT_WRITE |
                                TREAS_VM_PROTECT_EXECUTE) ==
            TREAS_STATUS_SUCCESS ||
        TreProtectVirtualMemory(VirtualBuffer,
                                TREAS_VM_PROTECT_READ |
                                TREAS_VM_PROTECT_WRITE) !=
            TREAS_STATUS_SUCCESS ||
        TreFreeVirtualMemory(VirtualBuffer) != TREAS_STATUS_SUCCESS) {
        return 15;
    }
    if (TreWriteBuffer("memory-ok\n", 10) < 0) {
        return 16;
    }
    return 0;
}
