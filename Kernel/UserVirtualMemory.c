#include <Treas/PhysicalMemory.h>
#include <Treas/SpinLock.h>
#include <Treas/UserVirtualMemoryManager.h>

#define MI_MAXIMUM_USER_REGIONS 32
#define MI_MAXIMUM_USER_REGION_SIZE (64ULL * 1024 * 1024)

typedef struct _MI_USER_REGION {
    ULONGLONG BaseAddress;
    ULONGLONG RegionSize;
    ULONG Protection;
    BOOLEAN Active;
} MI_USER_REGION, *PMI_USER_REGION;

static MI_USER_REGION MipUserRegions[MI_MAXIMUM_USER_REGIONS];
static KSPIN_LOCK MipUserRegionLock;

static VOID MipZeroPage(ULONGLONG PhysicalAddress)
{
    ULONGLONG *Page = (ULONGLONG *)(ULONG_PTR)PhysicalAddress;
    ULONG Index;

    for (Index = 0; Index < MM_PAGE_SIZE / sizeof(ULONGLONG); Index++) {
        Page[Index] = 0;
    }
}

static BOOLEAN MipConvertProtection(ULONG UserProtection,
                                    ULONGLONG *PageProtection)
{
    ULONGLONG Protection = MM_PAGE_USER;

    if ((UserProtection & ~(TREAS_VM_PROTECT_READ |
                            TREAS_VM_PROTECT_WRITE |
                            TREAS_VM_PROTECT_EXECUTE)) != 0 ||
        (UserProtection & TREAS_VM_PROTECT_READ) == 0 ||
        (UserProtection & (TREAS_VM_PROTECT_WRITE |
                           TREAS_VM_PROTECT_EXECUTE)) ==
            (TREAS_VM_PROTECT_WRITE | TREAS_VM_PROTECT_EXECUTE)) {
        return FALSE;
    }
    if ((UserProtection & TREAS_VM_PROTECT_WRITE) != 0) {
        Protection |= MM_PAGE_WRITE;
    }
    if ((UserProtection & TREAS_VM_PROTECT_EXECUTE) == 0) {
        Protection |= MM_PAGE_NO_EXECUTE;
    }
    *PageProtection = Protection;
    return TRUE;
}

static PMI_USER_REGION MipFindRegion(ULONGLONG BaseAddress)
{
    ULONG Index;

    for (Index = 0; Index < MI_MAXIMUM_USER_REGIONS; Index++) {
        if (MipUserRegions[Index].Active &&
            MipUserRegions[Index].BaseAddress == BaseAddress) {
            return &MipUserRegions[Index];
        }
    }
    return 0;
}

static BOOLEAN MipFindFreeRegion(ULONGLONG RegionSize,
                                 PMI_USER_REGION *Region,
                                 ULONGLONG *BaseAddress)
{
    PMI_USER_REGION FreeRegion = 0;
    ULONGLONG Candidate = MI_USER_DYNAMIC_BASE;
    ULONG Index;

    for (Index = 0; Index < MI_MAXIMUM_USER_REGIONS; Index++) {
        if (!MipUserRegions[Index].Active) {
            FreeRegion = &MipUserRegions[Index];
            break;
        }
    }
    if (FreeRegion == 0) {
        return FALSE;
    }

    for (;;) {
        BOOLEAN Collision = FALSE;

        if (Candidate > MI_USER_DYNAMIC_LIMIT - RegionSize) {
            return FALSE;
        }
        for (Index = 0; Index < MI_MAXIMUM_USER_REGIONS; Index++) {
            PMI_USER_REGION Current = &MipUserRegions[Index];

            if (!Current->Active ||
                Candidate + RegionSize <= Current->BaseAddress ||
                Candidate >= Current->BaseAddress + Current->RegionSize) {
                continue;
            }
            Candidate = Current->BaseAddress + Current->RegionSize;
            Collision = TRUE;
            break;
        }
        if (!Collision) {
            *Region = FreeRegion;
            *BaseAddress = Candidate;
            return TRUE;
        }
    }
}

static BOOLEAN MipAllocateRegion(PMM_ADDRESS_SPACE AddressSpace,
                                 PTREAS_VIRTUAL_MEMORY_REQUEST Request,
                                 ULONGLONG PageProtection)
{
    PMI_USER_REGION Region;
    ULONGLONG BaseAddress;
    ULONGLONG RegionSize;
    ULONGLONG Offset;

    if (Request->BaseAddress != 0 || Request->RegionSize == 0 ||
        Request->RegionSize > MI_MAXIMUM_USER_REGION_SIZE) {
        return FALSE;
    }
    RegionSize = (Request->RegionSize + MM_PAGE_SIZE - 1) &
                 ~(MM_PAGE_SIZE - 1);
    if (!MipFindFreeRegion(RegionSize, &Region, &BaseAddress)) {
        return FALSE;
    }

    for (Offset = 0; Offset < RegionSize; Offset += MM_PAGE_SIZE) {
        ULONGLONG PhysicalAddress = MmAllocatePhysicalPage();

        if (PhysicalAddress == 0) {
            break;
        }
        MipZeroPage(PhysicalAddress);
        if (!MmMapPhysicalPage(AddressSpace, BaseAddress + Offset,
                               PhysicalAddress, PageProtection)) {
            MmFreePhysicalPage(PhysicalAddress);
            break;
        }
    }
    if (Offset != RegionSize) {
        while (Offset != 0) {
            ULONGLONG PhysicalAddress;

            Offset -= MM_PAGE_SIZE;
            if (MmUnmapVirtualPage(AddressSpace, BaseAddress + Offset,
                                   &PhysicalAddress)) {
                MmFreePhysicalPage(PhysicalAddress);
            }
        }
        return FALSE;
    }

    Region->BaseAddress = BaseAddress;
    Region->RegionSize = RegionSize;
    Region->Protection = Request->Protection;
    Region->Active = TRUE;
    Request->BaseAddress = BaseAddress;
    Request->RegionSize = RegionSize;
    return TRUE;
}

static BOOLEAN MipReleaseRegion(PMM_ADDRESS_SPACE AddressSpace,
                                PTREAS_VIRTUAL_MEMORY_REQUEST Request)
{
    PMI_USER_REGION Region = MipFindRegion(Request->BaseAddress);
    ULONGLONG Offset;

    if (Region == 0) {
        return FALSE;
    }
    for (Offset = 0; Offset < Region->RegionSize; Offset += MM_PAGE_SIZE) {
        ULONGLONG PhysicalAddress;

        if (!MmUnmapVirtualPage(AddressSpace, Region->BaseAddress + Offset,
                                &PhysicalAddress) ||
            !MmFreePhysicalPage(PhysicalAddress)) {
            return FALSE;
        }
    }
    Request->RegionSize = Region->RegionSize;
    Region->Active = FALSE;
    return TRUE;
}

static BOOLEAN MipProtectRegion(PMM_ADDRESS_SPACE AddressSpace,
                                PTREAS_VIRTUAL_MEMORY_REQUEST Request,
                                ULONGLONG PageProtection)
{
    PMI_USER_REGION Region = MipFindRegion(Request->BaseAddress);
    ULONGLONG Offset;

    if (Region == 0) {
        return FALSE;
    }
    for (Offset = 0; Offset < Region->RegionSize; Offset += MM_PAGE_SIZE) {
        if (!MmProtectVirtualPage(AddressSpace,
                                  Region->BaseAddress + Offset,
                                  PageProtection)) {
            return FALSE;
        }
    }
    Region->Protection = Request->Protection;
    Request->RegionSize = Region->RegionSize;
    return TRUE;
}

BOOLEAN MiManageUserVirtualMemory(PMM_ADDRESS_SPACE AddressSpace,
                                  PTREAS_VIRTUAL_MEMORY_REQUEST Request)
{
    ULONGLONG PageProtection;
    BOOLEAN RestoreInterrupts;
    BOOLEAN Result;

    if (AddressSpace == 0 || Request == 0) {
        return FALSE;
    }

    PageProtection = 0;
    if (Request->Operation != TREAS_VM_OPERATION_RELEASE &&
        !MipConvertProtection(Request->Protection, &PageProtection)) {
        return FALSE;
    }

    RestoreInterrupts = KeAcquireSpinLock(&MipUserRegionLock);
    if (Request->Operation == TREAS_VM_OPERATION_ALLOCATE) {
        Result = MipAllocateRegion(AddressSpace, Request, PageProtection);
    } else if (Request->Operation == TREAS_VM_OPERATION_RELEASE) {
        Result = MipReleaseRegion(AddressSpace, Request);
    } else if (Request->Operation == TREAS_VM_OPERATION_PROTECT) {
        Result = MipProtectRegion(AddressSpace, Request, PageProtection);
    } else {
        Result = FALSE;
    }
    KeReleaseSpinLock(&MipUserRegionLock, RestoreInterrupts);
    return Result;
}
