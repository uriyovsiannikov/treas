#ifndef _TREAS_USER_API_H_
#define _TREAS_USER_API_H_

#include <Treas/Types.h>
#include <Treas/Status.h>
#include <Treas/UserAbi.h>
#include <Treas/UserFile.h>
#include <Treas/UserObject.h>
#include <Treas/UserSystemInformation.h>
#include <Treas/UserVirtualMemory.h>

LONG TreUserMain(ULONG ArgumentCount, const CHAR **Arguments);
LONG TreWriteBuffer(const VOID *Buffer, ULONGLONG Length);
LONG TreWriteErrorBuffer(const VOID *Buffer, ULONGLONG Length);
LONG TreReadBuffer(VOID *Buffer, ULONGLONG Length);
LONG TreQuerySystemInformation(PTREAS_SYSTEM_INFORMATION Information);
LONG TreCreateThread(VOID (*StartRoutine)(PVOID), PVOID Context);
PVOID TreAllocateVirtualMemory(ULONGLONG Size, ULONG Protection);
LONG TreFreeVirtualMemory(PVOID BaseAddress);
LONG TreProtectVirtualMemory(PVOID BaseAddress, ULONG Protection);
PVOID TreAllocate(ULONGLONG Size);
VOID TreFree(PVOID Buffer);
PVOID TreReallocate(PVOID Buffer, ULONGLONG Size);
VOID TreCopyMemory(PVOID Destination, const VOID *Source, ULONGLONG Length);
VOID TreZeroMemory(PVOID Destination, ULONGLONG Length);
LONG TreReadFile(ULONG Handle,
                 PVOID Buffer,
                 ULONGLONG Length,
                 ULONGLONG Offset);
LONG TreWriteFile(ULONG Handle,
                  const VOID *Buffer,
                  ULONGLONG Length,
                  ULONGLONG Offset);
LONG TreQueryFileSize(ULONG Handle, ULONGLONG *Size);
LONG TreCloseFile(ULONG Handle);
LONG TreCreateEvent(BOOLEAN ManualReset, BOOLEAN InitialState);
LONG TreSetEvent(ULONG Handle);
LONG TreResetEvent(ULONG Handle);
LONG TreWaitForEvent(ULONG Handle);
LONG TreCloseHandle(ULONG Handle);
VOID TreExitThread(VOID);
VOID TreYieldThread(VOID);
VOID TreExitProcess(ULONG ExitCode);

#endif
