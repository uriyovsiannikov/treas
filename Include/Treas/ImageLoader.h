#ifndef _TREAS_IMAGE_LOADER_H_
#define _TREAS_IMAGE_LOADER_H_

#include <Treas/Types.h>
#include <Treas/VirtualMemory.h>

BOOLEAN MiLoadInitialProcess(PMM_ADDRESS_SPACE AddressSpace,
                             const UCHAR *Image,
                             ULONGLONG ImageSize,
                             ULONGLONG *EntryPoint);

#endif
