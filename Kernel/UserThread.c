#include <Treas/PhysicalMemory.h>
#include <Treas/Kernel.h>
#include <Treas/SpinLock.h>
#include <Treas/UserThread.h>

#define MI_USER_THREAD_STACK_TOP(Slot) \
    (MM_USER_ADDRESS_LIMIT - (ULONGLONG)(Slot) * MI_USER_THREAD_STACK_STRIDE)

static BOOLEAN MipUserThreadStackSlots[MI_USER_THREAD_STACK_SLOT_COUNT];
static KSPIN_LOCK MipUserThreadStackLock;

extern UCHAR KiUserThreadExitThunkStart;
extern UCHAR KiUserThreadExitThunkEnd;

static VOID MipZeroPhysicalPage(ULONGLONG PhysicalAddress)
{
    ULONGLONG *Page = (ULONGLONG *)(ULONG_PTR)PhysicalAddress;
    ULONG Index;

    for (Index = 0; Index < MM_PAGE_SIZE / sizeof(ULONGLONG); Index++) {
        Page[Index] = 0;
    }
}

BOOLEAN MiInitializeUserThreadSupport(PMM_ADDRESS_SPACE AddressSpace)
{
    ULONGLONG PhysicalAddress;
    ULONGLONG ThunkSize;
    ULONGLONG Index;
    UCHAR *Destination;
    const UCHAR *Source;

    if (AddressSpace == 0) {
        return FALSE;
    }

    KeInitializeSpinLock(&MipUserThreadStackLock);
    ThunkSize = (ULONGLONG)(ULONG_PTR)&KiUserThreadExitThunkEnd -
                (ULONGLONG)(ULONG_PTR)&KiUserThreadExitThunkStart;
    if (ThunkSize == 0 || ThunkSize > MM_PAGE_SIZE) {
        return FALSE;
    }

    PhysicalAddress = MmAllocatePhysicalPage();
    if (PhysicalAddress == 0) {
        return FALSE;
    }
    MipZeroPhysicalPage(PhysicalAddress);
    Destination = (UCHAR *)(ULONG_PTR)PhysicalAddress;
    Source = &KiUserThreadExitThunkStart;
    for (Index = 0; Index < ThunkSize; Index++) {
        Destination[Index] = Source[Index];
    }

    if (!MmMapPhysicalPage(AddressSpace,
                           MI_USER_THREAD_EXIT_THUNK_ADDRESS,
                           PhysicalAddress,
                           MM_PAGE_USER)) {
        MmFreePhysicalPage(PhysicalAddress);
        return FALSE;
    }
    return TRUE;
}

BOOLEAN MiCreateUserThreadStack(PMM_ADDRESS_SPACE AddressSpace,
                                ULONGLONG *StackPointer,
                                ULONG *StackSlot,
                                ULONGLONG *StackPages)
{
    ULONG Slot;
    ULONG PageIndex;
    ULONGLONG Base;
    ULONGLONG Top;
    ULONGLONG PhysicalAddress;
    BOOLEAN RestoreInterrupts;

    if (AddressSpace == 0 || StackPointer == 0 || StackSlot == 0 ||
        StackPages == 0) {
        return FALSE;
    }

    RestoreInterrupts = KeAcquireSpinLock(&MipUserThreadStackLock);
    for (Slot = 1; Slot < MI_USER_THREAD_STACK_SLOT_COUNT; Slot++) {
        if (!MipUserThreadStackSlots[Slot]) {
            break;
        }
    }
    if (Slot == MI_USER_THREAD_STACK_SLOT_COUNT) {
        KeReleaseSpinLock(&MipUserThreadStackLock, RestoreInterrupts);
        return FALSE;
    }

    MipUserThreadStackSlots[Slot] = TRUE;
    KeReleaseSpinLock(&MipUserThreadStackLock, RestoreInterrupts);
    Top = MI_USER_THREAD_STACK_TOP(Slot);
    Base = Top - MI_USER_THREAD_STACK_PAGE_COUNT * MM_PAGE_SIZE;
    for (PageIndex = 0; PageIndex < MI_USER_THREAD_STACK_PAGE_COUNT; PageIndex++) {
        PhysicalAddress = MmAllocatePhysicalPage();
        if (PhysicalAddress == 0) {
            MiDestroyUserThreadStack(AddressSpace, Slot, StackPages,
                                     PageIndex);
            return FALSE;
        }
        StackPages[PageIndex] = PhysicalAddress;
        MipZeroPhysicalPage(PhysicalAddress);
        if (!MmMapPhysicalPage(AddressSpace,
                               Base + (ULONGLONG)PageIndex * MM_PAGE_SIZE,
                               PhysicalAddress,
                               MM_PAGE_WRITE | MM_PAGE_USER | MM_PAGE_NO_EXECUTE)) {
            MmFreePhysicalPage(PhysicalAddress);
            StackPages[PageIndex] = 0;
            MiDestroyUserThreadStack(AddressSpace, Slot, StackPages,
                                     PageIndex);
            return FALSE;
        }
    }

    *(ULONGLONG *)(ULONG_PTR)(StackPages[MI_USER_THREAD_STACK_PAGE_COUNT - 1] +
                              MM_PAGE_SIZE - sizeof(ULONGLONG)) =
        MI_USER_THREAD_EXIT_THUNK_ADDRESS;
    *StackPointer = Top - sizeof(ULONGLONG);
    *StackSlot = Slot;
    return TRUE;
}

VOID MiDestroyUserThreadStack(PMM_ADDRESS_SPACE AddressSpace,
                              ULONG StackSlot,
                              const ULONGLONG *StackPages,
                              ULONG StackPageCount)
{
    ULONGLONG StackBase;
    ULONG PageIndex;
    BOOLEAN RestoreInterrupts;

    if (AddressSpace == 0 || StackPages == 0 || StackSlot == 0 ||
        StackSlot >= MI_USER_THREAD_STACK_SLOT_COUNT) {
        return;
    }

    if (StackPageCount > MI_USER_THREAD_STACK_PAGE_COUNT) {
        StackPageCount = MI_USER_THREAD_STACK_PAGE_COUNT;
    }
    StackBase = MI_USER_THREAD_STACK_TOP(StackSlot) -
                MI_USER_THREAD_STACK_PAGE_COUNT * MM_PAGE_SIZE;
    RestoreInterrupts = KeAcquireSpinLock(&MipUserThreadStackLock);
    if (!MipUserThreadStackSlots[StackSlot]) {
        KeReleaseSpinLock(&MipUserThreadStackLock, RestoreInterrupts);
        return;
    }
    for (PageIndex = 0; PageIndex < StackPageCount; PageIndex++) {
        if (StackPages[PageIndex] == 0) {
            continue;
        }
        if (!MmUnmapVirtualPage(AddressSpace,
                                StackBase + (ULONGLONG)PageIndex * MM_PAGE_SIZE) ||
            !MmFreePhysicalPage(StackPages[PageIndex])) {
            KiBugCheck(KI_BUGCHECK_INVALID_MEMORY_MAP,
                       StackBase + (ULONGLONG)PageIndex * MM_PAGE_SIZE);
        }
    }
    MipUserThreadStackSlots[StackSlot] = FALSE;
    KeReleaseSpinLock(&MipUserThreadStackLock, RestoreInterrupts);
}
