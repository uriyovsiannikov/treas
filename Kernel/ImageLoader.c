#include <Treas/ImageLoader.h>
#include <Treas/PhysicalMemory.h>
#include <Treas/UserThread.h>
#include <Treas/UserVirtualMemoryManager.h>

#define MI_ELF_CLASS_64 2
#define MI_ELF_DATA_LITTLE_ENDIAN 1
#define MI_ELF_TYPE_EXECUTABLE 2
#define MI_ELF_MACHINE_X86_64 62
#define MI_ELF_PROGRAM_NULL 0
#define MI_ELF_PROGRAM_LOAD 1
#define MI_ELF_FLAG_EXECUTE 1
#define MI_ELF_FLAG_WRITE 2
#define MI_ELF_FLAG_READ 4
#define MI_ELF_FLAG_MASK 7
#define MI_MAX_PROGRAM_HEADERS 16
#define MI_MAX_IMAGE_PAGES 256

typedef struct _MI_ELF64_HEADER {
    UCHAR Identity[16];
    USHORT Type;
    USHORT Machine;
    ULONG Version;
    ULONGLONG Entry;
    ULONGLONG ProgramHeaderOffset;
    ULONGLONG SectionHeaderOffset;
    ULONG Flags;
    USHORT HeaderSize;
    USHORT ProgramHeaderSize;
    USHORT ProgramHeaderCount;
    USHORT SectionHeaderSize;
    USHORT SectionHeaderCount;
    USHORT SectionNameIndex;
} MI_ELF64_HEADER, *PMI_ELF64_HEADER;

typedef struct _MI_ELF64_PROGRAM_HEADER {
    ULONG Type;
    ULONG Flags;
    ULONGLONG Offset;
    ULONGLONG VirtualAddress;
    ULONGLONG PhysicalAddress;
    ULONGLONG FileSize;
    ULONGLONG MemorySize;
    ULONGLONG Alignment;
} MI_ELF64_PROGRAM_HEADER, *PMI_ELF64_PROGRAM_HEADER;

static BOOLEAN MipRangeIsValid(ULONGLONG Offset,
                               ULONGLONG Size,
                               ULONGLONG Limit)
{
    return Offset <= Limit && Size <= Limit - Offset;
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

static BOOLEAN MipValidateLoadSegment(ULONGLONG ImageSize,
                                      PMI_ELF64_PROGRAM_HEADER Segment,
                                      ULONGLONG *PageStart,
                                      ULONGLONG *PageEnd)
{
    ULONGLONG SegmentEnd;

    if (Segment->MemorySize == 0) {
        if (Segment->FileSize != 0 || Segment->Offset > ImageSize) {
            return FALSE;
        }
        *PageStart = 0;
        *PageEnd = 0;
        return TRUE;
    }

    if (Segment->FileSize > Segment->MemorySize ||
        !MipRangeIsValid(Segment->Offset, Segment->FileSize, ImageSize) ||
        Segment->VirtualAddress < MM_USER_ADDRESS_MIN ||
        Segment->VirtualAddress >= MM_USER_ADDRESS_LIMIT ||
        Segment->MemorySize > MM_USER_ADDRESS_LIMIT - Segment->VirtualAddress ||
        (Segment->Flags & ~MI_ELF_FLAG_MASK) != 0 ||
        (Segment->Flags & MI_ELF_FLAG_READ) == 0 ||
        (Segment->Flags & (MI_ELF_FLAG_WRITE | MI_ELF_FLAG_EXECUTE)) ==
            (MI_ELF_FLAG_WRITE | MI_ELF_FLAG_EXECUTE)) {
        return FALSE;
    }

    if (Segment->Alignment > 1 &&
        ((Segment->Alignment & (Segment->Alignment - 1)) != 0 ||
         (Segment->VirtualAddress & (Segment->Alignment - 1)) !=
             (Segment->Offset & (Segment->Alignment - 1)))) {
        return FALSE;
    }
    if ((Segment->VirtualAddress & (MM_PAGE_SIZE - 1)) !=
        (Segment->Offset & (MM_PAGE_SIZE - 1))) {
        return FALSE;
    }

    SegmentEnd = Segment->VirtualAddress + Segment->MemorySize;
    if ((Segment->VirtualAddress < MI_USER_THREAD_EXIT_THUNK_ADDRESS + MM_PAGE_SIZE &&
         SegmentEnd > MI_USER_THREAD_EXIT_THUNK_ADDRESS) ||
        (Segment->VirtualAddress < MI_USER_DYNAMIC_LIMIT &&
         SegmentEnd > MI_USER_DYNAMIC_BASE) ||
        SegmentEnd > MI_USER_THREAD_STACK_RESERVED_BASE) {
        return FALSE;
    }

    *PageStart = Segment->VirtualAddress & ~(MM_PAGE_SIZE - 1);
    *PageEnd = (SegmentEnd + MM_PAGE_SIZE - 1) & ~(MM_PAGE_SIZE - 1);
    return *PageEnd >= SegmentEnd;
}

static BOOLEAN MipLoadSegment(PMM_ADDRESS_SPACE AddressSpace,
                              const UCHAR *Image,
                              ULONGLONG ImageSize,
                              PMI_ELF64_PROGRAM_HEADER Segment,
                              ULONGLONG *LoadedPageCount)
{
    ULONGLONG PageAddress;
    ULONGLONG PageCount;
    ULONGLONG PageIndex;
    ULONGLONG Protection;
    ULONGLONG FileEndAddress;
    ULONGLONG PageStart;
    ULONGLONG PageEnd;

    if (!MipValidateLoadSegment(ImageSize, Segment, &PageStart, &PageEnd)) {
        return FALSE;
    }

    PageCount = (PageEnd - PageStart) / MM_PAGE_SIZE;
    if (PageCount > MI_MAX_IMAGE_PAGES - *LoadedPageCount) {
        return FALSE;
    }
    FileEndAddress = Segment->VirtualAddress + Segment->FileSize;

    Protection = MM_PAGE_USER;
    if ((Segment->Flags & MI_ELF_FLAG_WRITE) != 0) {
        Protection |= MM_PAGE_WRITE;
    }
    if ((Segment->Flags & MI_ELF_FLAG_EXECUTE) == 0) {
        Protection |= MM_PAGE_NO_EXECUTE;
    }

    for (PageIndex = 0; PageIndex < PageCount; PageIndex++) {
        ULONGLONG PhysicalAddress;
        ULONGLONG CopyStart;
        ULONGLONG CopyEnd;
        ULONGLONG CopySize;
        ULONGLONG SourceOffset;
        UCHAR *Destination;
        ULONGLONG Index;

        PageAddress = PageStart + PageIndex * MM_PAGE_SIZE;
        PhysicalAddress = MmAllocatePhysicalPage();
        if (PhysicalAddress == 0) {
            return FALSE;
        }
        MipZeroPhysicalPage(PhysicalAddress);
        if (!MmMapPhysicalPage(AddressSpace, PageAddress, PhysicalAddress, Protection)) {
            MmFreePhysicalPage(PhysicalAddress);
            return FALSE;
        }
        (*LoadedPageCount)++;

        CopyStart = PageAddress > Segment->VirtualAddress
                        ? PageAddress
                        : Segment->VirtualAddress;
        CopyEnd = PageAddress + MM_PAGE_SIZE;
        if (CopyEnd > FileEndAddress) {
            CopyEnd = FileEndAddress;
        }
        if (CopyStart < CopyEnd) {
            CopySize = CopyEnd - CopyStart;
            SourceOffset = Segment->Offset +
                           (CopyStart - Segment->VirtualAddress);
            Destination = (UCHAR *)(ULONG_PTR)(PhysicalAddress +
                                               CopyStart - PageAddress);
            for (Index = 0; Index < CopySize; Index++) {
                Destination[Index] = Image[SourceOffset + Index];
            }
        }
    }

    return TRUE;
}

BOOLEAN MiLoadInitialProcess(PMM_ADDRESS_SPACE AddressSpace,
                             const UCHAR *Image,
                             ULONGLONG ImageSize,
                             ULONGLONG *EntryPoint)
{
    PMI_ELF64_HEADER Header;
    PMI_ELF64_PROGRAM_HEADER ProgramHeaders;
    ULONGLONG LoadedPageCount;
    ULONGLONG ImageEntry;
    ULONG Index;
    BOOLEAN EntryMapped;
    ULONG LoadSegmentCount;

    if (AddressSpace == 0 || Image == 0 || EntryPoint == 0 ||
        ImageSize < sizeof(MI_ELF64_HEADER)) {
        return FALSE;
    }

    Header = (PMI_ELF64_HEADER)Image;
    if (Header->Identity[0] != 0x7F || Header->Identity[1] != 'E' ||
        Header->Identity[2] != 'L' || Header->Identity[3] != 'F' ||
        Header->Identity[4] != MI_ELF_CLASS_64 ||
        Header->Identity[5] != MI_ELF_DATA_LITTLE_ENDIAN ||
        Header->Identity[6] != 1 || Header->Type != MI_ELF_TYPE_EXECUTABLE ||
        Header->Machine != MI_ELF_MACHINE_X86_64 || Header->Version != 1 ||
        Header->Flags != 0 || Header->ProgramHeaderOffset < Header->HeaderSize ||
        Header->HeaderSize != sizeof(MI_ELF64_HEADER) ||
        Header->ProgramHeaderSize != sizeof(MI_ELF64_PROGRAM_HEADER) ||
        Header->ProgramHeaderCount == 0 ||
        Header->ProgramHeaderCount > MI_MAX_PROGRAM_HEADERS ||
        !MipRangeIsValid(Header->ProgramHeaderOffset,
                         (ULONGLONG)Header->ProgramHeaderCount *
                             sizeof(MI_ELF64_PROGRAM_HEADER),
                         ImageSize)) {
        return FALSE;
    }

    ProgramHeaders = (PMI_ELF64_PROGRAM_HEADER)(Image + Header->ProgramHeaderOffset);
    LoadedPageCount = 0;
    ImageEntry = Header->Entry;
    EntryMapped = FALSE;
    LoadSegmentCount = 0;

    /* Validate the complete image layout before allocating any user pages. */
    for (Index = 0; Index < Header->ProgramHeaderCount; Index++) {
        if (ProgramHeaders[Index].Type == MI_ELF_PROGRAM_LOAD) {
            ULONGLONG SegmentEnd;
            ULONGLONG PageStart;
            ULONGLONG PageEnd;
            ULONG PreviousIndex;

            if (!MipValidateLoadSegment(ImageSize, &ProgramHeaders[Index],
                                        &PageStart, &PageEnd)) {
                return FALSE;
            }
            if (ProgramHeaders[Index].MemorySize == 0) {
                continue;
            }

            if (ImageEntry >= ProgramHeaders[Index].VirtualAddress &&
                ProgramHeaders[Index].FileSize <=
                    (~0ULL - ProgramHeaders[Index].VirtualAddress)) {
                SegmentEnd = ProgramHeaders[Index].VirtualAddress +
                             ProgramHeaders[Index].FileSize;
                if (ImageEntry < SegmentEnd &&
                    (ProgramHeaders[Index].Flags & MI_ELF_FLAG_EXECUTE) != 0) {
                    EntryMapped = TRUE;
                }
            }

            for (PreviousIndex = 0; PreviousIndex < Index; PreviousIndex++) {
                ULONGLONG PreviousPageStart;
                ULONGLONG PreviousPageEnd;

                if (ProgramHeaders[PreviousIndex].Type != MI_ELF_PROGRAM_LOAD) {
                    continue;
                }
                if (!MipValidateLoadSegment(ImageSize,
                                            &ProgramHeaders[PreviousIndex],
                                            &PreviousPageStart,
                                            &PreviousPageEnd)) {
                    return FALSE;
                }
                if (PageStart < PreviousPageEnd &&
                    PreviousPageStart < PageEnd) {
                    return FALSE;
                }
            }
            LoadSegmentCount++;
        } else if (ProgramHeaders[Index].Type != MI_ELF_PROGRAM_NULL) {
            return FALSE;
        }
    }

    if (!EntryMapped || LoadSegmentCount == 0) {
        return FALSE;
    }

    for (Index = 0; Index < Header->ProgramHeaderCount; Index++) {
        if (ProgramHeaders[Index].Type == MI_ELF_PROGRAM_LOAD) {
            if (!MipLoadSegment(AddressSpace, Image, ImageSize,
                                &ProgramHeaders[Index],
                                &LoadedPageCount)) {
                return FALSE;
            }
        }
    }

    *EntryPoint = Header->Entry;
    return TRUE;
}
