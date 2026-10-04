#include <Treas/Hal.h>

VOID HalTerminateVirtualMachine(ULONG ExitCode)
{
    ULONG DebugExitValue;

    DebugExitValue = ExitCode;
    __asm__ volatile ("outl %0, $0xF4" : : "a"(DebugExitValue));

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
