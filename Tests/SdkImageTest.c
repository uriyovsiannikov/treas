#include <Treas/UserApi.h>

static volatile ULONG SdkInitializedData = 0x54524541;
static volatile ULONG SdkZeroInitializedData;

LONG TreUserMain(ULONG ArgumentCount, const CHAR **Arguments)
{
    static const CHAR SuccessMessage[] = "image-ok\n";

    (void)ArgumentCount;
    (void)Arguments;

    if (SdkInitializedData != 0x54524541 || SdkZeroInitializedData != 0) {
        return 1;
    }

    SdkZeroInitializedData = 0x53444B;
    if (SdkZeroInitializedData != 0x53444B) {
        return 1;
    }

    return TreWriteBuffer(SuccessMessage, sizeof(SuccessMessage) - 1) < 0;
}
