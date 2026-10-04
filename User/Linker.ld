ENTRY(UserMain)

PHDRS
{
    Text PT_LOAD FILEHDR PHDRS FLAGS(5);
    Data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x100000000 + SIZEOF_HEADERS;
    .text : ALIGN(0x1000) { *(.text .text.*) *(.rodata .rodata.*) } :Text
    . = ALIGN(0x1000);
    .data : ALIGN(0x1000) { *(.data .data.*) *(.sdata .sdata.*) } :Data
    .bss (NOLOAD) : ALIGN(0x1000) {
        *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON)
    } :Data

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
