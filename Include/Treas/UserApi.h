#ifndef _TREAS_USER_API_H_
#define _TREAS_USER_API_H_

#include <Treas/Types.h>
#include <Treas/UserAbi.h>
#include <Treas/UserSystemInformation.h>

#define TREAS_STATUS_SUCCESS 0
#define TREAS_STATUS_UNSUCCESSFUL (-2)

LONG TreUserMain(ULONG ArgumentCount, const CHAR **Arguments);
LONG TreWriteBuffer(const VOID *Buffer, ULONGLONG Length);
LONG TreWriteErrorBuffer(const VOID *Buffer, ULONGLONG Length);
LONG TreReadBuffer(VOID *Buffer, ULONGLONG Length);
LONG TreQuerySystemInformation(PTREAS_SYSTEM_INFORMATION Information);
LONG TreCreateThread(VOID (*StartRoutine)(PVOID), PVOID Context);
VOID TreExitThread(VOID);
VOID TreYieldThread(VOID);
VOID TreExitProcess(ULONG ExitCode);

#endif
