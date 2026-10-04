#ifndef _TREAS_SHARED_PROTOCOL_H_
#define _TREAS_SHARED_PROTOCOL_H_

#include <Treas/Types.h>

#define TREAS_SHARED_CHANNEL_MAGIC 0x4D485354u
#define TREAS_SHARED_CHANNEL_VERSION 1u
#define TREAS_SHARED_CHANNEL_SIZE (1u << 20)

#define TREAS_SHARED_OUTPUT_OFFSET 0x1000u
#define TREAS_SHARED_OUTPUT_SIZE 0x40000u
#define TREAS_SHARED_ERROR_OFFSET 0x41000u
#define TREAS_SHARED_ERROR_SIZE 0x20000u
#define TREAS_SHARED_INPUT_OFFSET 0x61000u
#define TREAS_SHARED_INPUT_SIZE 0x20000u
#define TREAS_SHARED_FILE_OFFSET 0x81000u
#define TREAS_SHARED_FILE_SIZE 0x40000u
#define TREAS_SHARED_MAX_FILES 16u

typedef struct _TREAS_SHARED_RING {
    volatile ULONG ReadOffset;
    volatile ULONG WriteOffset;
    ULONG DataOffset;
    ULONG Capacity;
} TREAS_SHARED_RING, *PTREAS_SHARED_RING;

typedef struct _TREAS_SHARED_CHANNEL {
    ULONG Magic;
    ULONG Version;
    ULONG HeaderSize;
    ULONG TotalSize;
    TREAS_SHARED_RING StandardOutput;
    TREAS_SHARED_RING StandardError;
    TREAS_SHARED_RING StandardInput;
    volatile ULONG InputClosed;
    volatile ULONG GuestReady;
    volatile ULONG ExitStatus;
    ULONG Reserved0;
    volatile ULONG FileRequestSequence;
    volatile ULONG FileResponseSequence;
    ULONG FileOperation;
    ULONG FileHandle;
    ULONGLONG FileOffset;
    ULONG FileLength;
    volatile LONG FileStatus;
    volatile ULONGLONG FileResult;
} TREAS_SHARED_CHANNEL, *PTREAS_SHARED_CHANNEL;

#endif
