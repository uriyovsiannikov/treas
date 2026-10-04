#ifndef _TREAS_SYSTEM_CALL_H_
#define _TREAS_SYSTEM_CALL_H_

#include <Treas/Types.h>
#include <Treas/UserAbi.h>

ULONGLONG KiSystemCallDispatch(ULONG ServiceNumber,
                               ULONG_PTR Argument1,
                               ULONGLONG Argument2);
BOOLEAN KiValidateUserReturnContext(ULONGLONG InstructionPointer,
                                   ULONGLONG StackPointer);
VOID KiTerminateInitialUserProcess(ULONG ExitCode);
VOID KiUserProcessExited(VOID);

#endif
