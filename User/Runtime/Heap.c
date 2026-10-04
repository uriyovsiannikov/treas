#include <Treas/UserApi.h>

#define TREP_HEAP_ARENA_MAGIC 0x414E455241534552ULL
#define TREP_HEAP_BLOCK_MAGIC 0x4B434F4C42534552ULL
#define TREP_HEAP_ARENA_SIZE (64ULL * 1024)
#define TREP_HEAP_ALIGNMENT 16ULL
#define TREP_HEAP_BLOCK_FREE 1u

typedef struct _TREP_HEAP_BLOCK {
    ULONGLONG Magic;
    ULONGLONG Size;
    struct _TREP_HEAP_BLOCK *Next;
    ULONG Flags;
    ULONG Reserved;
} TREP_HEAP_BLOCK, *PTREP_HEAP_BLOCK;

typedef struct _TREP_HEAP_ARENA {
    ULONGLONG Magic;
    ULONGLONG RegionSize;
    struct _TREP_HEAP_ARENA *Next;
    TREP_HEAP_BLOCK FirstBlock;
} TREP_HEAP_ARENA, *PTREP_HEAP_ARENA;

static PTREP_HEAP_ARENA TrepHeapArenas;
static volatile ULONG TrepHeapLock;

static VOID TrepAcquireHeapLock(VOID)
{
    while (__atomic_exchange_n(&TrepHeapLock, 1, __ATOMIC_ACQUIRE) != 0) {
        while (__atomic_load_n(&TrepHeapLock, __ATOMIC_RELAXED) != 0) {
            __asm__ volatile ("pause");
        }
    }
}

static VOID TrepReleaseHeapLock(VOID)
{
    __atomic_store_n(&TrepHeapLock, 0, __ATOMIC_RELEASE);
}

static ULONGLONG TrepAlignSize(ULONGLONG Size)
{
    return (Size + TREP_HEAP_ALIGNMENT - 1) & ~(TREP_HEAP_ALIGNMENT - 1);
}

static PVOID TrepAllocateFromArena(PTREP_HEAP_ARENA Arena,
                                   ULONGLONG Size)
{
    PTREP_HEAP_BLOCK Block;

    for (Block = &Arena->FirstBlock; Block != 0; Block = Block->Next) {
        if ((Block->Flags & TREP_HEAP_BLOCK_FREE) == 0 || Block->Size < Size) {
            continue;
        }
        if (Block->Size >= Size + sizeof(TREP_HEAP_BLOCK) +
                           TREP_HEAP_ALIGNMENT) {
            PTREP_HEAP_BLOCK NewBlock =
                (PTREP_HEAP_BLOCK)((UCHAR *)(Block + 1) + Size);

            NewBlock->Magic = TREP_HEAP_BLOCK_MAGIC;
            NewBlock->Size = Block->Size - Size - sizeof(TREP_HEAP_BLOCK);
            NewBlock->Next = Block->Next;
            NewBlock->Flags = TREP_HEAP_BLOCK_FREE;
            NewBlock->Reserved = 0;
            Block->Next = NewBlock;
            Block->Size = Size;
        }
        Block->Flags &= ~TREP_HEAP_BLOCK_FREE;
        return Block + 1;
    }
    return 0;
}

static PTREP_HEAP_ARENA TrepCreateArena(ULONGLONG Size)
{
    PTREP_HEAP_ARENA Arena;
    ULONGLONG RequiredSize;

    if (Size > ~0ULL - sizeof(TREP_HEAP_ARENA)) {
        return 0;
    }
    RequiredSize = Size + sizeof(TREP_HEAP_ARENA);
    if (RequiredSize < TREP_HEAP_ARENA_SIZE) {
        RequiredSize = TREP_HEAP_ARENA_SIZE;
    }
    Arena = TreAllocateVirtualMemory(
        RequiredSize,
        TREAS_VM_PROTECT_READ | TREAS_VM_PROTECT_WRITE);
    if (Arena == 0) {
        return 0;
    }

    Arena->Magic = TREP_HEAP_ARENA_MAGIC;
    Arena->RegionSize = (RequiredSize + 0xFFF) & ~0xFFFULL;
    Arena->Next = 0;
    Arena->FirstBlock.Magic = TREP_HEAP_BLOCK_MAGIC;
    Arena->FirstBlock.Size = Arena->RegionSize - sizeof(TREP_HEAP_ARENA);
    Arena->FirstBlock.Next = 0;
    Arena->FirstBlock.Flags = TREP_HEAP_BLOCK_FREE;
    Arena->FirstBlock.Reserved = 0;
    return Arena;
}

static PTREP_HEAP_ARENA TrepFindArena(PVOID Buffer,
                                      PTREP_HEAP_ARENA *PreviousArena)
{
    ULONG_PTR Address = (ULONG_PTR)Buffer;
    PTREP_HEAP_ARENA Previous = 0;
    PTREP_HEAP_ARENA Arena;

    for (Arena = TrepHeapArenas; Arena != 0; Arena = Arena->Next) {
        ULONG_PTR Base = (ULONG_PTR)Arena;

        if (Address > Base && Address < Base + Arena->RegionSize) {
            if (PreviousArena != 0) {
                *PreviousArena = Previous;
            }
            return Arena;
        }
        Previous = Arena;
    }
    return 0;
}

VOID TreCopyMemory(PVOID Destination, const VOID *Source, ULONGLONG Length)
{
    UCHAR *DestinationBytes = Destination;
    const UCHAR *SourceBytes = Source;
    ULONG_PTR DestinationAddress = (ULONG_PTR)Destination;
    ULONG_PTR SourceAddress = (ULONG_PTR)Source;
    ULONGLONG Index;

    if (DestinationAddress <= SourceAddress ||
        DestinationAddress - SourceAddress >= Length) {
        for (Index = 0; Index < Length; Index++) {
            DestinationBytes[Index] = SourceBytes[Index];
        }
    } else {
        for (Index = Length; Index != 0; Index--) {
            DestinationBytes[Index - 1] = SourceBytes[Index - 1];
        }
    }
}

VOID TreZeroMemory(PVOID Destination, ULONGLONG Length)
{
    UCHAR *Bytes = Destination;
    ULONGLONG Index;

    for (Index = 0; Index < Length; Index++) {
        Bytes[Index] = 0;
    }
}

PVOID TreAllocate(ULONGLONG Size)
{
    PTREP_HEAP_ARENA Arena;
    PVOID Buffer;

    if (Size == 0 || Size > ~0ULL - TREP_HEAP_ALIGNMENT) {
        return 0;
    }
    Size = TrepAlignSize(Size);

    TrepAcquireHeapLock();
    for (Arena = TrepHeapArenas; Arena != 0; Arena = Arena->Next) {
        Buffer = TrepAllocateFromArena(Arena, Size);
        if (Buffer != 0) {
            TrepReleaseHeapLock();
            return Buffer;
        }
    }
    TrepReleaseHeapLock();

    Arena = TrepCreateArena(Size);
    if (Arena == 0) {
        return 0;
    }
    TrepAcquireHeapLock();
    Arena->Next = TrepHeapArenas;
    TrepHeapArenas = Arena;
    Buffer = TrepAllocateFromArena(Arena, Size);
    TrepReleaseHeapLock();
    return Buffer;
}

VOID TreFree(PVOID Buffer)
{
    PTREP_HEAP_ARENA PreviousArena;
    PTREP_HEAP_ARENA Arena;
    PTREP_HEAP_BLOCK Block;
    BOOLEAN ReleaseArena = FALSE;

    if (Buffer == 0) {
        return;
    }
    TrepAcquireHeapLock();
    Arena = TrepFindArena(Buffer, &PreviousArena);
    Block = (PTREP_HEAP_BLOCK)Buffer - 1;
    if (Arena == 0 || Block->Magic != TREP_HEAP_BLOCK_MAGIC ||
        (Block->Flags & TREP_HEAP_BLOCK_FREE) != 0) {
        TrepReleaseHeapLock();
        return;
    }

    Block->Flags |= TREP_HEAP_BLOCK_FREE;
    for (Block = &Arena->FirstBlock; Block != 0 && Block->Next != 0;) {
        if ((Block->Flags & TREP_HEAP_BLOCK_FREE) != 0 &&
            (Block->Next->Flags & TREP_HEAP_BLOCK_FREE) != 0) {
            Block->Size += sizeof(TREP_HEAP_BLOCK) + Block->Next->Size;
            Block->Next = Block->Next->Next;
        } else {
            Block = Block->Next;
        }
    }

    if ((Arena->FirstBlock.Flags & TREP_HEAP_BLOCK_FREE) != 0 &&
        Arena->FirstBlock.Next == 0) {
        if (PreviousArena == 0) {
            TrepHeapArenas = Arena->Next;
        } else {
            PreviousArena->Next = Arena->Next;
        }
        ReleaseArena = TRUE;
    }
    TrepReleaseHeapLock();
    if (ReleaseArena) {
        (VOID)TreFreeVirtualMemory(Arena);
    }
}

PVOID TreReallocate(PVOID Buffer, ULONGLONG Size)
{
    PTREP_HEAP_ARENA Arena;
    PTREP_HEAP_BLOCK Block;
    ULONGLONG OldSize;
    PVOID NewBuffer;

    if (Buffer == 0) {
        return TreAllocate(Size);
    }
    if (Size == 0) {
        TreFree(Buffer);
        return 0;
    }

    TrepAcquireHeapLock();
    Arena = TrepFindArena(Buffer, 0);
    Block = (PTREP_HEAP_BLOCK)Buffer - 1;
    if (Arena == 0 || Block->Magic != TREP_HEAP_BLOCK_MAGIC ||
        (Block->Flags & TREP_HEAP_BLOCK_FREE) != 0) {
        TrepReleaseHeapLock();
        return 0;
    }
    OldSize = Block->Size;
    if (Size <= OldSize) {
        TrepReleaseHeapLock();
        return Buffer;
    }
    TrepReleaseHeapLock();

    NewBuffer = TreAllocate(Size);
    if (NewBuffer == 0) {
        return 0;
    }
    TreCopyMemory(NewBuffer, Buffer, OldSize);
    TreFree(Buffer);
    return NewBuffer;
}
