#ifndef _TREAS_USER_VIRTUAL_MEMORY_MANAGER_H_
#define _TREAS_USER_VIRTUAL_MEMORY_MANAGER_H_

#include <Treas/UserVirtualMemory.h>
#include <Treas/VirtualMemory.h>

#define MI_USER_DYNAMIC_BASE 0x0000600000000000ULL
#define MI_USER_DYNAMIC_LIMIT 0x0000600040000000ULL

BOOLEAN MiManageUserVirtualMemory(PMM_ADDRESS_SPACE AddressSpace,
                                  PTREAS_VIRTUAL_MEMORY_REQUEST Request);

#endif
