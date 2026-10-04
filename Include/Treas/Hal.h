#ifndef _TREAS_HAL_H_
#define _TREAS_HAL_H_

#include <Treas/Types.h>

VOID HalInitializeSerial(VOID);
VOID HalWriteApplicationBuffer(const CHAR *Buffer, ULONGLONG Length);
VOID HalWriteApplicationErrorBuffer(const CHAR *Buffer, ULONGLONG Length);
VOID HalFlushApplicationStreams(VOID);
ULONGLONG HalReadApplicationBuffer(CHAR *Buffer, ULONGLONG Length);
VOID HalInitializeInterrupts(VOID);
VOID HalInitializeTimer(ULONG Frequency);
VOID HalAcknowledgeTimerInterrupt(VOID);
VOID HalInitializeSystemCalls(VOID);
VOID HalSetKernelStack(ULONGLONG StackTop);
VOID HalTerminateVirtualMachine(ULONG ExitCode);

#endif
