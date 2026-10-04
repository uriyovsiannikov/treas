#include <Treas/Kernel.h>
#include <Treas/Hal.h>
#include <Treas/Thread.h>
#include <Treas/UserThread.h>

#define KI_MAXIMUM_THREAD_COUNT 8
#define KI_KERNEL_STACK_SIZE 4096
#define KI_THREAD_STATE_FREE 0
#define KI_THREAD_STATE_READY 1
#define KI_THREAD_STATE_RUNNING 2
#define KI_THREAD_STATE_TERMINATED 3
#define KI_THREAD_CONTEXT_COOPERATIVE 0
#define KI_THREAD_CONTEXT_INTERRUPT 1

typedef struct _KI_THREAD {
    ULONGLONG StackPointer;
    ULONGLONG KernelStackTop;
    PKI_THREAD_START_ROUTINE StartRoutine;
    PVOID Context;
    ULONG State;
    ULONG ContextType;
    BOOLEAN IsUserThread;
    BOOLEAN IsPrimaryUserThread;
    PMM_ADDRESS_SPACE UserAddressSpace;
    ULONG UserStackSlot;
    ULONGLONG UserStackPages[MI_USER_THREAD_STACK_PAGE_COUNT];
    struct _KI_THREAD *ReadyNext;
} KI_THREAD, *PKI_THREAD;

static KI_THREAD KipThreadTable[KI_MAXIMUM_THREAD_COUNT];
static UCHAR KipThreadStacks[KI_MAXIMUM_THREAD_COUNT][KI_KERNEL_STACK_SIZE]
    __attribute__((aligned(16)));
static PKI_THREAD KipCurrentThread;
static PKI_THREAD KipReadyThreadHead;
static PKI_THREAD KipReadyThreadTail;
static BOOLEAN KipThreadSchedulerInitialized;
static BOOLEAN KipPreemptionTimerHeld;

extern UCHAR KiBootStackTop;
extern VOID KipThreadStartup(VOID);
extern VOID KiSwitchContext(ULONGLONG *OldStackPointer,
                            ULONGLONG NewStackPointer,
                            ULONG NewContextType);
VOID KipThreadStartupBody(VOID);

static VOID KipAppendReadyThread(PKI_THREAD Thread)
{
    Thread->ReadyNext = 0;
    Thread->State = KI_THREAD_STATE_READY;

    if (KipReadyThreadTail == 0) {
        KipReadyThreadHead = Thread;
    } else {
        KipReadyThreadTail->ReadyNext = Thread;
    }
    KipReadyThreadTail = Thread;
}

static PKI_THREAD KipRemoveReadyThread(VOID)
{
    PKI_THREAD Thread;

    Thread = KipReadyThreadHead;
    if (Thread == 0) {
        return 0;
    }

    KipReadyThreadHead = Thread->ReadyNext;
    if (KipReadyThreadHead == 0) {
        KipReadyThreadTail = 0;
    }
    Thread->ReadyNext = 0;
    return Thread;
}

static VOID KipInitializeThreadContext(PKI_THREAD Thread,
                                       ULONG ThreadIndex,
                                       PKI_THREAD_START_ROUTINE StartRoutine,
                                       PVOID Context)
{
    ULONGLONG *Stack;
    ULONGLONG StackTop;

    StackTop = (ULONGLONG)(ULONG_PTR)&KipThreadStacks[ThreadIndex][KI_KERNEL_STACK_SIZE];
    StackTop &= ~0xFULL;
    Stack = (ULONGLONG *)(ULONG_PTR)(StackTop - 9 * sizeof(ULONGLONG));

    /* KiSwitchContext restores seven slots, returns into the trampoline, then leaves ABI alignment. */
    Stack[0] = 0; /* r15 */
    Stack[1] = 0; /* r14 */
    Stack[2] = 0; /* r13 */
    Stack[3] = 0; /* r12 */
    Stack[4] = 0; /* rbp */
    Stack[5] = 0; /* rbx */
    Stack[6] = 0x202; /* RFLAGS: interrupts enabled when this thread first runs. */
    Stack[7] = (ULONGLONG)(ULONG_PTR)KipThreadStartup;
    Stack[8] = 0;

    Thread->StackPointer = (ULONGLONG)(ULONG_PTR)Stack;
    Thread->KernelStackTop = StackTop;
    Thread->StartRoutine = StartRoutine;
    Thread->Context = Context;
    Thread->State = KI_THREAD_STATE_READY;
    Thread->ContextType = KI_THREAD_CONTEXT_COOPERATIVE;
    Thread->ReadyNext = 0;
    Thread->IsUserThread = FALSE;
    Thread->IsPrimaryUserThread = FALSE;
    Thread->UserAddressSpace = 0;
    Thread->UserStackSlot = 0;
}

BOOLEAN KeInitializeThreadScheduler(VOID)
{
    if (KipThreadSchedulerInitialized) {
        return FALSE;
    }

    /* Static scheduler storage starts cleared by the boot loader. */
    KipCurrentThread = &KipThreadTable[0];
    KipCurrentThread->KernelStackTop = (ULONGLONG)(ULONG_PTR)&KiBootStackTop;
    KipCurrentThread->State = KI_THREAD_STATE_RUNNING;
    KipThreadSchedulerInitialized = TRUE;
    return TRUE;
}

BOOLEAN KeCreateThread(PKI_THREAD_START_ROUTINE StartRoutine, PVOID Context)
{
    ULONG Index;
    PKI_THREAD Thread;

    if (!KipThreadSchedulerInitialized || StartRoutine == 0) {
        return FALSE;
    }

    for (Index = 1; Index < KI_MAXIMUM_THREAD_COUNT; Index++) {
        if (KipThreadTable[Index].State == KI_THREAD_STATE_FREE ||
            KipThreadTable[Index].State == KI_THREAD_STATE_TERMINATED) {
            break;
        }
    }
    if (Index == KI_MAXIMUM_THREAD_COUNT) {
        return FALSE;
    }

    Thread = &KipThreadTable[Index];
    KipInitializeThreadContext(Thread, Index, StartRoutine, Context);
    KipAppendReadyThread(Thread);
    return TRUE;
}

BOOLEAN KeCreateUserThread(PMM_ADDRESS_SPACE AddressSpace,
                           ULONGLONG EntryPoint,
                           ULONGLONG Argument,
                           ULONGLONG UserStackPointer,
                           ULONG UserStackSlot,
                           const ULONGLONG *UserStackPages,
                           ULONG *ThreadId)
{
    ULONG Index;
    ULONG Slot;
    PKI_THREAD Thread;
    ULONGLONG StackTop;
    ULONGLONG *Frame;

    if (!KipThreadSchedulerInitialized || AddressSpace == 0 ||
        UserStackPages == 0 || ThreadId == 0) {
        return FALSE;
    }

    for (Index = 1; Index < KI_MAXIMUM_THREAD_COUNT; Index++) {
        if (KipThreadTable[Index].State == KI_THREAD_STATE_FREE ||
            KipThreadTable[Index].State == KI_THREAD_STATE_TERMINATED) {
            break;
        }
    }
    if (Index == KI_MAXIMUM_THREAD_COUNT) {
        return FALSE;
    }

    Thread = &KipThreadTable[Index];
    StackTop = (ULONGLONG)(ULONG_PTR)&KipThreadStacks[Index][KI_KERNEL_STACK_SIZE];
    StackTop &= ~0xFULL;
    Frame = (ULONGLONG *)(ULONG_PTR)(StackTop - 20 * sizeof(ULONGLONG));
    for (Slot = 0; Slot < 15; Slot++) {
        Frame[Slot] = 0;
    }
    Frame[8] = Argument; /* rdi */
    Frame[15] = EntryPoint;
    Frame[16] = 0x23; /* User code segment */
    Frame[17] = 0x202; /* IF + reserved bit */
    Frame[18] = UserStackPointer;
    Frame[19] = 0x1B; /* User data segment */

    Thread->StackPointer = (ULONGLONG)(ULONG_PTR)Frame;
    Thread->KernelStackTop = StackTop;
    Thread->StartRoutine = 0;
    Thread->Context = 0;
    Thread->State = KI_THREAD_STATE_READY;
    Thread->ContextType = KI_THREAD_CONTEXT_INTERRUPT;
    Thread->IsUserThread = TRUE;
    Thread->IsPrimaryUserThread = FALSE;
    Thread->UserAddressSpace = AddressSpace;
    Thread->UserStackSlot = UserStackSlot;
    for (Slot = 0; Slot < MI_USER_THREAD_STACK_PAGE_COUNT; Slot++) {
        Thread->UserStackPages[Slot] = UserStackPages[Slot];
    }
    Thread->ReadyNext = 0;
    KipAppendReadyThread(Thread);
    if (!KipPreemptionTimerHeld) {
        HalAcquireTimer();
        KipPreemptionTimerHeld = TRUE;
    }
    *ThreadId = Index;
    return TRUE;
}

VOID KeSetCurrentThreadAsPrimaryUser(VOID)
{
    if (KipCurrentThread != 0) {
        KipCurrentThread->IsUserThread = TRUE;
        KipCurrentThread->IsPrimaryUserThread = TRUE;
    }
}

BOOLEAN KeCurrentThreadIsPrimaryUser(VOID)
{
    return KipCurrentThread != 0 && KipCurrentThread->IsPrimaryUserThread;
}

VOID KeTerminateCurrentUserThread(VOID)
{
    PKI_THREAD Thread = KipCurrentThread;

    if (Thread == 0 || !Thread->IsUserThread || Thread->IsPrimaryUserThread) {
        KiBugCheck(KI_BUGCHECK_SCHEDULER_FAILURE, 5);
    }

    MiDestroyUserThreadStack(Thread->UserAddressSpace,
                             Thread->UserStackSlot,
                             Thread->UserStackPages,
                             MI_USER_THREAD_STACK_PAGE_COUNT);
    Thread->IsUserThread = FALSE;
    KeExitThread();
}

VOID KeYieldThread(VOID)
{
    PKI_THREAD OldThread;
    PKI_THREAD NewThread;

    if (!KipThreadSchedulerInitialized) {
        return;
    }

    NewThread = KipRemoveReadyThread();
    if (NewThread == 0) {
        return;
    }

    OldThread = KipCurrentThread;
    if (OldThread != &KipThreadTable[0]) {
        KipAppendReadyThread(OldThread);
    }
    OldThread->ContextType = KI_THREAD_CONTEXT_COOPERATIVE;
    NewThread->State = KI_THREAD_STATE_RUNNING;
    KipCurrentThread = NewThread;
    HalSetKernelStack(NewThread->KernelStackTop);
    KiSwitchContext(&OldThread->StackPointer,
                    NewThread->StackPointer,
                    NewThread->ContextType);
}

VOID KeExitThread(VOID)
{
    PKI_THREAD OldThread;
    PKI_THREAD NewThread;

    if (!KipThreadSchedulerInitialized || KipCurrentThread == &KipThreadTable[0]) {
        KiBugCheck(KI_BUGCHECK_SCHEDULER_FAILURE, 1);
    }

    OldThread = KipCurrentThread;
    OldThread->State = KI_THREAD_STATE_TERMINATED;
    NewThread = KipRemoveReadyThread();
    if (NewThread == 0) {
        NewThread = &KipThreadTable[0];
        if (NewThread->StackPointer == 0) {
            KiBugCheck(KI_BUGCHECK_SCHEDULER_FAILURE, 2);
        }
    }

    if (KipReadyThreadHead == 0 && KipPreemptionTimerHeld) {
        HalReleaseTimer();
        KipPreemptionTimerHeld = FALSE;
    }

    NewThread->State = KI_THREAD_STATE_RUNNING;
    KipCurrentThread = NewThread;
    HalSetKernelStack(NewThread->KernelStackTop);
    KiSwitchContext(&OldThread->StackPointer,
                    NewThread->StackPointer,
                    NewThread->ContextType);

    KiBugCheck(KI_BUGCHECK_SCHEDULER_FAILURE, 3);
}

VOID KiScheduleFromInterrupt(ULONGLONG *InterruptStack,
                             ULONGLONG *TargetStack,
                             ULONG *TargetContextType)
{
    PKI_THREAD OldThread;
    PKI_THREAD NewThread;

    *TargetStack = (ULONGLONG)(ULONG_PTR)InterruptStack;
    *TargetContextType = KI_THREAD_CONTEXT_INTERRUPT;
    NewThread = KipRemoveReadyThread();
    if (NewThread == 0) {
        return;
    }

    OldThread = KipCurrentThread;
    OldThread->StackPointer = (ULONGLONG)(ULONG_PTR)InterruptStack;
    OldThread->ContextType = KI_THREAD_CONTEXT_INTERRUPT;
    if (OldThread != &KipThreadTable[0]) {
        KipAppendReadyThread(OldThread);
    }

    NewThread->State = KI_THREAD_STATE_RUNNING;
    KipCurrentThread = NewThread;
    HalSetKernelStack(NewThread->KernelStackTop);
    *TargetStack = NewThread->StackPointer;
    *TargetContextType = NewThread->ContextType;
}

VOID KipThreadStartupBody(VOID)
{
    PKI_THREAD Thread;

    Thread = KipCurrentThread;
    if (Thread == 0 || Thread->StartRoutine == 0) {
        KiBugCheck(KI_BUGCHECK_SCHEDULER_FAILURE, 4);
    }

    Thread->StartRoutine(Thread->Context);
    KeExitThread();
}
