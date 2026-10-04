#include <Treas/UserApi.h>

static volatile UCHAR DataPageCode[16];

LONG TreUserMain(ULONG ArgumentCount, const CHAR **Arguments)
{
    VOID (*DataPageRoutine)(VOID);

    (void)ArgumentCount;
    (void)Arguments;
    DataPageRoutine = (VOID (*)(VOID))(ULONG_PTR)&DataPageCode[0];
    DataPageRoutine();
    return 0;
}
