#ifndef _TREAS_HOST_VIRTUAL_MACHINE_H_
#define _TREAS_HOST_VIRTUAL_MACHINE_H_

#include <Treas/Types.h>
#include "FilePortal.h"

int TreaspRunVirtualMachine(const char *KernelPath,
                            const char *LaunchPath,
                            ULONG FileCount,
                            const TREASP_FILE_ARGUMENT *Files);

#endif
