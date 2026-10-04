#ifndef _TREAS_VIRTUAL_MEMORY_H_
#define _TREAS_VIRTUAL_MEMORY_H_

#include <Treas/Types.h>

#define MM_PAGE_WRITE 0x00000002ULL
#define MM_PAGE_USER 0x00000004ULL
#define MM_PAGE_NO_EXECUTE (1ULL << 63)
#define MM_USER_ADDRESS_MIN (1ULL << 32)
#define MM_USER_ADDRESS_LIMIT 0x0000800000000000ULL

typedef struct _MM_ADDRESS_SPACE {
    ULONGLONG PageTableBase;
} MM_ADDRESS_SPACE, *PMM_ADDRESS_SPACE;

BOOLEAN MmInitializeVirtualMemory(VOID);
BOOLEAN MmCreateAddressSpace(PMM_ADDRESS_SPACE AddressSpace);
VOID MmDestroyAddressSpace(PMM_ADDRESS_SPACE AddressSpace);
BOOLEAN MmSwitchAddressSpace(PMM_ADDRESS_SPACE AddressSpace);
VOID MmSwitchToKernelAddressSpace(VOID);
BOOLEAN MmMapPhysicalPage(PMM_ADDRESS_SPACE AddressSpace,
                          ULONGLONG VirtualAddress,
                          ULONGLONG PhysicalAddress,
                          ULONGLONG Protection);
BOOLEAN MmUnmapVirtualPage(PMM_ADDRESS_SPACE AddressSpace,
                           ULONGLONG VirtualAddress);
BOOLEAN MmIsUserRangeValid(ULONGLONG VirtualAddress,
                           ULONGLONG Length);
BOOLEAN MmIsUserRangeWritable(ULONGLONG VirtualAddress,
                              ULONGLONG Length);
BOOLEAN MmIsUserAddressValid(ULONGLONG VirtualAddress,
                             BOOLEAN WriteAccess,
                             BOOLEAN ExecuteAccess);

#endif
