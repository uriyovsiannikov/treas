#ifndef _TREAS_USER_SYSTEM_INFORMATION_H_
#define _TREAS_USER_SYSTEM_INFORMATION_H_

#include <Treas/Types.h>

#define TREAS_SYSTEM_INFORMATION_VERSION 1
typedef struct _TREAS_SYSTEM_INFORMATION {
    ULONG Size;
    ULONG Version;
    ULONGLONG ManagedMemoryBytes;
    ULONGLONG FreeMemoryBytes;
    ULONGLONG TimerTicks;
    ULONG TimerFrequency;
    ULONG ProcessorCount;
} TREAS_SYSTEM_INFORMATION, *PTREAS_SYSTEM_INFORMATION;

#endif
