#include <Treas/PhysicalMemory.h>
#include <Treas/Pvh.h>
#include <Treas/SpinLock.h>

#define MM_FRAME_COUNT (MM_MAX_PHYSICAL_ADDRESS / MM_PAGE_SIZE)
#define MM_BITMAP_WORD_COUNT (MM_FRAME_COUNT / 64)
#define MM_LOW_RESERVED_LIMIT 0x100000ULL
#define MM_MAX_MEMORY_MAP_ENTRIES 256
#define MM_MAX_MODULE_COUNT 32
#define MM_MAX_ALLOCATABLE_RANGES \
    (MM_MAX_MEMORY_MAP_ENTRIES + MM_MAX_MODULE_COUNT + 4)

typedef struct _MM_FRAME_RANGE {
    ULONG FirstFrame;
    ULONG PastLastFrame;
} MM_FRAME_RANGE, *PMM_FRAME_RANGE;

extern UCHAR KiKernelImageEnd;

static ULONGLONG MmpAllocatedBitmap[MM_BITMAP_WORD_COUNT];
static MM_FRAME_RANGE MmpAllocatableRanges[MM_MAX_ALLOCATABLE_RANGES];
static ULONGLONG MmpFreePageCount;
static ULONGLONG MmpManagedPageCount;
static ULONG MmpSearchHint;
static ULONG MmpAllocatableRangeCount;
static BOOLEAN MmpInitialized;
static KSPIN_LOCK MmpAllocatorLock;

static VOID MmpSetBit(ULONGLONG *Bitmap, ULONG FrameIndex)
{
    Bitmap[FrameIndex / 64] |= 1ULL << (FrameIndex % 64);
}

static VOID MmpClearBit(ULONGLONG *Bitmap, ULONG FrameIndex)
{
    Bitmap[FrameIndex / 64] &= ~(1ULL << (FrameIndex % 64));
}

static BOOLEAN MmpIsBitSet(const ULONGLONG *Bitmap, ULONG FrameIndex)
{
    return (Bitmap[FrameIndex / 64] & (1ULL << (FrameIndex % 64))) != 0;
}

static VOID MmpReserveRange(ULONGLONG BaseAddress, ULONGLONG Size)
{
    ULONGLONG EndAddress;
    ULONGLONG FirstFrame;
    ULONGLONG LastFrame;
    ULONGLONG FrameIndex;

    if (Size == 0 || BaseAddress >= MM_MAX_PHYSICAL_ADDRESS ||
        BaseAddress > (~0ULL - Size)) {
        return;
    }

    EndAddress = BaseAddress + Size;
    if (EndAddress > MM_MAX_PHYSICAL_ADDRESS) {
        EndAddress = MM_MAX_PHYSICAL_ADDRESS;
    }

    FirstFrame = BaseAddress / MM_PAGE_SIZE;
    LastFrame = (EndAddress + MM_PAGE_SIZE - 1) / MM_PAGE_SIZE;

    for (FrameIndex = FirstFrame; FrameIndex < LastFrame; FrameIndex++) {
        ULONG Index = (ULONG)FrameIndex;

        if (!MmpIsBitSet(MmpAllocatedBitmap, Index)) {
            MmpSetBit(MmpAllocatedBitmap, Index);
            MmpFreePageCount--;
        }
    }
}

static VOID MmpMakeRamRangeAvailable(ULONGLONG BaseAddress, ULONGLONG Size)
{
    ULONGLONG EndAddress;
    ULONGLONG KernelEndAddress;
    ULONGLONG FirstFrame;
    ULONGLONG LastFrame;
    ULONGLONG FrameIndex;

    if (Size == 0 || BaseAddress > (~0ULL - Size)) {
        return;
    }

    EndAddress = BaseAddress + Size;
    KernelEndAddress = ((ULONGLONG)(ULONG_PTR)&KiKernelImageEnd + MM_PAGE_SIZE - 1) &
                       ~(MM_PAGE_SIZE - 1);

    if (BaseAddress < MM_LOW_RESERVED_LIMIT) {
        BaseAddress = MM_LOW_RESERVED_LIMIT;
    }
    if (BaseAddress < KernelEndAddress) {
        BaseAddress = KernelEndAddress;
    }
    if (EndAddress > MM_MAX_PHYSICAL_ADDRESS) {
        EndAddress = MM_MAX_PHYSICAL_ADDRESS;
    }
    if (BaseAddress >= EndAddress) {
        return;
    }

    FirstFrame = (BaseAddress + MM_PAGE_SIZE - 1) / MM_PAGE_SIZE;
    LastFrame = EndAddress / MM_PAGE_SIZE;

    for (FrameIndex = FirstFrame; FrameIndex < LastFrame; FrameIndex++) {
        ULONG Index = (ULONG)FrameIndex;

        if (MmpIsBitSet(MmpAllocatedBitmap, Index)) {
            MmpClearBit(MmpAllocatedBitmap, Index);
            MmpFreePageCount++;
            MmpManagedPageCount++;
        }
    }
}

static BOOLEAN MmpBuildAllocatableRanges(VOID)
{
    ULONG FrameIndex = 0;

    while (FrameIndex < MM_FRAME_COUNT) {
        ULONG FirstFrame;

        while (FrameIndex < MM_FRAME_COUNT &&
               MmpIsBitSet(MmpAllocatedBitmap, FrameIndex)) {
            FrameIndex++;
        }
        if (FrameIndex == MM_FRAME_COUNT) {
            break;
        }

        FirstFrame = FrameIndex;
        while (FrameIndex < MM_FRAME_COUNT &&
               !MmpIsBitSet(MmpAllocatedBitmap, FrameIndex)) {
            FrameIndex++;
        }
        if (MmpAllocatableRangeCount == MM_MAX_ALLOCATABLE_RANGES) {
            return FALSE;
        }
        MmpAllocatableRanges[MmpAllocatableRangeCount].FirstFrame = FirstFrame;
        MmpAllocatableRanges[MmpAllocatableRangeCount].PastLastFrame = FrameIndex;
        MmpAllocatableRangeCount++;
    }

    return MmpAllocatableRangeCount != 0;
}

static BOOLEAN MmpIsFrameAllocatable(ULONG FrameIndex)
{
    ULONG FirstRange = 0;
    ULONG PastLastRange = MmpAllocatableRangeCount;

    while (FirstRange < PastLastRange) {
        ULONG RangeIndex = FirstRange + (PastLastRange - FirstRange) / 2;
        PMM_FRAME_RANGE Range = &MmpAllocatableRanges[RangeIndex];

        if (FrameIndex < Range->FirstFrame) {
            PastLastRange = RangeIndex;
        } else if (FrameIndex >= Range->PastLastFrame) {
            FirstRange = RangeIndex + 1;
        } else {
            return TRUE;
        }
    }

    return FALSE;
}

BOOLEAN MmInitializePhysicalMemory(ULONG StartInfoAddress)
{
    PPVH_START_INFO StartInfo;
    PPVH_MEMORY_MAP_ENTRY MemoryMap;
    PPVH_MODULE_ENTRY Modules;
    ULONGLONG MemoryMapSize;
    ULONG Index;

    KeInitializeSpinLock(&MmpAllocatorLock);

    if (StartInfoAddress == 0) {
        return FALSE;
    }

    StartInfo = (PPVH_START_INFO)(ULONG_PTR)StartInfoAddress;
    if (StartInfo->Magic != PVH_START_INFO_MAGIC ||
        StartInfo->Version < PVH_START_INFO_VERSION_MEMORY_MAP ||
        StartInfo->MemoryMapEntryCount == 0 ||
        StartInfo->MemoryMapEntryCount > MM_MAX_MEMORY_MAP_ENTRIES ||
        StartInfo->MemoryMapPhysicalAddress == 0) {
        return FALSE;
    }

    MemoryMapSize = (ULONGLONG)StartInfo->MemoryMapEntryCount *
                    sizeof(PVH_MEMORY_MAP_ENTRY);
    if (StartInfo->MemoryMapPhysicalAddress >= MM_MAX_PHYSICAL_ADDRESS ||
        MemoryMapSize > MM_MAX_PHYSICAL_ADDRESS - StartInfo->MemoryMapPhysicalAddress) {
        return FALSE;
    }

    MemoryMap = (PPVH_MEMORY_MAP_ENTRY)(ULONG_PTR)StartInfo->MemoryMapPhysicalAddress;
    /* The PVH ELF loader clears BSS, so only the nonzero initial bitmap is written. */
    for (Index = 0; Index < MM_BITMAP_WORD_COUNT; Index++) {
        MmpAllocatedBitmap[Index] = ~0ULL;
    }

    for (Index = 0; Index < StartInfo->MemoryMapEntryCount; Index++) {
        if (MemoryMap[Index].Type == PVH_MEMORY_MAP_TYPE_RAM) {
            MmpMakeRamRangeAvailable(MemoryMap[Index].BaseAddress,
                                     MemoryMap[Index].Size);
        }
    }

    MmpReserveRange(StartInfoAddress, sizeof(PVH_START_INFO));
    MmpReserveRange(StartInfo->MemoryMapPhysicalAddress, MemoryMapSize);

    if (StartInfo->CommandLinePhysicalAddress != 0) {
        MmpReserveRange(StartInfo->CommandLinePhysicalAddress, 1);
    }

    if (StartInfo->ModuleCount > MM_MAX_MODULE_COUNT ||
        (StartInfo->ModuleCount != 0 && StartInfo->ModuleListPhysicalAddress == 0)) {
        return FALSE;
    }

    if (StartInfo->ModuleCount != 0) {
        ULONGLONG ModuleListSize = (ULONGLONG)StartInfo->ModuleCount * sizeof(PVH_MODULE_ENTRY);

        if (StartInfo->ModuleListPhysicalAddress >= MM_MAX_PHYSICAL_ADDRESS ||
            ModuleListSize > MM_MAX_PHYSICAL_ADDRESS - StartInfo->ModuleListPhysicalAddress) {
            return FALSE;
        }

        Modules = (PPVH_MODULE_ENTRY)(ULONG_PTR)StartInfo->ModuleListPhysicalAddress;
        MmpReserveRange(StartInfo->ModuleListPhysicalAddress, ModuleListSize);

        for (Index = 0; Index < StartInfo->ModuleCount; Index++) {
            MmpReserveRange(Modules[Index].PhysicalAddress, Modules[Index].Size);
        }
    }

    if (!MmpBuildAllocatableRanges()) {
        return FALSE;
    }
    MmpInitialized = TRUE;
    return MmpFreePageCount != 0;
}

ULONGLONG MmAllocatePhysicalPage(VOID)
{
    ULONG StartWord;
    ULONG WordOffset;
    ULONGLONG PhysicalAddress = 0;
    BOOLEAN RestoreInterrupts = KeAcquireSpinLock(&MmpAllocatorLock);

    if (!MmpInitialized || MmpFreePageCount == 0) {
        goto Exit;
    }

    StartWord = MmpSearchHint / 64;
    for (WordOffset = 0; WordOffset < MM_BITMAP_WORD_COUNT; WordOffset++) {
        ULONG WordIndex = (StartWord + WordOffset) % MM_BITMAP_WORD_COUNT;
        ULONGLONG FreeBits = ~MmpAllocatedBitmap[WordIndex];

        if (FreeBits != 0) {
            ULONG BitIndex = (ULONG)__builtin_ctzll(FreeBits);
            ULONG FrameIndex = WordIndex * 64 + BitIndex;
            ULONGLONG FrameMask = 1ULL << BitIndex;

            MmpAllocatedBitmap[WordIndex] |= FrameMask;
            MmpFreePageCount--;
            MmpSearchHint = (FrameIndex + 1) % MM_FRAME_COUNT;
            PhysicalAddress = (ULONGLONG)FrameIndex * MM_PAGE_SIZE;
            goto Exit;
        }
    }

Exit:
    KeReleaseSpinLock(&MmpAllocatorLock, RestoreInterrupts);
    return PhysicalAddress;
}

BOOLEAN MmFreePhysicalPage(ULONGLONG PhysicalAddress)
{
    ULONG FrameIndex;
    BOOLEAN Result = FALSE;
    BOOLEAN RestoreInterrupts = KeAcquireSpinLock(&MmpAllocatorLock);

    if (!MmpInitialized || PhysicalAddress >= MM_MAX_PHYSICAL_ADDRESS ||
        (PhysicalAddress & (MM_PAGE_SIZE - 1)) != 0) {
        goto Exit;
    }

    FrameIndex = (ULONG)(PhysicalAddress / MM_PAGE_SIZE);
    if (!MmpIsFrameAllocatable(FrameIndex) ||
        !MmpIsBitSet(MmpAllocatedBitmap, FrameIndex)) {
        goto Exit;
    }

    MmpClearBit(MmpAllocatedBitmap, FrameIndex);
    MmpFreePageCount++;
    if (FrameIndex < MmpSearchHint) {
        MmpSearchHint = FrameIndex;
    }

    Result = TRUE;

Exit:
    KeReleaseSpinLock(&MmpAllocatorLock, RestoreInterrupts);
    return Result;
}

ULONGLONG MmGetFreePageCount(VOID)
{
#ifdef TREAS_MULTIPROCESSOR
    ULONGLONG PageCount;
    BOOLEAN RestoreInterrupts = KeAcquireSpinLock(&MmpAllocatorLock);

    PageCount = MmpFreePageCount;
    KeReleaseSpinLock(&MmpAllocatorLock, RestoreInterrupts);
    return PageCount;
#else
    return MmpFreePageCount;
#endif
}

ULONGLONG MmGetManagedPageCount(VOID)
{
#ifdef TREAS_MULTIPROCESSOR
    ULONGLONG PageCount;
    BOOLEAN RestoreInterrupts = KeAcquireSpinLock(&MmpAllocatorLock);

    PageCount = MmpManagedPageCount;
    KeReleaseSpinLock(&MmpAllocatorLock, RestoreInterrupts);
    return PageCount;
#else
    return MmpManagedPageCount;
#endif
}
