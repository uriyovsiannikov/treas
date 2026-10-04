#include <Treas/PhysicalMemory.h>
#include <Treas/SpinLock.h>
#include <Treas/VirtualMemory.h>

#define MM_PAGE_PRESENT 0x00000001ULL
#define MM_PAGE_LARGE 0x00000080ULL
#define MM_PAGE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL
#define MM_LARGE_PAGE_ADDRESS_MASK 0x000FFFFFFFE00000ULL
#define MM_PAGE_FRAME_MASK 0x000FFFFFFFFFFFFFULL
#define MM_PAGE_TABLE_ENTRIES 512
#define MM_KERNEL_IDENTITY_DIRECTORY_COUNT 4

static BOOLEAN MmpNoExecuteEnabled;
static ULONGLONG MmpKernelPageTableBase;
static KSPIN_LOCK MmpPageTableLock;

static VOID MmpZeroPage(ULONGLONG PhysicalAddress)
{
    ULONGLONG *Page;
    ULONG Index;

    Page = (ULONGLONG *)(ULONG_PTR)PhysicalAddress;
    for (Index = 0; Index < MM_PAGE_TABLE_ENTRIES; Index++) {
        Page[Index] = 0;
    }
}

static ULONGLONG MmpReadCr3(VOID)
{
    ULONGLONG Value;

    __asm__ volatile ("mov %%cr3, %0" : "=r"(Value));
    return Value & MM_PAGE_ADDRESS_MASK;
}

static BOOLEAN MmpEnableNoExecute(VOID)
{
    ULONG MaximumExtendedFunction;
    ULONG FeatureFlags;
    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;
    ULONG EferLow;
    ULONG EferHigh;

    Eax = 0x80000000;
    __asm__ volatile ("cpuid"
                      : "=a"(MaximumExtendedFunction), "=b"(Ebx), "=c"(Ecx), "=d"(Edx)
                      : "a"(Eax), "c"(0));
    if (MaximumExtendedFunction < 0x80000001) {
        return FALSE;
    }

    Eax = 0x80000001;
    __asm__ volatile ("cpuid"
                      : "=a"(Eax), "=b"(Ebx), "=c"(Ecx), "=d"(FeatureFlags)
                      : "a"(Eax), "c"(0));
    if ((FeatureFlags & (1UL << 20)) == 0) {
        return FALSE;
    }

    Eax = 0xC0000080;
    __asm__ volatile ("rdmsr" : "=a"(EferLow), "=d"(EferHigh) : "c"(Eax));
    EferLow |= 1UL << 11;
    __asm__ volatile ("wrmsr" : : "a"(EferLow), "d"(EferHigh), "c"(Eax));
    return TRUE;
}

BOOLEAN MmInitializeVirtualMemory(VOID)
{
    ULONGLONG Cr0;

    KeInitializeSpinLock(&MmpPageTableLock);
    MmpKernelPageTableBase = MmpReadCr3();
    MmpNoExecuteEnabled = MmpEnableNoExecute();

    __asm__ volatile ("mov %%cr0, %0" : "=r"(Cr0));
    Cr0 |= 1ULL << 16;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(Cr0) : "memory");

    return MmpKernelPageTableBase != 0;
}

static ULONGLONG *MmpGetOrCreateTable(ULONGLONG *Entry, BOOLEAN UserAccess)
{
    ULONGLONG TableAddress;

    if ((*Entry & MM_PAGE_PRESENT) == 0) {
        TableAddress = MmAllocatePhysicalPage();
        if (TableAddress == 0) {
            return 0;
        }

        MmpZeroPage(TableAddress);
        *Entry = TableAddress | MM_PAGE_PRESENT | MM_PAGE_WRITE;
    }

    if (UserAccess) {
        *Entry |= MM_PAGE_USER;
    }

    return (ULONGLONG *)(ULONG_PTR)(*Entry & MM_PAGE_ADDRESS_MASK);
}

static BOOLEAN MmpSplitLargePage(ULONGLONG *Entry)
{
    ULONGLONG OldEntry;
    ULONGLONG TableAddress;
    ULONGLONG PhysicalBase;
    ULONGLONG LeafProtection;
    ULONGLONG TableProtection;
    ULONGLONG *PageTable;
    ULONG Index;

    OldEntry = *Entry;
    if ((OldEntry & (MM_PAGE_PRESENT | MM_PAGE_LARGE)) !=
        (MM_PAGE_PRESENT | MM_PAGE_LARGE)) {
        return FALSE;
    }

    TableAddress = MmAllocatePhysicalPage();
    if (TableAddress == 0) {
        return FALSE;
    }

    PhysicalBase = OldEntry & MM_LARGE_PAGE_ADDRESS_MASK;
    LeafProtection = OldEntry & 0x1FFFULL;
    LeafProtection &= ~(MM_PAGE_LARGE | (1ULL << 12));

    /* A large-page PAT bit moves from bit 12 to the 4 KiB PTE bit 7. */
    if ((OldEntry & (1ULL << 12)) != 0) {
        LeafProtection |= MM_PAGE_LARGE;
    }
    if ((OldEntry & MM_PAGE_NO_EXECUTE) != 0 && MmpNoExecuteEnabled) {
        LeafProtection |= MM_PAGE_NO_EXECUTE;
    }

    PageTable = (ULONGLONG *)(ULONG_PTR)TableAddress;
    for (Index = 0; Index < MM_PAGE_TABLE_ENTRIES; Index++) {
        PageTable[Index] = (PhysicalBase + (ULONGLONG)Index * MM_PAGE_SIZE) |
                           LeafProtection;
    }

    TableProtection = MM_PAGE_PRESENT | MM_PAGE_WRITE;
    TableProtection |= OldEntry & MM_PAGE_USER;
    *Entry = TableAddress | TableProtection;
    return TRUE;
}

static BOOLEAN MmpIsCanonicalAddress(ULONGLONG VirtualAddress)
{
    ULONGLONG UpperBits;
    BOOLEAN SignBit;

    UpperBits = VirtualAddress >> 48;
    SignBit = (BOOLEAN)((VirtualAddress >> 47) & 1);
    return SignBit ? UpperBits == 0xFFFF : UpperBits == 0;
}

static BOOLEAN MmpIsUserAddressValidLocked(ULONGLONG VirtualAddress,
                                            BOOLEAN WriteAccess,
                                            BOOLEAN ExecuteAccess);

static ULONGLONG *MmpGetPte(ULONGLONG PageTableBase,
                            ULONGLONG VirtualAddress,
                            BOOLEAN CreateTables,
                            BOOLEAN UserAccess)
{
    ULONGLONG *PageMapLevel4;
    ULONGLONG *PageDirectoryPointerTable;
    ULONGLONG *PageDirectory;
    ULONGLONG *PageTable;
    ULONGLONG *Entry;
    ULONG Index;

    if (!MmpIsCanonicalAddress(VirtualAddress) || PageTableBase == 0) {
        return 0;
    }

    PageMapLevel4 = (ULONGLONG *)(ULONG_PTR)PageTableBase;
    Index = (ULONG)((VirtualAddress >> 39) & 0x1FF);
    if ((PageMapLevel4[Index] & MM_PAGE_PRESENT) == 0 && !CreateTables) {
        return 0;
    }
    PageDirectoryPointerTable = MmpGetOrCreateTable(&PageMapLevel4[Index], UserAccess);
    if (PageDirectoryPointerTable == 0) {
        return 0;
    }

    Index = (ULONG)((VirtualAddress >> 30) & 0x1FF);
    Entry = &PageDirectoryPointerTable[Index];
    if ((*Entry & MM_PAGE_PRESENT) == 0 && !CreateTables) {
        return 0;
    }
    if ((*Entry & (MM_PAGE_PRESENT | MM_PAGE_LARGE)) ==
        (MM_PAGE_PRESENT | MM_PAGE_LARGE)) {
        return 0;
    }
    PageDirectory = MmpGetOrCreateTable(Entry, UserAccess);
    if (PageDirectory == 0) {
        return 0;
    }

    Index = (ULONG)((VirtualAddress >> 21) & 0x1FF);
    Entry = &PageDirectory[Index];
    if ((*Entry & MM_PAGE_PRESENT) == 0 && !CreateTables) {
        return 0;
    }
    if ((*Entry & MM_PAGE_LARGE) != 0) {
        if (!CreateTables || !MmpSplitLargePage(Entry)) {
            return 0;
        }
    }

    PageTable = MmpGetOrCreateTable(Entry, UserAccess);
    if (PageTable == 0) {
        return 0;
    }

    Index = (ULONG)((VirtualAddress >> 12) & 0x1FF);
    return &PageTable[Index];
}

BOOLEAN MmCreateAddressSpace(PMM_ADDRESS_SPACE AddressSpace)
{
    ULONGLONG RootAddress;
    ULONGLONG PointerTableAddress;
    ULONGLONG *KernelRoot;
    ULONGLONG *ProcessRoot;
    ULONGLONG *KernelPointerTable;
    ULONGLONG *ProcessPointerTable;
    ULONGLONG RootEntry;
    ULONG Index;

    if (AddressSpace == 0 || MmpKernelPageTableBase == 0) {
        return FALSE;
    }

    AddressSpace->PageTableBase = 0;
    RootAddress = MmAllocatePhysicalPage();
    if (RootAddress == 0) {
        return FALSE;
    }

    KernelRoot = (ULONGLONG *)(ULONG_PTR)MmpKernelPageTableBase;
    ProcessRoot = (ULONGLONG *)(ULONG_PTR)RootAddress;
    for (Index = 0; Index < MM_PAGE_TABLE_ENTRIES; Index++) {
        ProcessRoot[Index] = Index >= 256 ? KernelRoot[Index] : 0;
    }

    RootEntry = KernelRoot[0];
    if ((RootEntry & MM_PAGE_PRESENT) == 0) {
        MmFreePhysicalPage(RootAddress);
        return FALSE;
    }

    KernelPointerTable = (ULONGLONG *)(ULONG_PTR)(RootEntry & MM_PAGE_ADDRESS_MASK);
    PointerTableAddress = MmAllocatePhysicalPage();
    if (PointerTableAddress == 0) {
        MmFreePhysicalPage(RootAddress);
        return FALSE;
    }

    ProcessPointerTable = (ULONGLONG *)(ULONG_PTR)PointerTableAddress;
    MmpZeroPage(PointerTableAddress);
    for (Index = 0; Index < MM_KERNEL_IDENTITY_DIRECTORY_COUNT; Index++) {
        ULONGLONG Entry = KernelPointerTable[Index];

        if ((Entry & MM_PAGE_PRESENT) == 0 ||
            (Entry & MM_PAGE_LARGE) != 0) {
            MmFreePhysicalPage(PointerTableAddress);
            MmFreePhysicalPage(RootAddress);
            return FALSE;
        }
        ProcessPointerTable[Index] = Entry;
    }

    ProcessRoot[0] = PointerTableAddress | (RootEntry & ~MM_PAGE_ADDRESS_MASK);
    AddressSpace->PageTableBase = RootAddress;
    return TRUE;
}

static VOID MmpDestroyUserTables(ULONGLONG TableAddress, ULONG Level)
{
    ULONGLONG *Table = (ULONGLONG *)(ULONG_PTR)(TableAddress & MM_PAGE_ADDRESS_MASK);
    ULONG Index;

    for (Index = 0; Index < MM_PAGE_TABLE_ENTRIES; Index++) {
        ULONGLONG Entry = Table[Index];
        ULONGLONG ChildAddress;

        if ((Entry & MM_PAGE_PRESENT) == 0) {
            continue;
        }

        if (Level == 1) {
            if ((Entry & MM_PAGE_USER) != 0) {
                MmFreePhysicalPage(Entry & MM_PAGE_ADDRESS_MASK);
            }
            continue;
        }

        if ((Entry & MM_PAGE_LARGE) != 0) {
            continue;
        }

        ChildAddress = Entry & MM_PAGE_ADDRESS_MASK;
        MmpDestroyUserTables(ChildAddress, Level - 1);
        MmFreePhysicalPage(ChildAddress);
    }
}

VOID MmDestroyAddressSpace(PMM_ADDRESS_SPACE AddressSpace)
{
    ULONGLONG CurrentRoot;
    ULONGLONG *Root;
    ULONG Index;
    BOOLEAN RestoreInterrupts;

    if (AddressSpace == 0 || AddressSpace->PageTableBase == 0 ||
        AddressSpace->PageTableBase == MmpKernelPageTableBase) {
        return;
    }

    RestoreInterrupts = KeAcquireSpinLock(&MmpPageTableLock);

    CurrentRoot = MmpReadCr3();
    if (CurrentRoot == AddressSpace->PageTableBase) {
        MmSwitchToKernelAddressSpace();
    }

    Root = (ULONGLONG *)(ULONG_PTR)AddressSpace->PageTableBase;
    for (Index = 0; Index < MM_PAGE_TABLE_ENTRIES; Index++) {
        ULONGLONG Entry = Root[Index];
        ULONGLONG PointerTableAddress;
        ULONGLONG *PointerTable;
        ULONG PointerIndex;

        if ((Entry & MM_PAGE_PRESENT) == 0 || Index >= 256) {
            continue;
        }

        PointerTableAddress = Entry & MM_PAGE_ADDRESS_MASK;
        PointerTable = (ULONGLONG *)(ULONG_PTR)PointerTableAddress;
        for (PointerIndex = 0; PointerIndex < MM_PAGE_TABLE_ENTRIES; PointerIndex++) {
            ULONGLONG PointerEntry = PointerTable[PointerIndex];
            ULONGLONG DirectoryAddress;

            if ((PointerEntry & MM_PAGE_PRESENT) == 0 ||
                (Index == 0 &&
                 PointerIndex < MM_KERNEL_IDENTITY_DIRECTORY_COUNT)) {
                continue;
            }
            if ((PointerEntry & MM_PAGE_LARGE) != 0) {
                continue;
            }

            DirectoryAddress = PointerEntry & MM_PAGE_ADDRESS_MASK;
            MmpDestroyUserTables(DirectoryAddress, 2);
            MmFreePhysicalPage(DirectoryAddress);
        }

        /* Root[0]'s private PDPT references shared identity-map directories. */
        MmFreePhysicalPage(PointerTableAddress);
    }

    MmFreePhysicalPage(AddressSpace->PageTableBase);
    AddressSpace->PageTableBase = 0;
    KeReleaseSpinLock(&MmpPageTableLock, RestoreInterrupts);
}

BOOLEAN MmSwitchAddressSpace(PMM_ADDRESS_SPACE AddressSpace)
{
    if (AddressSpace == 0 || AddressSpace->PageTableBase == 0 ||
        (AddressSpace->PageTableBase & (MM_PAGE_SIZE - 1)) != 0) {
        return FALSE;
    }

    __asm__ volatile ("mov %0, %%cr3" : : "r"(AddressSpace->PageTableBase) : "memory");
    return TRUE;
}

VOID MmSwitchToKernelAddressSpace(VOID)
{
    __asm__ volatile ("mov %0, %%cr3" : : "r"(MmpKernelPageTableBase) : "memory");
}

BOOLEAN MmMapPhysicalPage(PMM_ADDRESS_SPACE AddressSpace,
                          ULONGLONG VirtualAddress,
                          ULONGLONG PhysicalAddress,
                          ULONGLONG Protection)
{
    ULONGLONG *Entry;
    ULONGLONG AllowedProtection;
    BOOLEAN RestoreInterrupts;

    AllowedProtection = MM_PAGE_WRITE | MM_PAGE_USER | MM_PAGE_NO_EXECUTE;
    if (AddressSpace == 0 || AddressSpace->PageTableBase == 0 ||
        (VirtualAddress & (MM_PAGE_SIZE - 1)) != 0 ||
        (PhysicalAddress & (MM_PAGE_SIZE - 1)) != 0 ||
        (PhysicalAddress & ~MM_PAGE_ADDRESS_MASK) != 0 ||
        (Protection & ~AllowedProtection) != 0 ||
        !MmpIsCanonicalAddress(VirtualAddress) ||
        (Protection & MM_PAGE_USER) == 0 ||
        VirtualAddress < MM_USER_ADDRESS_MIN ||
        VirtualAddress >= MM_USER_ADDRESS_LIMIT ||
        ((Protection & MM_PAGE_NO_EXECUTE) != 0 && !MmpNoExecuteEnabled)) {
        return FALSE;
    }

    RestoreInterrupts = KeAcquireSpinLock(&MmpPageTableLock);
    Entry = MmpGetPte(AddressSpace->PageTableBase, VirtualAddress,
                       TRUE, (Protection & MM_PAGE_USER) != 0);
    if (Entry == 0 || (*Entry & MM_PAGE_PRESENT) != 0) {
        KeReleaseSpinLock(&MmpPageTableLock, RestoreInterrupts);
        return FALSE;
    }

    *Entry = PhysicalAddress | MM_PAGE_PRESENT | Protection;
    __asm__ volatile ("invlpg (%0)" : : "r"((PVOID)(ULONG_PTR)VirtualAddress) : "memory");
    KeReleaseSpinLock(&MmpPageTableLock, RestoreInterrupts);
    return TRUE;
}

BOOLEAN MmUnmapVirtualPage(PMM_ADDRESS_SPACE AddressSpace,
                           ULONGLONG VirtualAddress,
                           ULONGLONG *PhysicalAddress)
{
    ULONGLONG *Entry;
    BOOLEAN RestoreInterrupts;

    if (AddressSpace == 0 || AddressSpace->PageTableBase == 0 ||
        (VirtualAddress & (MM_PAGE_SIZE - 1)) != 0 ||
        !MmpIsCanonicalAddress(VirtualAddress) ||
        VirtualAddress < MM_USER_ADDRESS_MIN ||
        VirtualAddress >= MM_USER_ADDRESS_LIMIT) {
        return FALSE;
    }

    RestoreInterrupts = KeAcquireSpinLock(&MmpPageTableLock);
    Entry = MmpGetPte(AddressSpace->PageTableBase, VirtualAddress, FALSE, FALSE);
    if (Entry == 0 || (*Entry & (MM_PAGE_PRESENT | MM_PAGE_USER)) !=
                      (MM_PAGE_PRESENT | MM_PAGE_USER)) {
        KeReleaseSpinLock(&MmpPageTableLock, RestoreInterrupts);
        return FALSE;
    }

    if (PhysicalAddress != 0) {
        *PhysicalAddress = *Entry & MM_PAGE_ADDRESS_MASK;
    }
    *Entry = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"((PVOID)(ULONG_PTR)VirtualAddress) : "memory");
    KeReleaseSpinLock(&MmpPageTableLock, RestoreInterrupts);
    return TRUE;
}

BOOLEAN MmProtectVirtualPage(PMM_ADDRESS_SPACE AddressSpace,
                             ULONGLONG VirtualAddress,
                             ULONGLONG Protection)
{
    ULONGLONG *Entry;
    ULONGLONG AllowedProtection;
    ULONGLONG PhysicalAddress;
    BOOLEAN RestoreInterrupts;

    AllowedProtection = MM_PAGE_WRITE | MM_PAGE_USER | MM_PAGE_NO_EXECUTE;
    if (AddressSpace == 0 || AddressSpace->PageTableBase == 0 ||
        (VirtualAddress & (MM_PAGE_SIZE - 1)) != 0 ||
        (Protection & ~AllowedProtection) != 0 ||
        (Protection & MM_PAGE_USER) == 0 ||
        VirtualAddress < MM_USER_ADDRESS_MIN ||
        VirtualAddress >= MM_USER_ADDRESS_LIMIT ||
        ((Protection & MM_PAGE_NO_EXECUTE) != 0 && !MmpNoExecuteEnabled)) {
        return FALSE;
    }

    RestoreInterrupts = KeAcquireSpinLock(&MmpPageTableLock);
    Entry = MmpGetPte(AddressSpace->PageTableBase, VirtualAddress, FALSE, FALSE);
    if (Entry == 0 || (*Entry & (MM_PAGE_PRESENT | MM_PAGE_USER)) !=
                      (MM_PAGE_PRESENT | MM_PAGE_USER)) {
        KeReleaseSpinLock(&MmpPageTableLock, RestoreInterrupts);
        return FALSE;
    }

    PhysicalAddress = *Entry & MM_PAGE_ADDRESS_MASK;
    *Entry = PhysicalAddress | MM_PAGE_PRESENT | Protection;
    __asm__ volatile ("invlpg (%0)" : : "r"((PVOID)(ULONG_PTR)VirtualAddress)
                      : "memory");
    KeReleaseSpinLock(&MmpPageTableLock, RestoreInterrupts);
    return TRUE;
}

static BOOLEAN MmpIsUserRangeValidWithAccess(ULONGLONG VirtualAddress,
                                              ULONGLONG Length,
                                              BOOLEAN WriteAccess)
{
    ULONGLONG EndAddress;
    ULONGLONG PageAddress;
    BOOLEAN RestoreInterrupts;
    BOOLEAN Valid = TRUE;

    if (Length == 0) {
        return TRUE;
    }

    if (VirtualAddress < MM_USER_ADDRESS_MIN ||
        VirtualAddress >= MM_USER_ADDRESS_LIMIT ||
        Length > MM_USER_ADDRESS_LIMIT - VirtualAddress) {
        return FALSE;
    }

    EndAddress = VirtualAddress + Length;
    PageAddress = VirtualAddress & ~(MM_PAGE_SIZE - 1);
    RestoreInterrupts = KeAcquireSpinLock(&MmpPageTableLock);
    while (PageAddress < EndAddress) {
        if (!MmpIsUserAddressValidLocked(PageAddress, WriteAccess, FALSE)) {
            Valid = FALSE;
            break;
        }
        PageAddress += MM_PAGE_SIZE;
    }

    KeReleaseSpinLock(&MmpPageTableLock, RestoreInterrupts);
    return Valid;
}

BOOLEAN MmIsUserRangeValid(ULONGLONG VirtualAddress, ULONGLONG Length)
{
    return MmpIsUserRangeValidWithAccess(VirtualAddress, Length, FALSE);
}

BOOLEAN MmIsUserRangeWritable(ULONGLONG VirtualAddress, ULONGLONG Length)
{
    return MmpIsUserRangeValidWithAccess(VirtualAddress, Length, TRUE);
}

static BOOLEAN MmpIsUserAddressValidLocked(ULONGLONG VirtualAddress,
                                            BOOLEAN WriteAccess,
                                            BOOLEAN ExecuteAccess)
{
    ULONGLONG *Table;
    ULONGLONG Entry;
    ULONG Indices[4];
    LONG Level;
    BOOLEAN Writable = TRUE;
    BOOLEAN Executable = TRUE;
    BOOLEAN Valid = TRUE;

    if (VirtualAddress < MM_USER_ADDRESS_MIN ||
        VirtualAddress >= MM_USER_ADDRESS_LIMIT ||
        !MmpIsCanonicalAddress(VirtualAddress)) {
        return FALSE;
    }

    Indices[0] = (ULONG)((VirtualAddress >> 39) & 0x1FF);
    Indices[1] = (ULONG)((VirtualAddress >> 30) & 0x1FF);
    Indices[2] = (ULONG)((VirtualAddress >> 21) & 0x1FF);
    Indices[3] = (ULONG)((VirtualAddress >> 12) & 0x1FF);
    Table = (ULONGLONG *)(ULONG_PTR)MmpReadCr3();

    for (Level = 0; Level < 4; Level++) {
        Entry = Table[Indices[Level]];
        if ((Entry & (MM_PAGE_PRESENT | MM_PAGE_USER)) !=
            (MM_PAGE_PRESENT | MM_PAGE_USER)) {
            Valid = FALSE;
            break;
        }
        Writable = Writable && ((Entry & MM_PAGE_WRITE) != 0);
        Executable = Executable &&
                     (!MmpNoExecuteEnabled || (Entry & MM_PAGE_NO_EXECUTE) == 0);
        if (Level == 3) {
            break;
        }
        if ((Entry & MM_PAGE_LARGE) != 0) {
            Valid = FALSE;
            break;
        }
        Table = (ULONGLONG *)(ULONG_PTR)(Entry & MM_PAGE_ADDRESS_MASK);
    }

    Valid = Valid && (!WriteAccess || Writable) &&
            (!ExecuteAccess || Executable);
    return Valid;
}

BOOLEAN MmIsUserAddressValid(ULONGLONG VirtualAddress,
                             BOOLEAN WriteAccess,
                             BOOLEAN ExecuteAccess)
{
    BOOLEAN RestoreInterrupts;
    BOOLEAN Valid;

    RestoreInterrupts = KeAcquireSpinLock(&MmpPageTableLock);
    Valid = MmpIsUserAddressValidLocked(VirtualAddress,
                                        WriteAccess,
                                        ExecuteAccess);
    KeReleaseSpinLock(&MmpPageTableLock, RestoreInterrupts);
    return Valid;
}
