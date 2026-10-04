#include <Treas/Hal.h>
#include <Treas/ImageLoader.h>
#include <Treas/Kernel.h>
#include <Treas/PhysicalMemory.h>
#include <Treas/ProcessStartup.h>
#include <Treas/Pvh.h>
#include <Treas/Thread.h>
#include <Treas/Timer.h>
#include <Treas/UserThread.h>
#include <Treas/VirtualMemory.h>

extern VOID KiEnterUserMode(ULONGLONG InstructionPointer,
                            ULONGLONG StackPointer,
                            ULONGLONG ArgumentCount,
                            ULONGLONG ArgumentVector);

static MM_ADDRESS_SPACE KipInitialAddressSpace;
static ULONGLONG KipInitialUserEntryPoint;
static ULONGLONG KipInitialUserStackPointer;
static ULONGLONG KipInitialUserArgumentCount;
static ULONGLONG KipInitialUserArgumentVector;

static BOOLEAN KipLoadInitialUserProcess(PPVH_MODULE_ENTRY Module)
{
    MI_LAUNCH_IMAGE LaunchImage;

    if (Module == 0 || !MiParseLaunchImage(Module, &LaunchImage)) {
        return FALSE;
    }

    if (!MmCreateAddressSpace(&KipInitialAddressSpace) ||
        !MiLoadInitialProcess(&KipInitialAddressSpace,
                              LaunchImage.Image,
                              LaunchImage.ImageSize,
                              &KipInitialUserEntryPoint) ||
        !MiInitializeUserThreadSupport(&KipInitialAddressSpace) ||
        !MiBuildInitialUserStack(&KipInitialAddressSpace,
                                 &LaunchImage,
                                 &KipInitialUserStackPointer,
                                 &KipInitialUserArgumentCount,
                                 &KipInitialUserArgumentVector)) {
        MmDestroyAddressSpace(&KipInitialAddressSpace);
        return FALSE;
    }

    return TRUE;
}

static VOID KipRunInitialUserProcess(VOID)
{
    if (!MmSwitchAddressSpace(&KipInitialAddressSpace)) {
        KiBugCheck(KI_BUGCHECK_INVALID_MEMORY_MAP,
                   KipInitialAddressSpace.PageTableBase);
    }

    KeSetCurrentThreadAsPrimaryUser();

    KiEnterUserMode(KipInitialUserEntryPoint,
                    KipInitialUserStackPointer,
                    KipInitialUserArgumentCount,
                    KipInitialUserArgumentVector);
}

static VOID KipStartInitialUserProcess(PVOID StartInfoContext)
{
    ULONG StartInfoAddress = (ULONG)(ULONG_PTR)StartInfoContext;
    PPVH_START_INFO StartInfo = (PPVH_START_INFO)(ULONG_PTR)StartInfoAddress;
    PPVH_MODULE_ENTRY Modules;

    if (StartInfo->ModuleCount == 0 ||
        StartInfo->ModuleListPhysicalAddress == 0 ||
        StartInfo->ModuleListPhysicalAddress >= MM_MAX_PHYSICAL_ADDRESS ||
        (ULONGLONG)StartInfo->ModuleCount * sizeof(PVH_MODULE_ENTRY) >
            MM_MAX_PHYSICAL_ADDRESS - StartInfo->ModuleListPhysicalAddress) {
        KiBugCheck(KI_BUGCHECK_INVALID_PVH_INFO, StartInfo->ModuleCount);
    }

    Modules = (PPVH_MODULE_ENTRY)(ULONG_PTR)StartInfo->ModuleListPhysicalAddress;
    if (!KipLoadInitialUserProcess(&Modules[0])) {
        KiBugCheck(KI_BUGCHECK_INVALID_IMAGE, Modules[0].Size);
    }
    KipRunInitialUserProcess();
}

PMM_ADDRESS_SPACE KiGetInitialProcessAddressSpace(VOID)
{
    return &KipInitialAddressSpace;
}

VOID KiKernelMain(ULONG StartInfoAddress)
{
    HalInitializeSerial();
    HalInitializeInterrupts();
    HalInitializeTimer(KI_SYSTEM_TIMER_FREQUENCY);
    HalInitializeSystemCalls();

    if (!MmInitializePhysicalMemory(StartInfoAddress)) {
        KiBugCheck(KI_BUGCHECK_INVALID_MEMORY_MAP, StartInfoAddress);
    }

    if (!MmInitializeVirtualMemory()) {
        KiBugCheck(KI_BUGCHECK_INVALID_MEMORY_MAP, StartInfoAddress);
    }

    if (!KeInitializeThreadScheduler() ||
        !KeCreateThread(KipStartInitialUserProcess,
                        (PVOID)(ULONG_PTR)StartInfoAddress)) {
        KiBugCheck(KI_BUGCHECK_SCHEDULER_FAILURE, StartInfoAddress);
    }

    KeYieldThread();

    for (;;) {
        KeYieldThread();
        __asm__ volatile ("sti; hlt; cli" : : : "memory");
    }
}
