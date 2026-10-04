#include <Treas/Kernel.h>
#include <Treas/SystemCall.h>
#include <Treas/Trap.h>
#include <Treas/Thread.h>
#include <Treas/VirtualMemory.h>

VOID KiHandleException(PKI_EXCEPTION_FRAME ExceptionFrame)
{
    if ((ExceptionFrame->CodeSegment & 3) == 3) {
        if (!KeCurrentThreadIsPrimaryUser()) {
            KeTerminateCurrentUserThread();
        }
        MmSwitchToKernelAddressSpace();
        KiTerminateInitialUserProcess(127);
    }

    KiBugCheck((ULONG)(KI_BUGCHECK_CPU_EXCEPTION_BASE +
                       ExceptionFrame->Vector),
               ExceptionFrame->ErrorCode);
}
