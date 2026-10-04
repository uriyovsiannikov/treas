#include <Fetch.h>

typedef struct _FETCH_CPUID_RESULT {
    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;
} FETCH_CPUID_RESULT, *PFETCH_CPUID_RESULT;

static VOID FetchQueryCpuid(ULONG Leaf,
                            ULONG Subleaf,
                            PFETCH_CPUID_RESULT Result)
{
    ULONG Eax = Leaf;
    ULONG Ebx;
    ULONG Ecx = Subleaf;
    ULONG Edx;

    __asm__ volatile ("cpuid"
                      : "+a"(Eax), "=b"(Ebx), "+c"(Ecx), "=d"(Edx));
    Result->Eax = Eax;
    Result->Ebx = Ebx;
    Result->Ecx = Ecx;
    Result->Edx = Edx;
}

BOOLEAN FetchQueryProcessorName(CHAR *Buffer, ULONG BufferSize)
{
    FETCH_CPUID_RESULT Result;
    UCHAR *Destination = (UCHAR *)Buffer;
    const UCHAR *Source;
    ULONG MaximumExtendedLeaf;
    ULONG Leaf;
    ULONG Index;

    if (Buffer == 0 || BufferSize < 13) {
        return FALSE;
    }

    FetchQueryCpuid(0, 0, &Result);
    if (BufferSize >= 13) {
        Source = (const UCHAR *)&Result.Ebx;
        for (Index = 0; Index < 4; Index++) {
            Destination[Index] = Source[Index];
        }
        Source = (const UCHAR *)&Result.Edx;
        for (Index = 0; Index < 4; Index++) {
            Destination[Index + 4] = Source[Index];
        }
        Source = (const UCHAR *)&Result.Ecx;
        for (Index = 0; Index < 4; Index++) {
            Destination[Index + 8] = Source[Index];
        }
        Destination[12] = '\0';
    }

    if (BufferSize < 49) {
        return TRUE;
    }

    FetchQueryCpuid(0x80000000, 0, &Result);
    MaximumExtendedLeaf = Result.Eax;
    if (MaximumExtendedLeaf < 0x80000004) {
        return TRUE;
    }

    for (Leaf = 0x80000002; Leaf <= 0x80000004; Leaf++) {
        FetchQueryCpuid(Leaf, 0, &Result);
        Source = (const UCHAR *)&Result;
        for (Index = 0; Index < sizeof(Result); Index++) {
            Destination[(Leaf - 0x80000002) * sizeof(Result) + Index] =
                Source[Index];
        }
    }

    Destination[48] = '\0';
    for (Index = 48; Index > 0 && Destination[Index - 1] == ' '; Index--) {
        Destination[Index - 1] = '\0';
    }
    return TRUE;
}
