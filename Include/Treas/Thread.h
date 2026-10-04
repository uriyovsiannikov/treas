#ifndef _TREAS_THREAD_H_
#define _TREAS_THREAD_H_

#include <Treas/Types.h>
#include <Treas/VirtualMemory.h>

typedef VOID (*PKI_THREAD_START_ROUTINE)(PVOID Context);

BOOLEAN KeInitializeThreadScheduler(VOID);
BOOLEAN KeCreateThread(PKI_THREAD_START_ROUTINE StartRoutine, PVOID Context);
BOOLEAN KeCreateUserThread(PMM_ADDRESS_SPACE AddressSpace,
                           ULONGLONG EntryPoint,
                           ULONGLONG Argument,
                           ULONGLONG UserStackPointer,
                           ULONG UserStackSlot,
                           const ULONGLONG *UserStackPages,
                           ULONG *ThreadId);
VOID KeSetCurrentThreadAsPrimaryUser(VOID);
BOOLEAN KeCurrentThreadIsPrimaryUser(VOID);
VOID KeTerminateCurrentUserThread(VOID);
VOID KeYieldThread(VOID);
VOID KeExitThread(VOID);

#endif
