#include <Treas/UserApi.h>

LONG TreUserMain(ULONG ArgumentCount, const CHAR **Arguments)
{
    CHAR InputBuffer[16];
    static const CHAR OutputPrefix[] = "stdin:";
    static const CHAR ErrorMessage[] = "stderr-ok\n";
    LONG ReadCount;

    (void)ArgumentCount;
    (void)Arguments;

    ReadCount = TreReadBuffer(InputBuffer, sizeof(InputBuffer));
    if (ReadCount != 6) {
        if (ReadCount < 0) {
            (VOID)TreWriteBuffer("read-error\n", 11);
        } else if (ReadCount == 0) {
            (VOID)TreWriteBuffer("read-eof\n", 9);
        } else {
            (VOID)TreWriteBuffer(InputBuffer, (ULONGLONG)ReadCount);
        }
        return 1;
    }
    if (TreReadBuffer(InputBuffer, sizeof(InputBuffer)) != 0) {
        return 1;
    }
    if (TreWriteBuffer(OutputPrefix, sizeof(OutputPrefix) - 1) < 0 ||
        TreWriteBuffer(InputBuffer, (ULONGLONG)ReadCount) != ReadCount ||
        TreWriteErrorBuffer(ErrorMessage, sizeof(ErrorMessage) - 1) !=
            (LONG)(sizeof(ErrorMessage) - 1)) {
        return 1;
    }

    return 0;
}
