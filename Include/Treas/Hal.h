#ifndef _TREAS_HAL_H_
#define _TREAS_HAL_H_

#include <Treas/Types.h>
#include <Treas/UserFile.h>

BOOLEAN HalInitializeApplicationIo(VOID);
VOID HalWriteApplicationBuffer(const CHAR *Buffer, ULONGLONG Length);
VOID HalWriteApplicationErrorBuffer(const CHAR *Buffer, ULONGLONG Length);
VOID HalFlushApplicationStreams(VOID);
ULONGLONG HalReadApplicationBuffer(CHAR *Buffer, ULONGLONG Length);
VOID HalExecuteHostFileRequest(PTREAS_FILE_REQUEST Request);
VOID HalInitializeInterrupts(VOID);
VOID HalInitializeTimer(ULONG Frequency);
VOID HalAcquireTimer(VOID);
VOID HalReleaseTimer(VOID);
VOID HalAcknowledgeTimerInterrupt(VOID);
ULONGLONG HalQueryPerformanceCounter(VOID);
ULONGLONG HalQueryPerformanceFrequency(VOID);
VOID HalInitializeSystemCalls(VOID);
VOID HalSetKernelStack(ULONGLONG StackTop);
VOID HalTerminateVirtualMachine(ULONG ExitCode);

#endif
