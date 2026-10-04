#include <Treas/Hal.h>
#include <Treas/SharedProtocol.h>
#include <Treas/Status.h>
#include "HalInternal.h"

static PTREAS_SHARED_CHANNEL HalpSharedChannel;

static BOOLEAN HalpValidateRing(PTREAS_SHARED_RING Ring)
{
    return Ring->Capacity != 0 &&
           (Ring->Capacity & (Ring->Capacity - 1)) == 0 &&
           Ring->DataOffset >= sizeof(TREAS_SHARED_CHANNEL) &&
           Ring->DataOffset <= TREAS_SHARED_CHANNEL_SIZE &&
           Ring->Capacity <= TREAS_SHARED_CHANNEL_SIZE - Ring->DataOffset;
}

static VOID HalpWaitForHost(VOID)
{
    HalAcquireTimer();
    __asm__ volatile ("sti; hlt; cli" : : : "memory");
    HalReleaseTimer();
}

static VOID HalpWriteRing(PTREAS_SHARED_RING Ring,
                          const CHAR *Buffer,
                          ULONGLONG Length)
{
    UCHAR *ChannelBase = (UCHAR *)HalpSharedChannel;

    while (Length != 0) {
        ULONG ReadOffset = __atomic_load_n(&Ring->ReadOffset, __ATOMIC_ACQUIRE);
        ULONG WriteOffset = __atomic_load_n(&Ring->WriteOffset, __ATOMIC_RELAXED);
        ULONG Used = WriteOffset - ReadOffset;
        ULONG Available;
        ULONG RingIndex;
        ULONG CopyLength;
        ULONG Index;

        if (Used >= Ring->Capacity) {
            HalpWaitForHost();
            continue;
        }
        Available = Ring->Capacity - Used;
        RingIndex = WriteOffset & (Ring->Capacity - 1);
        CopyLength = Ring->Capacity - RingIndex;
        if (CopyLength > Available) {
            CopyLength = Available;
        }
        if ((ULONGLONG)CopyLength > Length) {
            CopyLength = (ULONG)Length;
        }
        for (Index = 0; Index < CopyLength; Index++) {
            ChannelBase[Ring->DataOffset + RingIndex + Index] =
                (UCHAR)Buffer[Index];
        }
        __atomic_store_n(&Ring->WriteOffset,
                         WriteOffset + CopyLength,
                         __ATOMIC_RELEASE);
        Buffer += CopyLength;
        Length -= CopyLength;
    }
}

BOOLEAN HalInitializeApplicationIo(VOID)
{
    PTREAS_SHARED_CHANNEL Channel;

    Channel = (PTREAS_SHARED_CHANNEL)HalpMapSharedChannel(
        TREAS_SHARED_CHANNEL_SIZE);
    if (Channel == 0 ||
        Channel->Magic != TREAS_SHARED_CHANNEL_MAGIC ||
        Channel->Version != TREAS_SHARED_CHANNEL_VERSION ||
        Channel->HeaderSize != sizeof(TREAS_SHARED_CHANNEL) ||
        Channel->TotalSize != TREAS_SHARED_CHANNEL_SIZE ||
        !HalpValidateRing(&Channel->StandardOutput) ||
        !HalpValidateRing(&Channel->StandardError) ||
        !HalpValidateRing(&Channel->StandardInput)) {
        return FALSE;
    }

    HalpSharedChannel = Channel;
    __atomic_store_n(&Channel->GuestReady, 1, __ATOMIC_RELEASE);
    return TRUE;
}

VOID HalWriteApplicationBuffer(const CHAR *Buffer, ULONGLONG Length)
{
    if (HalpSharedChannel != 0) {
        HalpWriteRing(&HalpSharedChannel->StandardOutput, Buffer, Length);
    }
}

VOID HalWriteApplicationErrorBuffer(const CHAR *Buffer, ULONGLONG Length)
{
    if (HalpSharedChannel != 0) {
        HalpWriteRing(&HalpSharedChannel->StandardError, Buffer, Length);
    }
}

VOID HalFlushApplicationStreams(VOID)
{
    if (HalpSharedChannel == 0) {
        return;
    }

    while (__atomic_load_n(&HalpSharedChannel->StandardOutput.ReadOffset,
                           __ATOMIC_ACQUIRE) !=
               __atomic_load_n(&HalpSharedChannel->StandardOutput.WriteOffset,
                               __ATOMIC_RELAXED) ||
           __atomic_load_n(&HalpSharedChannel->StandardError.ReadOffset,
                           __ATOMIC_ACQUIRE) !=
               __atomic_load_n(&HalpSharedChannel->StandardError.WriteOffset,
                               __ATOMIC_RELAXED)) {
        HalpWaitForHost();
    }
}

ULONGLONG HalReadApplicationBuffer(CHAR *Buffer, ULONGLONG Length)
{
    PTREAS_SHARED_RING Ring;
    UCHAR *ChannelBase;
    ULONG ReadOffset;
    ULONG WriteOffset;
    ULONG Available;
    ULONG RingIndex;
    ULONG CopyLength;
    ULONG Index;

    if (HalpSharedChannel == 0 || Length == 0) {
        return 0;
    }

    Ring = &HalpSharedChannel->StandardInput;
    ChannelBase = (UCHAR *)HalpSharedChannel;
    for (;;) {
        ReadOffset = __atomic_load_n(&Ring->ReadOffset, __ATOMIC_RELAXED);
        WriteOffset = __atomic_load_n(&Ring->WriteOffset, __ATOMIC_ACQUIRE);
        if (ReadOffset != WriteOffset) {
            break;
        }
        if (__atomic_load_n(&HalpSharedChannel->InputClosed,
                            __ATOMIC_ACQUIRE) != 0) {
            return 0;
        }
        HalpWaitForHost();
    }

    Available = WriteOffset - ReadOffset;
    RingIndex = ReadOffset & (Ring->Capacity - 1);
    CopyLength = Ring->Capacity - RingIndex;
    if (CopyLength > Available) {
        CopyLength = Available;
    }
    if ((ULONGLONG)CopyLength > Length) {
        CopyLength = (ULONG)Length;
    }
    for (Index = 0; Index < CopyLength; Index++) {
        Buffer[Index] = (CHAR)ChannelBase[Ring->DataOffset + RingIndex + Index];
    }
    __atomic_store_n(&Ring->ReadOffset,
                     ReadOffset + CopyLength,
                     __ATOMIC_RELEASE);
    return CopyLength;
}

VOID HalExecuteHostFileRequest(PTREAS_FILE_REQUEST Request)
{
    PTREAS_SHARED_CHANNEL Channel = HalpSharedChannel;
    UCHAR *FileBuffer;
    ULONG Sequence;
    ULONG Index;

    if (Channel == 0 || Request == 0 ||
        Request->Length > TREAS_SHARED_FILE_SIZE) {
        if (Request != 0) {
            Request->Status = TREAS_STATUS_INVALID_PARAMETER;
        }
        return;
    }

    while (__atomic_load_n(&Channel->FileRequestSequence, __ATOMIC_ACQUIRE) !=
           __atomic_load_n(&Channel->FileResponseSequence, __ATOMIC_ACQUIRE)) {
        HalpWaitForHost();
    }

    FileBuffer = (UCHAR *)Channel + TREAS_SHARED_FILE_OFFSET;
    if (Request->Operation == TREAS_FILE_OPERATION_WRITE) {
        const UCHAR *Source = (const UCHAR *)(ULONG_PTR)Request->BufferAddress;

        for (Index = 0; Index < (ULONG)Request->Length; Index++) {
            FileBuffer[Index] = Source[Index];
        }
    }

    Channel->FileOperation = Request->Operation;
    Channel->FileHandle = Request->Handle;
    Channel->FileOffset = Request->Offset;
    Channel->FileLength = (ULONG)Request->Length;
    Channel->FileStatus = TREAS_STATUS_UNSUCCESSFUL;
    Channel->FileResult = 0;
    Sequence = Channel->FileRequestSequence + 1;
    __atomic_store_n(&Channel->FileRequestSequence, Sequence, __ATOMIC_RELEASE);

    while (__atomic_load_n(&Channel->FileResponseSequence, __ATOMIC_ACQUIRE) !=
           Sequence) {
        HalpWaitForHost();
    }

    Request->Status = Channel->FileStatus;
    Request->Result = Channel->FileResult;
    if (Request->Status == TREAS_STATUS_SUCCESS &&
        Request->Operation == TREAS_FILE_OPERATION_READ &&
        Request->Result <= Request->Length) {
        UCHAR *Destination = (UCHAR *)(ULONG_PTR)Request->BufferAddress;

        for (Index = 0; Index < (ULONG)Request->Result; Index++) {
            Destination[Index] = FileBuffer[Index];
        }
    } else if (Request->Status == TREAS_STATUS_SUCCESS &&
               Request->Operation == TREAS_FILE_OPERATION_READ) {
        Request->Status = TREAS_STATUS_UNSUCCESSFUL;
        Request->Result = 0;
    }
}
