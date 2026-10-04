#ifndef _TREAS_PHYSICAL_MEMORY_H_
#define _TREAS_PHYSICAL_MEMORY_H_

#include <Treas/Types.h>

#define MM_PAGE_SIZE 0x1000ULL
#define MM_MAX_PHYSICAL_ADDRESS (1ULL << 32)

BOOLEAN MmInitializePhysicalMemory(ULONG StartInfoAddress);
ULONGLONG MmAllocatePhysicalPage(VOID);
BOOLEAN MmFreePhysicalPage(ULONGLONG PhysicalAddress);
ULONGLONG MmGetFreePageCount(VOID);
ULONGLONG MmGetManagedPageCount(VOID);

#endif
