#ifndef _TREAS_PROCESS_H_
#define _TREAS_PROCESS_H_

#include <Treas/VirtualMemory.h>

typedef struct _EPROCESS {
    MM_ADDRESS_SPACE AddressSpace;
    ULONG ExitStatus;
} EPROCESS, *PEPROCESS;

PEPROCESS PsGetInitialProcess(VOID);
VOID PsSetInitialProcessExitStatus(ULONG ExitStatus);
ULONG PsGetInitialProcessExitStatus(VOID);

#endif
