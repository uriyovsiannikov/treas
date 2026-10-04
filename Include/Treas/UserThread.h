#ifndef _TREAS_USER_THREAD_H_
#define _TREAS_USER_THREAD_H_

#include <Treas/VirtualMemory.h>

#define MI_USER_THREAD_STACK_PAGE_COUNT 8
#define MI_USER_THREAD_STACK_SLOT_COUNT 8
#define MI_USER_THREAD_STACK_STRIDE (16ULL * MM_PAGE_SIZE)
#define MI_USER_THREAD_STACK_RESERVED_BASE \
    (MM_USER_ADDRESS_LIMIT - \
     (MI_USER_THREAD_STACK_SLOT_COUNT - 1ULL) * MI_USER_THREAD_STACK_STRIDE - \
     MI_USER_THREAD_STACK_PAGE_COUNT * MM_PAGE_SIZE)
#define MI_USER_THREAD_EXIT_THUNK_ADDRESS 0x00007FFFFF000000ULL

BOOLEAN MiInitializeUserThreadSupport(PMM_ADDRESS_SPACE AddressSpace);
BOOLEAN MiCreateUserThreadStack(PMM_ADDRESS_SPACE AddressSpace,
                                ULONGLONG *StackPointer,
                                ULONG *StackSlot,
                                ULONGLONG *StackPages);
VOID MiDestroyUserThreadStack(PMM_ADDRESS_SPACE AddressSpace,
                              ULONG StackSlot,
                              const ULONGLONG *StackPages,
                              ULONG StackPageCount);

#endif
