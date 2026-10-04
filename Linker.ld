ENTRY(KiStart)
SECTIONS
{
    . = 1M;
    .note.Xen ALIGN(4) : { KEEP(*(.note.Xen)) }
    .text ALIGN(4K) : { *(.text.boot) *(.text .text.*) }
    .rodata ALIGN(4K) : { *(.rodata .rodata.*) }
    .data ALIGN(4K) : { *(.data.boot) *(.data .data.*) }
    .bss ALIGN(4K) : { *(COMMON) *(.bss .bss.*) }
    . = ALIGN(4K);
    KiKernelImageEnd = .;
    /DISCARD/ : { *(.comment) *(.eh_frame*) }
}
