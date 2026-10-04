#include <Treas/UserApi.h>

LONG TreUserMain(ULONG ArgumentCount, const CHAR **Arguments)
{
    static const volatile CHAR ReadOnlyValue[] = "read-only";

    (void)ArgumentCount;
    (void)Arguments;
    *(volatile CHAR *)(ULONG_PTR)ReadOnlyValue = 'R';
    return 0;
}
