#ifndef _TREAS_PVH_H_
#define _TREAS_PVH_H_

#include <Treas/Types.h>

#define PVH_START_INFO_MAGIC 0x336EC578u
#define PVH_START_INFO_VERSION_MEMORY_MAP 1
#define PVH_MEMORY_MAP_TYPE_RAM 1

typedef struct _PVH_START_INFO {
    ULONG Magic;
    ULONG Version;
    ULONG Flags;
    ULONG ModuleCount;
    ULONGLONG ModuleListPhysicalAddress;
    ULONGLONG CommandLinePhysicalAddress;
    ULONGLONG RsdpPhysicalAddress;
    ULONGLONG MemoryMapPhysicalAddress;
    ULONG MemoryMapEntryCount;
    ULONG Reserved;
} PVH_START_INFO, *PPVH_START_INFO;

typedef struct _PVH_MODULE_ENTRY {
    ULONGLONG PhysicalAddress;
    ULONGLONG Size;
    ULONGLONG CommandLinePhysicalAddress;
    ULONGLONG Reserved;
} PVH_MODULE_ENTRY, *PPVH_MODULE_ENTRY;

typedef struct _PVH_MEMORY_MAP_ENTRY {
    ULONGLONG BaseAddress;
    ULONGLONG Size;
    ULONG Type;
    ULONG Reserved;
} PVH_MEMORY_MAP_ENTRY, *PPVH_MEMORY_MAP_ENTRY;

#endif
