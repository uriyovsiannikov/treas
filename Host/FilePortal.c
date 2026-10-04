#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <Treas/Status.h>
#include <Treas/UserFile.h>
#include "FilePortal.h"

int TreaspInitializeFilePortal(PTREASP_FILE_PORTAL Portal,
                               ULONG FileCount,
                               const TREASP_FILE_ARGUMENT *Files)
{
    ULONG Index;

    memset(Portal, 0, sizeof(*Portal));
    for (Index = 0; Index < TREAS_SHARED_MAX_FILES; Index++) {
        Portal->Files[Index].Descriptor = -1;
    }
    if (FileCount > TREAS_SHARED_MAX_FILES) {
        fprintf(stderr, "treas: too many host files\n");
        return 0;
    }

    for (Index = 0; Index < FileCount; Index++) {
        int Flags = Files[Index].Access == TREASP_FILE_ACCESS_READ
                        ? O_RDONLY | O_CLOEXEC
                        : O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC;

        Portal->Files[Index].Descriptor = open(Files[Index].Path, Flags, 0644);
        if (Portal->Files[Index].Descriptor < 0) {
            perror("treas: cannot open host file");
            TreaspDestroyFilePortal(Portal);
            return 0;
        }
        Portal->Files[Index].Access = Files[Index].Access;
        Portal->FileCount++;
    }
    return 1;
}

void TreaspServiceFilePortal(PTREASP_FILE_PORTAL Portal,
                             PTREAS_SHARED_CHANNEL Channel)
{
    ULONG RequestSequence;
    ULONG HandleIndex;
    PTREASP_HOST_FILE File;
    UCHAR *FileBuffer;
    ssize_t Result;

    RequestSequence = __atomic_load_n(&Channel->FileRequestSequence,
                                      __ATOMIC_ACQUIRE);
    if (RequestSequence ==
        __atomic_load_n(&Channel->FileResponseSequence, __ATOMIC_RELAXED)) {
        return;
    }

    Channel->FileStatus = TREAS_STATUS_INVALID_HANDLE;
    Channel->FileResult = 0;
    if (Channel->FileHandle == 0 ||
        Channel->FileHandle > Portal->FileCount) {
        goto Complete;
    }
    HandleIndex = Channel->FileHandle - 1;
    File = &Portal->Files[HandleIndex];
    if (File->Descriptor < 0) {
        goto Complete;
    }

    FileBuffer = (UCHAR *)Channel + TREAS_SHARED_FILE_OFFSET;
    if (Channel->FileOperation == TREAS_FILE_OPERATION_READ) {
        if (File->Access != TREASP_FILE_ACCESS_READ) {
            Channel->FileStatus = TREAS_STATUS_ACCESS_DENIED;
            goto Complete;
        }
        do {
            Result = pread(File->Descriptor, FileBuffer,
                           Channel->FileLength, (off_t)Channel->FileOffset);
        } while (Result < 0 && errno == EINTR);
        if (Result >= 0) {
            Channel->FileStatus = TREAS_STATUS_SUCCESS;
            Channel->FileResult = (ULONGLONG)Result;
        }
    } else if (Channel->FileOperation == TREAS_FILE_OPERATION_WRITE) {
        if (File->Access != TREASP_FILE_ACCESS_WRITE) {
            Channel->FileStatus = TREAS_STATUS_ACCESS_DENIED;
            goto Complete;
        }
        do {
            Result = pwrite(File->Descriptor, FileBuffer,
                            Channel->FileLength, (off_t)Channel->FileOffset);
        } while (Result < 0 && errno == EINTR);
        if (Result >= 0) {
            Channel->FileStatus = TREAS_STATUS_SUCCESS;
            Channel->FileResult = (ULONGLONG)Result;
        }
    } else if (Channel->FileOperation == TREAS_FILE_OPERATION_QUERY_SIZE) {
        struct stat Status;

        if (fstat(File->Descriptor, &Status) == 0) {
            Channel->FileStatus = TREAS_STATUS_SUCCESS;
            Channel->FileResult = (ULONGLONG)Status.st_size;
        }
    } else if (Channel->FileOperation == TREAS_FILE_OPERATION_CLOSE) {
        close(File->Descriptor);
        File->Descriptor = -1;
        Channel->FileStatus = TREAS_STATUS_SUCCESS;
    } else {
        Channel->FileStatus = TREAS_STATUS_INVALID_PARAMETER;
    }

Complete:
    __atomic_store_n(&Channel->FileResponseSequence,
                     RequestSequence,
                     __ATOMIC_RELEASE);
}

void TreaspDestroyFilePortal(PTREASP_FILE_PORTAL Portal)
{
    ULONG Index;

    for (Index = 0; Index < Portal->FileCount; Index++) {
        if (Portal->Files[Index].Descriptor >= 0) {
            close(Portal->Files[Index].Descriptor);
            Portal->Files[Index].Descriptor = -1;
        }
    }
    Portal->FileCount = 0;
}
