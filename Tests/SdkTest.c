#include <Treas/UserApi.h>

LONG TreUserMain(ULONG ArgumentCount, const CHAR **Arguments)
{
    TREAS_SYSTEM_INFORMATION Information;
    static const CHAR SuccessMessage[] = "sdk-ok\n";

    (void)ArgumentCount;
    (void)Arguments;

    if (TreQuerySystemInformation(&Information) != TREAS_STATUS_SUCCESS ||
        Information.ProcessorCount == 0 ||
        TreWriteBuffer(SuccessMessage, sizeof(SuccessMessage) - 1) !=
            (LONG)(sizeof(SuccessMessage) - 1)) {
        return 1;
    }

    return 0;
}
