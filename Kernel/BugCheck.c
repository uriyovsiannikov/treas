#include <Treas/Hal.h>
#include <Treas/Kernel.h>

VOID KiBugCheck(ULONG BugCheckCode, ULONGLONG Parameter1)
{
    (void)BugCheckCode;
    (void)Parameter1;

    HalFlushApplicationStreams();
    HalTerminateVirtualMachine(0x7F);

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
