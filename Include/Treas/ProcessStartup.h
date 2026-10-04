#ifndef _TREAS_PROCESS_STARTUP_H_
#define _TREAS_PROCESS_STARTUP_H_

#include <Treas/LaunchProtocol.h>
#include <Treas/Pvh.h>
#include <Treas/VirtualMemory.h>

typedef struct _MI_LAUNCH_IMAGE {
    const UCHAR *Image;
    ULONGLONG ImageSize;
    const CHAR *Arguments;
    ULONGLONG ArgumentSize;
    ULONG ArgumentCount;
} MI_LAUNCH_IMAGE, *PMI_LAUNCH_IMAGE;

BOOLEAN MiParseLaunchImage(PPVH_MODULE_ENTRY Module,
                           PMI_LAUNCH_IMAGE LaunchImage);
BOOLEAN MiBuildInitialUserStack(PMM_ADDRESS_SPACE AddressSpace,
                                PMI_LAUNCH_IMAGE LaunchImage,
                                ULONGLONG *StackPointer,
                                ULONGLONG *ArgumentCount,
                                ULONGLONG *ArgumentVector);
PMM_ADDRESS_SPACE KiGetInitialProcessAddressSpace(VOID);

#endif
