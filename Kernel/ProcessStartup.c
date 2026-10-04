#include <Treas/PhysicalMemory.h>
#include <Treas/ProcessStartup.h>

#define MI_USER_STACK_PAGE_COUNT 8
#define MI_USER_STACK_TOP MM_USER_ADDRESS_LIMIT
#define MI_USER_STACK_BASE (MI_USER_STACK_TOP - (MI_USER_STACK_PAGE_COUNT * MM_PAGE_SIZE))

static BOOLEAN MipRangeIsValid(ULONGLONG Offset,
                               ULONGLONG Length,
                               ULONGLONG Limit)
{
    return Offset <= Limit && Length <= Limit - Offset;
}

static VOID MipZeroPhysicalPage(ULONGLONG PhysicalAddress)
{
    ULONGLONG *Page;
    ULONG Index;

    Page = (ULONGLONG *)(ULONG_PTR)PhysicalAddress;
    for (Index = 0; Index < MM_PAGE_SIZE / sizeof(ULONGLONG); Index++) {
        Page[Index] = 0;
    }
}

static BOOLEAN MipCopyToInitialStack(const ULONGLONG *StackPages,
                                     ULONGLONG DestinationAddress,
                                     const UCHAR *Source,
                                     ULONGLONG Length)
{
    ULONGLONG StackOffset;
    ULONGLONG PageIndex;
    ULONGLONG PageOffset;
    ULONGLONG CopyLength;
    UCHAR *Destination;
    ULONGLONG Index;

    if (DestinationAddress < MI_USER_STACK_BASE ||
        Length > MI_USER_STACK_TOP - DestinationAddress) {
        return FALSE;
    }

    while (Length != 0) {
        StackOffset = DestinationAddress - MI_USER_STACK_BASE;
        PageIndex = StackOffset / MM_PAGE_SIZE;
        PageOffset = StackOffset & (MM_PAGE_SIZE - 1);
        if (PageIndex >= MI_USER_STACK_PAGE_COUNT) {
            return FALSE;
        }

        CopyLength = MM_PAGE_SIZE - PageOffset;
        if (CopyLength > Length) {
            CopyLength = Length;
        }

        Destination = (UCHAR *)(ULONG_PTR)(StackPages[PageIndex] + PageOffset);
        for (Index = 0; Index < CopyLength; Index++) {
            Destination[Index] = Source[Index];
        }

        DestinationAddress += CopyLength;
        Source += CopyLength;
        Length -= CopyLength;
    }

    return TRUE;
}

BOOLEAN MiParseLaunchImage(PPVH_MODULE_ENTRY Module,
                           PMI_LAUNCH_IMAGE LaunchImage)
{
    PTREAS_LAUNCH_IMAGE_HEADER Header;
    const CHAR *Arguments;
    ULONGLONG ArgumentOffset;
    ULONG ArgumentIndex;

    if (Module == 0 || LaunchImage == 0 || Module->PhysicalAddress == 0 ||
        Module->PhysicalAddress >= MM_MAX_PHYSICAL_ADDRESS ||
        Module->Size > MM_MAX_PHYSICAL_ADDRESS - Module->PhysicalAddress ||
        Module->Size < sizeof(TREAS_LAUNCH_IMAGE_HEADER)) {
        return FALSE;
    }

    Header = (PTREAS_LAUNCH_IMAGE_HEADER)(ULONG_PTR)Module->PhysicalAddress;
    if (Header->Magic != TREAS_LAUNCH_IMAGE_MAGIC ||
        Header->Version != TREAS_LAUNCH_IMAGE_VERSION ||
        Header->HeaderSize != sizeof(TREAS_LAUNCH_IMAGE_HEADER) ||
        Header->ImageOffset != Header->HeaderSize ||
        Header->ImageSize == 0 ||
        Header->ImageSize > TREAS_LAUNCH_MAX_IMAGE_SIZE ||
        Header->ArgumentCount == 0 ||
        Header->ArgumentCount > TREAS_LAUNCH_MAX_ARGUMENT_COUNT ||
        Header->ArgumentSize > TREAS_LAUNCH_MAX_ARGUMENT_SIZE ||
        Header->Reserved != 0 ||
        !MipRangeIsValid(Header->ImageOffset, Header->ImageSize, Module->Size)) {
        return FALSE;
    }

    ArgumentOffset = Header->ImageOffset + Header->ImageSize;
    if (Header->ArgumentOffset != ArgumentOffset ||
        !MipRangeIsValid(Header->ArgumentOffset, Header->ArgumentSize,
                         Module->Size) ||
        Header->ArgumentOffset + Header->ArgumentSize != Module->Size) {
        return FALSE;
    }

    Arguments = (const CHAR *)(ULONG_PTR)(Module->PhysicalAddress +
                                          Header->ArgumentOffset);
    ArgumentOffset = 0;
    for (ArgumentIndex = 0; ArgumentIndex < Header->ArgumentCount; ArgumentIndex++) {
        while (ArgumentOffset < Header->ArgumentSize &&
               Arguments[ArgumentOffset] != '\0') {
            ArgumentOffset++;
        }
        if (ArgumentOffset == Header->ArgumentSize) {
            return FALSE;
        }
        ArgumentOffset++;
    }
    if (ArgumentOffset != Header->ArgumentSize) {
        return FALSE;
    }

    LaunchImage->Image = (const UCHAR *)(ULONG_PTR)(Module->PhysicalAddress +
                                                    Header->ImageOffset);
    LaunchImage->ImageSize = Header->ImageSize;
    LaunchImage->Arguments = Arguments;
    LaunchImage->ArgumentSize = Header->ArgumentSize;
    LaunchImage->ArgumentCount = Header->ArgumentCount;
    return TRUE;
}

BOOLEAN MiBuildInitialUserStack(PMM_ADDRESS_SPACE AddressSpace,
                                PMI_LAUNCH_IMAGE LaunchImage,
                                ULONGLONG *StackPointer,
                                ULONGLONG *ArgumentCount,
                                ULONGLONG *ArgumentVector)
{
    ULONGLONG StackPages[MI_USER_STACK_PAGE_COUNT];
    ULONGLONG UserArgumentVector[TREAS_LAUNCH_MAX_ARGUMENT_COUNT + 1];
    ULONGLONG Cursor;
    ULONGLONG ArgumentDataAddress;
    ULONGLONG ArgumentVectorSize;
    ULONGLONG CurrentArgumentOffset;
    ULONG Index;

    if (AddressSpace == 0 || LaunchImage == 0 || StackPointer == 0 ||
        ArgumentCount == 0 || ArgumentVector == 0 ||
        LaunchImage->ArgumentCount == 0 ||
        LaunchImage->ArgumentCount > TREAS_LAUNCH_MAX_ARGUMENT_COUNT) {
        return FALSE;
    }

    for (Index = 0; Index < MI_USER_STACK_PAGE_COUNT; Index++) {
        StackPages[Index] = MmAllocatePhysicalPage();
        if (StackPages[Index] == 0) {
            return FALSE;
        }
        MipZeroPhysicalPage(StackPages[Index]);
        if (!MmMapPhysicalPage(AddressSpace,
                               MI_USER_STACK_BASE + (ULONGLONG)Index * MM_PAGE_SIZE,
                               StackPages[Index],
                               MM_PAGE_WRITE | MM_PAGE_USER | MM_PAGE_NO_EXECUTE)) {
            MmFreePhysicalPage(StackPages[Index]);
            return FALSE;
        }
    }

    Cursor = MI_USER_STACK_TOP;
    if (LaunchImage->ArgumentSize > Cursor - MI_USER_STACK_BASE) {
        return FALSE;
    }
    Cursor -= LaunchImage->ArgumentSize;
    ArgumentDataAddress = Cursor;

    CurrentArgumentOffset = 0;
    for (Index = 0; Index < LaunchImage->ArgumentCount; Index++) {
        ULONGLONG StartOffset = CurrentArgumentOffset;

        while (CurrentArgumentOffset < LaunchImage->ArgumentSize &&
               LaunchImage->Arguments[CurrentArgumentOffset] != '\0') {
            CurrentArgumentOffset++;
        }
        if (CurrentArgumentOffset == LaunchImage->ArgumentSize) {
            return FALSE;
        }

        UserArgumentVector[Index] = ArgumentDataAddress + StartOffset;
        CurrentArgumentOffset++;
    }
    if (CurrentArgumentOffset != LaunchImage->ArgumentSize) {
        return FALSE;
    }
    if (!MipCopyToInitialStack(StackPages,
                               ArgumentDataAddress,
                               (const UCHAR *)LaunchImage->Arguments,
                               LaunchImage->ArgumentSize)) {
        return FALSE;
    }

    Cursor &= ~0xFULL;
    ArgumentVectorSize = ((ULONGLONG)LaunchImage->ArgumentCount + 1) *
                         sizeof(ULONGLONG);
    if (ArgumentVectorSize > Cursor - MI_USER_STACK_BASE) {
        return FALSE;
    }
    Cursor = (Cursor - ArgumentVectorSize) & ~0xFULL;
    if (Cursor < MI_USER_STACK_BASE) {
        return FALSE;
    }

    UserArgumentVector[LaunchImage->ArgumentCount] = 0;
    if (!MipCopyToInitialStack(StackPages, Cursor,
                               (const UCHAR *)UserArgumentVector,
                               ArgumentVectorSize)) {
        return FALSE;
    }

    *StackPointer = Cursor;
    *ArgumentCount = LaunchImage->ArgumentCount;
    *ArgumentVector = Cursor;
    return TRUE;
}
