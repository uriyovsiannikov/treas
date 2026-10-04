#ifndef _TREAS_HOST_FILE_IO_H_
#define _TREAS_HOST_FILE_IO_H_

#include <stddef.h>

int TreaspWriteFile(int FileDescriptor,
                    const void *Buffer,
                    size_t BufferSize);
void TreaspCopyFileToDescriptor(const char *Path, int OutputDescriptor);

#endif
