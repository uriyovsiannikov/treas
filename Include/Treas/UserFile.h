#ifndef _TREAS_USER_FILE_H_
#define _TREAS_USER_FILE_H_

#include <Treas/Types.h>

#define TREAS_FILE_OPERATION_READ 1u
#define TREAS_FILE_OPERATION_WRITE 2u
#define TREAS_FILE_OPERATION_QUERY_SIZE 3u
#define TREAS_FILE_OPERATION_CLOSE 4u

typedef struct _TREAS_FILE_REQUEST {
    ULONGLONG BufferAddress;
    ULONGLONG Offset;
    ULONGLONG Length;
    ULONGLONG Result;
    ULONG Operation;
    ULONG Handle;
    LONG Status;
    ULONG Reserved;
} TREAS_FILE_REQUEST, *PTREAS_FILE_REQUEST;

#endif
