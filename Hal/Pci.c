#include <Treas/SharedProtocol.h>
#include "HalInternal.h"

#define HAL_PCI_CONFIG_ADDRESS 0xCF8
#define HAL_PCI_CONFIG_DATA 0xCFC
#define HAL_PCI_ENABLE_CONFIGURATION 0x80000000UL
#define HAL_PCI_IVSHMEM_IDENTIFIER 0x11101AF4UL
#define HAL_PCI_COMMAND_MEMORY_SPACE 0x0002
#define HAL_PCI_SHARED_BAR_INDEX 2
#define HAL_PCI_SHARED_BASE 0xE0000000UL

static VOID HalpWritePortUlong(USHORT Port, ULONG Value)
{
    __asm__ volatile ("outl %0, %1" : : "a"(Value), "Nd"(Port));
}

static ULONG HalpReadPortUlong(USHORT Port)
{
    ULONG Value;

    __asm__ volatile ("inl %1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

static ULONG HalpReadPciConfig(ULONG Device, ULONG Function, ULONG Offset)
{
    ULONG Address = HAL_PCI_ENABLE_CONFIGURATION |
                    (Device << 11) | (Function << 8) | (Offset & ~3UL);

    HalpWritePortUlong(HAL_PCI_CONFIG_ADDRESS, Address);
    return HalpReadPortUlong(HAL_PCI_CONFIG_DATA);
}

static VOID HalpWritePciConfig(ULONG Device,
                               ULONG Function,
                               ULONG Offset,
                               ULONG Value)
{
    ULONG Address = HAL_PCI_ENABLE_CONFIGURATION |
                    (Device << 11) | (Function << 8) | (Offset & ~3UL);

    HalpWritePortUlong(HAL_PCI_CONFIG_ADDRESS, Address);
    HalpWritePortUlong(HAL_PCI_CONFIG_DATA, Value);
}

PVOID HalpMapSharedChannel(ULONGLONG ChannelSize)
{
    ULONG Device;

    if (ChannelSize != TREAS_SHARED_CHANNEL_SIZE ||
        (HAL_PCI_SHARED_BASE & (ChannelSize - 1)) != 0) {
        return 0;
    }

    for (Device = 0; Device < 32; Device++) {
        ULONG Function;

        for (Function = 0; Function < 8; Function++) {
            ULONG Identifier = HalpReadPciConfig(Device, Function, 0);

            if (Identifier != HAL_PCI_IVSHMEM_IDENTIFIER) {
                continue;
            }

            HalpWritePciConfig(Device, Function,
                               0x10 + HAL_PCI_SHARED_BAR_INDEX * 4,
                               HAL_PCI_SHARED_BASE | 0x0C);
            HalpWritePciConfig(Device, Function,
                               0x10 + (HAL_PCI_SHARED_BAR_INDEX + 1) * 4,
                               0);
            HalpWritePciConfig(Device, Function, 0x04,
                               HalpReadPciConfig(Device, Function, 0x04) |
                                   HAL_PCI_COMMAND_MEMORY_SPACE);
            return (PVOID)(ULONG_PTR)HAL_PCI_SHARED_BASE;
        }
    }

    return 0;
}
