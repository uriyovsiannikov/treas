#ifndef _TREAS_USER_FETCH_H_
#define _TREAS_USER_FETCH_H_

#include <Treas/Types.h>

VOID FetchWriteBuffer(const CHAR *Buffer, ULONGLONG Length);
VOID FetchWriteString(const CHAR *String);
VOID FetchWriteUnsigned(ULONGLONG Value);
BOOLEAN FetchQueryProcessorName(CHAR *Buffer, ULONG BufferSize);
LONG TreUserMain(ULONG ArgumentCount, const CHAR **Arguments);

#endif
