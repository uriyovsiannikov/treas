#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "FileIo.h"

int TreaspWriteFile(int FileDescriptor,
                    const void *Buffer,
                    size_t BufferSize)
{
    const char *Bytes = Buffer;
    size_t Written = 0;

    while (Written < BufferSize) {
        ssize_t Result = write(FileDescriptor, Bytes + Written,
                               BufferSize - Written);
        if (Result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        if (Result == 0) {
            return 0;
        }
        Written += (size_t)Result;
    }
    return 1;
}

void TreaspCopyFileToDescriptor(const char *Path, int OutputDescriptor)
{
    char Buffer[4096];
    int Descriptor = open(Path, O_RDONLY);

    if (Descriptor < 0) {
        return;
    }
    for (;;) {
        ssize_t Size = read(Descriptor, Buffer, sizeof(Buffer));

        if (Size < 0 && errno == EINTR) {
            continue;
        }
        if (Size <= 0) {
            break;
        }
        (void)TreaspWriteFile(OutputDescriptor, Buffer, (size_t)Size);
    }
    close(Descriptor);
}
