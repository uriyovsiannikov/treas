#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "FileIo.h"
#include "FilePortal.h"
#include "SharedChannel.h"

static VOID TreaspInitializeRing(PTREAS_SHARED_RING Ring,
                                 ULONG DataOffset,
                                 ULONG Capacity)
{
    Ring->ReadOffset = 0;
    Ring->WriteOffset = 0;
    Ring->DataOffset = DataOffset;
    Ring->Capacity = Capacity;
}

static int TreaspDrainRing(PTREASP_SHARED_CHANNEL Channel,
                           PTREAS_SHARED_RING Ring,
                           int OutputDescriptor)
{
    UCHAR *ChannelBase = (UCHAR *)Channel->Header;
    ULONG ReadOffset = __atomic_load_n(&Ring->ReadOffset, __ATOMIC_RELAXED);
    ULONG WriteOffset = __atomic_load_n(&Ring->WriteOffset, __ATOMIC_ACQUIRE);

    while (ReadOffset != WriteOffset) {
        ULONG Available = WriteOffset - ReadOffset;
        ULONG RingIndex = ReadOffset & (Ring->Capacity - 1);
        ULONG CopyLength = Ring->Capacity - RingIndex;

        if (CopyLength > Available) {
            CopyLength = Available;
        }
        if (!TreaspWriteFile(OutputDescriptor,
                             ChannelBase + Ring->DataOffset + RingIndex,
                             CopyLength)) {
            return 0;
        }
        ReadOffset += CopyLength;
        __atomic_store_n(&Ring->ReadOffset, ReadOffset, __ATOMIC_RELEASE);
        WriteOffset = __atomic_load_n(&Ring->WriteOffset, __ATOMIC_ACQUIRE);
    }
    return 1;
}

static VOID TreaspFillInputRing(PTREASP_SHARED_CHANNEL Channel)
{
    PTREAS_SHARED_RING Ring;
    struct pollfd PollDescriptor;
    ULONG ReadOffset;
    ULONG WriteOffset;
    ULONG FreeLength;
    ULONG RingIndex;
    ULONG CopyLength;
    ssize_t ReadLength;
    UCHAR *ChannelBase;

    if (Channel->InputClosed) {
        return;
    }

    PollDescriptor.fd = STDIN_FILENO;
    PollDescriptor.events = POLLIN | POLLHUP;
    PollDescriptor.revents = 0;
    if (poll(&PollDescriptor, 1, 0) <= 0 ||
        (PollDescriptor.revents & (POLLIN | POLLHUP)) == 0) {
        return;
    }

    Ring = &Channel->Header->StandardInput;
    ReadOffset = __atomic_load_n(&Ring->ReadOffset, __ATOMIC_ACQUIRE);
    WriteOffset = __atomic_load_n(&Ring->WriteOffset, __ATOMIC_RELAXED);
    if (WriteOffset - ReadOffset >= Ring->Capacity) {
        return;
    }

    FreeLength = Ring->Capacity - (WriteOffset - ReadOffset);
    RingIndex = WriteOffset & (Ring->Capacity - 1);
    CopyLength = Ring->Capacity - RingIndex;
    if (CopyLength > FreeLength) {
        CopyLength = FreeLength;
    }
    if (CopyLength > 4096) {
        CopyLength = 4096;
    }

    ChannelBase = (UCHAR *)Channel->Header;
    do {
        ReadLength = read(STDIN_FILENO,
                          ChannelBase + Ring->DataOffset + RingIndex,
                          CopyLength);
    } while (ReadLength < 0 && errno == EINTR);

    if (ReadLength > 0) {
        __atomic_store_n(&Ring->WriteOffset,
                         WriteOffset + (ULONG)ReadLength,
                         __ATOMIC_RELEASE);
    } else if (ReadLength == 0) {
        Channel->InputClosed = 1;
        __atomic_store_n(&Channel->Header->InputClosed, 1, __ATOMIC_RELEASE);
    }
}

int TreaspCreateSharedChannel(PTREASP_SHARED_CHANNEL Channel)
{
    VOID *Mapping;

    memset(Channel, 0, sizeof(*Channel));
    Channel->Descriptor = -1;
    memcpy(Channel->Path, "/tmp/TreasSharedChannelXXXXXX",
           sizeof("/tmp/TreasSharedChannelXXXXXX"));
    Channel->Descriptor = mkstemp(Channel->Path);
    if (Channel->Descriptor < 0 ||
        fcntl(Channel->Descriptor, F_SETFD, FD_CLOEXEC) != 0 ||
        ftruncate(Channel->Descriptor, TREAS_SHARED_CHANNEL_SIZE) != 0) {
        perror("treas: cannot create shared channel");
        TreaspDestroySharedChannel(Channel);
        return 0;
    }

    Mapping = mmap(NULL, TREAS_SHARED_CHANNEL_SIZE,
                   PROT_READ | PROT_WRITE, MAP_SHARED,
                   Channel->Descriptor, 0);
    if (Mapping == MAP_FAILED) {
        perror("treas: cannot map shared channel");
        TreaspDestroySharedChannel(Channel);
        return 0;
    }

    Channel->Header = Mapping;
    memset(Channel->Header, 0, TREAS_SHARED_CHANNEL_SIZE);
    Channel->Header->Magic = TREAS_SHARED_CHANNEL_MAGIC;
    Channel->Header->Version = TREAS_SHARED_CHANNEL_VERSION;
    Channel->Header->HeaderSize = sizeof(TREAS_SHARED_CHANNEL);
    Channel->Header->TotalSize = TREAS_SHARED_CHANNEL_SIZE;
    TreaspInitializeRing(&Channel->Header->StandardOutput,
                         TREAS_SHARED_OUTPUT_OFFSET,
                         TREAS_SHARED_OUTPUT_SIZE);
    TreaspInitializeRing(&Channel->Header->StandardError,
                         TREAS_SHARED_ERROR_OFFSET,
                         TREAS_SHARED_ERROR_SIZE);
    TreaspInitializeRing(&Channel->Header->StandardInput,
                         TREAS_SHARED_INPUT_OFFSET,
                         TREAS_SHARED_INPUT_SIZE);
    return 1;
}

int TreaspPumpSharedChannel(PTREASP_SHARED_CHANNEL Channel,
                            PTREASP_FILE_PORTAL FilePortal)
{
    TreaspFillInputRing(Channel);
    TreaspServiceFilePortal(FilePortal, Channel->Header);
    return TreaspDrainRing(Channel, &Channel->Header->StandardOutput,
                           STDOUT_FILENO) &&
           TreaspDrainRing(Channel, &Channel->Header->StandardError,
                           STDERR_FILENO);
}

void TreaspDestroySharedChannel(PTREASP_SHARED_CHANNEL Channel)
{
    if (Channel->Header != NULL) {
        munmap(Channel->Header, TREAS_SHARED_CHANNEL_SIZE);
        Channel->Header = NULL;
    }
    if (Channel->Descriptor >= 0) {
        close(Channel->Descriptor);
        Channel->Descriptor = -1;
    }
    if (Channel->Path[0] != '\0') {
        unlink(Channel->Path);
        Channel->Path[0] = '\0';
    }
}
