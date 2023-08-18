SECTIONS
{
    .text 0x0 : { *(.text) } = 0

    .rodata : { *(.rodata) }

    .data : { *(.data) }

    .bss : { *(.bss) }

    end = ALIGN(4);
}
