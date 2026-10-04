#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <Treas/LaunchProtocol.h>
#include "FileIo.h"
#include "LaunchImage.h"

int TreaspCreateLaunchImage(const char *BinaryPath,
                            ULONG ArgumentCount,
                            char *const *Arguments,
                            char **LaunchPath)
{
    TREAS_LAUNCH_IMAGE_HEADER Header;
    struct stat BinaryStatus;
    char *TemporaryPath;
    char CopyBuffer[8192];
    int BinaryDescriptor;
    int LaunchDescriptor;
    ULONG Index;
    ULONGLONG ArgumentSize;
    ULONGLONG ImageBytesCopied;

    if (ArgumentCount == 0 ||
        ArgumentCount > TREAS_LAUNCH_MAX_ARGUMENT_COUNT) {
        fprintf(stderr, "treas: too many application arguments\n");
        return 0;
    }

    BinaryDescriptor = open(BinaryPath, O_RDONLY);
    if (BinaryDescriptor < 0 || fstat(BinaryDescriptor, &BinaryStatus) != 0 ||
        !S_ISREG(BinaryStatus.st_mode) || BinaryStatus.st_size <= 0 ||
        (ULONGLONG)BinaryStatus.st_size > TREAS_LAUNCH_MAX_IMAGE_SIZE) {
        fprintf(stderr, "treas: application image has an invalid size or type\n");
        if (BinaryDescriptor >= 0) {
            close(BinaryDescriptor);
        }
        return 0;
    }

    ArgumentSize = 0;
    for (Index = 0; Index < ArgumentCount; Index++) {
        ULONGLONG StringSize = (ULONGLONG)strlen(Arguments[Index]) + 1;
        if (StringSize > TREAS_LAUNCH_MAX_ARGUMENT_SIZE - ArgumentSize) {
            fprintf(stderr, "treas: application arguments exceed 16 KiB\n");
            close(BinaryDescriptor);
            return 0;
        }
        ArgumentSize += StringSize;
    }

    TemporaryPath = strdup("/tmp/TreasLaunchXXXXXX");
    if (TemporaryPath == NULL) {
        perror("treas: cannot allocate launch path");
        close(BinaryDescriptor);
        return 0;
    }
    LaunchDescriptor = mkstemp(TemporaryPath);
    if (LaunchDescriptor < 0) {
        perror("treas: cannot create launch image");
        free(TemporaryPath);
        close(BinaryDescriptor);
        return 0;
    }

    Header.Magic = TREAS_LAUNCH_IMAGE_MAGIC;
    Header.Version = TREAS_LAUNCH_IMAGE_VERSION;
    Header.HeaderSize = sizeof(Header);
    Header.ImageOffset = sizeof(Header);
    Header.ImageSize = (ULONGLONG)BinaryStatus.st_size;
    Header.ArgumentOffset = Header.ImageOffset + Header.ImageSize;
    Header.ArgumentSize = ArgumentSize;
    Header.ArgumentCount = ArgumentCount;
    Header.Reserved = 0;

    if (!TreaspWriteFile(LaunchDescriptor, &Header, sizeof(Header))) {
        goto Failure;
    }

    ImageBytesCopied = 0;
    while (ImageBytesCopied < Header.ImageSize) {
        size_t RequestedSize = sizeof(CopyBuffer);
        ssize_t ReadSize;

        if (Header.ImageSize - ImageBytesCopied < RequestedSize) {
            RequestedSize = (size_t)(Header.ImageSize - ImageBytesCopied);
        }
        ReadSize = read(BinaryDescriptor, CopyBuffer, RequestedSize);
        if (ReadSize < 0 && errno == EINTR) {
            continue;
        }
        if (ReadSize <= 0 ||
            !TreaspWriteFile(LaunchDescriptor, CopyBuffer, (size_t)ReadSize)) {
            goto Failure;
        }
        ImageBytesCopied += (ULONGLONG)ReadSize;
    }

    for (Index = 0; Index < ArgumentCount; Index++) {
        size_t StringSize = strlen(Arguments[Index]) + 1;
        if (!TreaspWriteFile(LaunchDescriptor, Arguments[Index], StringSize)) {
            goto Failure;
        }
    }

    close(BinaryDescriptor);
    close(LaunchDescriptor);
    *LaunchPath = TemporaryPath;
    return 1;

Failure:
    perror("treas: cannot write launch image");
    close(BinaryDescriptor);
    close(LaunchDescriptor);
    unlink(TemporaryPath);
    free(TemporaryPath);
    return 0;
}
