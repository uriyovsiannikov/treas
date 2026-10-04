#include <Treas/Hal.h>
#include <Treas/Process.h>
#include <Treas/SystemCall.h>
#include <Treas/SystemService.h>
#include <Treas/Thread.h>
#include <Treas/UserAbi.h>
#include <Treas/VirtualMemory.h>

static PKI_SYSTEM_SERVICE_ROUTINE const KipSystemServiceTable[] = {
    0,
    KipWriteBufferService,
    KipExitProcessService,
    KipYieldThreadService,
    KipCreateThreadService,
    KipExitThreadService,
    KipQuerySystemInformationService,
    KipReadBufferService,
    KipWriteErrorBufferService,
    KipManageVirtualMemoryService,
    KipHostFileService,
    KipManageObjectService
};

ULONGLONG KiSystemCallDispatch(ULONG ServiceNumber,
                               ULONG_PTR Argument1,
                               ULONGLONG Argument2)
{
    if (ServiceNumber >=
            sizeof(KipSystemServiceTable) / sizeof(KipSystemServiceTable[0]) ||
        KipSystemServiceTable[ServiceNumber] == 0) {
        return KI_SYSTEM_CALL_INVALID_RESULT;
    }
    return KipSystemServiceTable[ServiceNumber](Argument1, Argument2);
}

BOOLEAN KiValidateUserReturnContext(ULONGLONG InstructionPointer,
                                    ULONGLONG StackPointer)
{
    if (StackPointer <= MM_USER_ADDRESS_MIN ||
        StackPointer >= MM_USER_ADDRESS_LIMIT ||
        !MmIsUserAddressValid(InstructionPointer, FALSE, TRUE) ||
        !MmIsUserAddressValid(StackPointer - 1, TRUE, FALSE)) {
        return FALSE;
    }
    return TRUE;
}

VOID KiTerminateInitialUserProcess(ULONG ExitCode)
{
    PsSetInitialProcessExitStatus(ExitCode > 127 ? 127 : ExitCode);
    HalFlushApplicationStreams();
    HalTerminateVirtualMachine(PsGetInitialProcessExitStatus());
}

VOID KiUserProcessExited(VOID)
{
    KiTerminateInitialUserProcess(PsGetInitialProcessExitStatus());
}
