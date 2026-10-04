#include <Treas/UserApi.h>

LONG TreUserMain(ULONG ArgumentCount, const CHAR **Arguments)
{
    CHAR Buffer[32];
    ULONGLONG FileSize;
    LONG Length;

    (void)ArgumentCount;
    (void)Arguments;

    if (TreQueryFileSize(1, &FileSize) != TREAS_STATUS_SUCCESS ||
        FileSize != 16) {
        return 20;
    }
    Length = TreReadFile(1, Buffer, sizeof(Buffer), 0);
    if (Length != 16 || TreWriteFile(2, Buffer, (ULONGLONG)Length, 0) != Length) {
        return 21;
    }
    if (TreCloseFile(1) != TREAS_STATUS_SUCCESS ||
        TreCloseFile(2) != TREAS_STATUS_SUCCESS) {
        return 22;
    }
    if (TreWriteBuffer("file-ok\n", 8) < 0) {
        return 23;
    }
    return 0;
}
