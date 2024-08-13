OUTPUT_FORMAT("binary")
OUTPUT_ARCH(i386)

MEMORY
{
        DATA (r) : ORIGIN = 0x0, LENGTH = 64K-16
        CODE (rx) : ORIGIN = 0x0, LENGTH = 64K-16
        TEXT_HEAD (rx) : ORIGIN = 64K-16, LENGTH = 16

        ROM (r) : ORIGIN = 0x0, LENGTH = 64K-16
        ROM_HEAD (r) : ORIGIN = 64K-16, LENGTH = 16

}

SECTIONS{
        . = 0;
        .text : { *(.text) . = ALIGN(16);  __text_end = .; } > CODE AT > ROM

        __dataseg_rom_start = LOADADDR(.data);
        .data : { *(.data) } > DATA AT > ROM
        .rodata : { *(.rodata .rodata.*); __dataseg_contents_size = .; } > DATA  AT > ROM
        .bss : {
             __bss_start = .;
             *(.bss) *(COMMON) . = ALIGN(16);
             __dataseg_size = .;
             __bss_size = . - __bss_start;
        } > DATA  AT > ROM

        . = 0x10000 - 16;
        .text.head : { *(.text.head) } > TEXT_HEAD AT > ROM_HEAD
        . = 0x10000;
        _end = .;

        /DISCARD/ : { *(.note.*) *(.eh_frame) }
}
