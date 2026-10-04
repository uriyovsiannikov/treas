#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "GuestMemory.h"

#define TREASP_MEBIBYTE (1024ULL * 1024ULL)
#define TREASP_MINIMUM_GUEST_MEMORY_MIB 64ULL
#define TREASP_MAXIMUM_GUEST_MEMORY_MIB 3072ULL
#define TREASP_DEFAULT_GUEST_MEMORY_MIB 256ULL

static ULONGLONG TreaspReadUnsignedFile(const char *Path)
{
    FILE *File;
    char Buffer[64];
    char *End;
    unsigned long long Value;

    File = fopen(Path, "r");
    if (File == NULL) {
        return 0;
    }
    if (fgets(Buffer, sizeof(Buffer), File) == NULL) {
        fclose(File);
        return 0;
    }
    fclose(File);

    errno = 0;
    Value = strtoull(Buffer, &End, 10);
    if (errno != 0 || End == Buffer || (*End != '\0' && *End != '\n') ||
        Value > (1ULL << 60)) {
        return 0;
    }
    return (ULONGLONG)Value;
}

static ULONGLONG TreaspGetCgroupMemoryAvailable(VOID)
{
    ULONGLONG Limit;
    ULONGLONG Current;

    Limit = TreaspReadUnsignedFile("/sys/fs/cgroup/memory.max");
    Current = TreaspReadUnsignedFile("/sys/fs/cgroup/memory.current");
    if (Limit != 0 && Current < Limit) {
        return Limit - Current;
    }

    Limit = TreaspReadUnsignedFile("/sys/fs/cgroup/memory/memory.limit_in_bytes");
    Current = TreaspReadUnsignedFile("/sys/fs/cgroup/memory/memory.usage_in_bytes");
    if (Limit != 0 && Current < Limit) {
        return Limit - Current;
    }
    return 0;
}

ULONGLONG TreaspGetGuestMemoryMegabytes(VOID)
{
    long PageSize;
    long AvailablePages;
    long TotalPages;
    ULONGLONG AvailableBytes;
    ULONGLONG CgroupAvailableBytes;
    ULONGLONG MemoryMegabytes;

    PageSize = sysconf(_SC_PAGESIZE);
    if (PageSize <= 0) {
        PageSize = 4096;
    }
    AvailablePages = sysconf(_SC_AVPHYS_PAGES);
    TotalPages = sysconf(_SC_PHYS_PAGES);
    if (AvailablePages > 0) {
        AvailableBytes = (ULONGLONG)AvailablePages * (ULONGLONG)PageSize;
    } else if (TotalPages > 0) {
        AvailableBytes = (ULONGLONG)TotalPages * (ULONGLONG)PageSize / 2;
    } else {
        return TREASP_DEFAULT_GUEST_MEMORY_MIB;
    }

    CgroupAvailableBytes = TreaspGetCgroupMemoryAvailable();
    if (CgroupAvailableBytes != 0 && CgroupAvailableBytes < AvailableBytes) {
        AvailableBytes = CgroupAvailableBytes;
    }

    MemoryMegabytes = (AvailableBytes / 4) / TREASP_MEBIBYTE;
    if (MemoryMegabytes < TREASP_MINIMUM_GUEST_MEMORY_MIB) {
        MemoryMegabytes = TREASP_MINIMUM_GUEST_MEMORY_MIB;
    }
    if (MemoryMegabytes > TREASP_MAXIMUM_GUEST_MEMORY_MIB) {
        MemoryMegabytes = TREASP_MAXIMUM_GUEST_MEMORY_MIB;
    }
    return MemoryMegabytes;
}
