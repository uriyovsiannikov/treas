#include <Treas/UserApi.h>

static VOID SdkObjectSignalThread(PVOID Context)
{
    ULONG Handle = (ULONG)(ULONG_PTR)Context;

    (VOID)TreSetEvent(Handle);
}

LONG TreUserMain(ULONG ArgumentCount, const CHAR **Arguments)
{
    LONG Handle;

    (void)ArgumentCount;
    (void)Arguments;

    Handle = TreCreateEvent(FALSE, FALSE);
    if (Handle < 0 ||
        TreCreateThread(SdkObjectSignalThread,
                        (PVOID)(ULONG_PTR)(ULONG)Handle) < 0 ||
        TreWaitForEvent((ULONG)Handle) != TREAS_STATUS_SUCCESS ||
        TreCloseHandle((ULONG)Handle) != TREAS_STATUS_SUCCESS) {
        return 30;
    }
    if (TreWriteBuffer("object-ok\n", 10) < 0) {
        return 31;
    }
    return 0;
}
