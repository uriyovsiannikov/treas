#include <Treas/Hal.h>

#define HAL_EXCEPTION_COUNT 32
#define HAL_TIMER_INTERRUPT_VECTOR 0x20
#define HAL_IDT_ENTRY_COUNT (HAL_TIMER_INTERRUPT_VECTOR + 1)
#define HAL_KERNEL_CODE_SELECTOR 0x08
#define HAL_TSS_SELECTOR 0x28
#define HAL_INTERRUPT_GATE 0x8E

typedef struct _HAL_IDT_ENTRY {
    USHORT OffsetLow;
    USHORT Selector;
    UCHAR InterruptStackTable;
    UCHAR TypeAttributes;
    USHORT OffsetMiddle;
    ULONG OffsetHigh;
    ULONG Reserved;
} __attribute__((packed)) HAL_IDT_ENTRY, *PHAL_IDT_ENTRY;

typedef struct _HAL_IDT_REGISTER {
    USHORT Limit;
    ULONGLONG Base;
} __attribute__((packed)) HAL_IDT_REGISTER, *PHAL_IDT_REGISTER;

typedef struct _HAL_TASK_STATE_SEGMENT {
    ULONG Reserved0;
    ULONGLONG Rsp0;
    ULONGLONG Rsp1;
    ULONGLONG Rsp2;
    ULONGLONG Reserved1;
    ULONGLONG Ist1;
    ULONGLONG Ist2;
    ULONGLONG Ist3;
    ULONGLONG Ist4;
    ULONGLONG Ist5;
    ULONGLONG Ist6;
    ULONGLONG Ist7;
    ULONGLONG Reserved2;
    USHORT Reserved3;
    USHORT IoMapBase;
} __attribute__((packed)) HAL_TASK_STATE_SEGMENT, *PHAL_TASK_STATE_SEGMENT;

static HAL_IDT_ENTRY HalpInterruptDescriptorTable[HAL_IDT_ENTRY_COUNT];
static HAL_TASK_STATE_SEGMENT HalpTaskStateSegment;

typedef struct _HAL_PROCESSOR_CONTROL_REGION {
    ULONGLONG Reserved;
    ULONGLONG KernelStack;
} HAL_PROCESSOR_CONTROL_REGION, *PHAL_PROCESSOR_CONTROL_REGION;

static HAL_PROCESSOR_CONTROL_REGION HalpProcessorControlRegion;

extern VOID (*KiExceptionStubTable[HAL_EXCEPTION_COUNT])(VOID);
extern UCHAR KiBootStackTop;
extern ULONGLONG KiGdtTss[2];
extern VOID KiSystemCallEntry(VOID);
extern VOID KiTimerInterruptEntry(VOID);

static VOID HalpSetInterruptGate(ULONG Vector,
                                 ULONGLONG HandlerAddress,
                                 UCHAR TypeAttributes)
{
    PHAL_IDT_ENTRY Entry;

    Entry = &HalpInterruptDescriptorTable[Vector];
    Entry->OffsetLow = (USHORT)(HandlerAddress & 0xFFFF);
    Entry->Selector = HAL_KERNEL_CODE_SELECTOR;
    Entry->InterruptStackTable = 0;
    Entry->TypeAttributes = TypeAttributes;
    Entry->OffsetMiddle = (USHORT)((HandlerAddress >> 16) & 0xFFFF);
    Entry->OffsetHigh = (ULONG)(HandlerAddress >> 32);
    Entry->Reserved = 0;
}

static VOID HalpInitializeTaskStateSegment(VOID)
{
    ULONGLONG BaseAddress;
    ULONGLONG Limit;

    HalpTaskStateSegment.Rsp0 = (ULONGLONG)(ULONG_PTR)&KiBootStackTop;
    HalpTaskStateSegment.IoMapBase = sizeof(HalpTaskStateSegment);

    BaseAddress = (ULONGLONG)(ULONG_PTR)&HalpTaskStateSegment;
    Limit = sizeof(HalpTaskStateSegment) - 1;
    KiGdtTss[0] = (Limit & 0xFFFF) |
                  ((BaseAddress & 0xFFFFFF) << 16) |
                  (0x89ULL << 40) |
                  ((Limit & 0xF0000) << 32) |
                  ((BaseAddress & 0xFF000000) << 32);
    KiGdtTss[1] = BaseAddress >> 32;

    __asm__ volatile ("ltr %0" : : "r"((USHORT)HAL_TSS_SELECTOR));
}

VOID HalInitializeInterrupts(VOID)
{
    HAL_IDT_REGISTER IdtRegister;
    ULONG Vector;

    HalpInitializeTaskStateSegment();

    for (Vector = 0; Vector < HAL_EXCEPTION_COUNT; Vector++) {
        HalpSetInterruptGate(Vector,
                             (ULONGLONG)KiExceptionStubTable[Vector],
                             HAL_INTERRUPT_GATE);
    }

    HalpSetInterruptGate(HAL_TIMER_INTERRUPT_VECTOR,
                         (ULONGLONG)KiTimerInterruptEntry,
                         HAL_INTERRUPT_GATE);

    IdtRegister.Limit = sizeof(HalpInterruptDescriptorTable) - 1;
    IdtRegister.Base = (ULONGLONG)HalpInterruptDescriptorTable;

    __asm__ volatile ("lidt %0" : : "m"(IdtRegister));
}

static VOID HalpWriteModelSpecificRegister(ULONG Register, ULONGLONG Value)
{
    ULONG LowValue;
    ULONG HighValue;

    LowValue = (ULONG)Value;
    HighValue = (ULONG)(Value >> 32);
    __asm__ volatile ("wrmsr" : : "c"(Register), "a"(LowValue), "d"(HighValue));
}

static ULONGLONG HalpReadModelSpecificRegister(ULONG Register)
{
    ULONG LowValue;
    ULONG HighValue;

    __asm__ volatile ("rdmsr" : "=a"(LowValue), "=d"(HighValue) : "c"(Register));
    return ((ULONGLONG)HighValue << 32) | LowValue;
}

VOID HalInitializeSystemCalls(VOID)
{
    ULONGLONG EferValue;

    HalpProcessorControlRegion.Reserved = 0;
    HalpProcessorControlRegion.KernelStack = (ULONGLONG)(ULONG_PTR)&KiBootStackTop;
    __asm__ volatile ("mov %0, %%gs" : : "r"((USHORT)0x1B));

    EferValue = HalpReadModelSpecificRegister(0xC0000080);
    EferValue |= 1;
    HalpWriteModelSpecificRegister(0xC0000080, EferValue);

    HalpWriteModelSpecificRegister(0xC0000081, (0x13ULL << 48) | (0x08ULL << 32));
    HalpWriteModelSpecificRegister(0xC0000082,
                                    (ULONGLONG)(ULONG_PTR)KiSystemCallEntry);
    HalpWriteModelSpecificRegister(0xC0000084, (1ULL << 8) | (1ULL << 9) | (1ULL << 10));
    HalpWriteModelSpecificRegister(0xC0000101,
                                    (ULONGLONG)(ULONG_PTR)&HalpProcessorControlRegion);
    HalpWriteModelSpecificRegister(0xC0000102, 0);
}

VOID HalSetKernelStack(ULONGLONG StackTop)
{
    HalpTaskStateSegment.Rsp0 = StackTop;
    HalpProcessorControlRegion.KernelStack = StackTop;
}
