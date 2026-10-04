#include <Treas/Process.h>

static EPROCESS PspInitialProcess;

PEPROCESS PsGetInitialProcess(VOID)
{
    return &PspInitialProcess;
}

VOID PsSetInitialProcessExitStatus(ULONG ExitStatus)
{
    PspInitialProcess.ExitStatus = ExitStatus;
}

ULONG PsGetInitialProcessExitStatus(VOID)
{
    return PspInitialProcess.ExitStatus;
}
