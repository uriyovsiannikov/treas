#include <Fetch.h>
#include <Treas/UserApi.h>

#define FETCH_LOGO_COLUMN_WIDTH 16
#define FETCH_INFO_LABEL_WIDTH 12

static VOID FetchWriteTextColumnPrefix(VOID)
{
    FetchWriteBuffer("                ", FETCH_LOGO_COLUMN_WIDTH);
}

static VOID FetchWriteLogoPrefix(const CHAR *Logo)
{
    ULONGLONG LogoLength = 0;

    FetchWriteString("\033[96m");
    FetchWriteString(Logo);
    FetchWriteString("\033[0m");
    while (Logo[LogoLength] != '\0') {
        LogoLength++;
    }
    if (LogoLength < FETCH_LOGO_COLUMN_WIDTH) {
        FetchWriteBuffer("                ",
                         FETCH_LOGO_COLUMN_WIDTH - LogoLength);
    }
}

static VOID FetchWriteInformationLabel(const CHAR *Label)
{
    ULONGLONG LabelLength = 0;

    FetchWriteString(Label);
    while (Label[LabelLength] != '\0') {
        LabelLength++;
    }
    if (LabelLength < FETCH_INFO_LABEL_WIDTH) {
        FetchWriteBuffer("            ", FETCH_INFO_LABEL_WIDTH - LabelLength);
    }
}

static VOID FetchWriteMemoryValues(
    const TREAS_SYSTEM_INFORMATION *Information)
{
    const ULONGLONG Mebibyte = 1024ULL * 1024ULL;

    FetchWriteUnsigned(Information->ManagedMemoryBytes / Mebibyte);
    FetchWriteString(" MiB managed, ");
    FetchWriteUnsigned(Information->FreeMemoryBytes / Mebibyte);
    FetchWriteString(" MiB free");
}

static VOID FetchWriteUptime(const TREAS_SYSTEM_INFORMATION *Information)
{
    ULONGLONG UptimeSeconds;
    ULONGLONG Hundredths;

    FetchWriteTextColumnPrefix();
    FetchWriteInformationLabel("Uptime:");
    if (Information->TimerTicks == 0) {
        FetchWriteString("<0.01 seconds\n");
        return;
    }

    if (Information->TimerFrequency == 0) {
        UptimeSeconds = 0;
        Hundredths = 0;
    } else {
        UptimeSeconds = Information->TimerTicks / Information->TimerFrequency;
        Hundredths = ((Information->TimerTicks % Information->TimerFrequency) *
                      100) / Information->TimerFrequency;
    }
    FetchWriteUnsigned(UptimeSeconds);
    FetchWriteString(".");
    if (Hundredths < 10) {
        FetchWriteString("0");
    }
    FetchWriteUnsigned(Hundredths);
    FetchWriteString(" seconds\n");
}

LONG TreUserMain(ULONG ArgumentCount, const CHAR **Arguments)
{
    TREAS_SYSTEM_INFORMATION Information;
    CHAR ProcessorName[49];
    ULONGLONG Result;
    BOOLEAN HasProcessorName;
    BOOLEAN HasSystemInformation;

    (void)ArgumentCount;
    (void)Arguments;

    HasProcessorName = FetchQueryProcessorName(ProcessorName,
                                               sizeof(ProcessorName));
    Result = TreQuerySystemInformation(&Information);
    HasSystemInformation = Result == TREAS_STATUS_SUCCESS;

    FetchWriteTextColumnPrefix();
    FetchWriteString("Treas OS 0.1\n");

    FetchWriteLogoPrefix("    \\__  o");
    FetchWriteInformationLabel("Kernel:");
    FetchWriteString("Treas64\n");

    FetchWriteLogoPrefix("|\\/    o\\  o");
    FetchWriteInformationLabel("CPU:");
    FetchWriteString(HasProcessorName ? ProcessorName : "unavailable");
    FetchWriteString("\n");

    FetchWriteLogoPrefix(">       < o");
    FetchWriteInformationLabel("Memory:");
    if (HasSystemInformation) {
        FetchWriteMemoryValues(&Information);
    } else {
        FetchWriteString("unavailable");
    }
    FetchWriteString("\n");

    FetchWriteLogoPrefix("|/\\   __/");
    FetchWriteInformationLabel("vCPU:");
    if (HasSystemInformation) {
        FetchWriteUnsigned(Information.ProcessorCount);
    } else {
        FetchWriteString("unavailable");
    }
    FetchWriteString("\n");

    FetchWriteLogoPrefix("    //");
    FetchWriteInformationLabel("Runtime:");
    FetchWriteString("PVH virtual machine\n");

    if (HasSystemInformation) {
        FetchWriteUptime(&Information);
        return 0;
    }

    FetchWriteTextColumnPrefix();
    FetchWriteInformationLabel("Uptime:");
    FetchWriteString("unavailable\n");
    return 1;
}
