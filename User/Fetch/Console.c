#include <Treas/UserApi.h>
#include <Fetch.h>

VOID FetchWriteBuffer(const CHAR *Buffer, ULONGLONG Length)
{
    (VOID)TreWriteBuffer(Buffer, Length);
}

VOID FetchWriteString(const CHAR *String)
{
    ULONGLONG Length = 0;

    while (String[Length] != '\0') {
        Length++;
    }
    FetchWriteBuffer(String, Length);
}

VOID FetchWriteUnsigned(ULONGLONG Value)
{
    CHAR Digits[20];
    ULONG DigitCount = 0;
    ULONG Index;

    do {
        Digits[DigitCount++] = (CHAR)('0' + Value % 10);
        Value /= 10;
    } while (Value != 0);

    for (Index = 0; Index < DigitCount / 2; Index++) {
        CHAR Temporary = Digits[Index];
        Digits[Index] = Digits[DigitCount - Index - 1];
        Digits[DigitCount - Index - 1] = Temporary;
    }
    FetchWriteBuffer(Digits, DigitCount);
}
