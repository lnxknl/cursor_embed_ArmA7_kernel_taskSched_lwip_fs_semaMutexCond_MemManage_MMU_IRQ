ENTRY(_start)

MEMORY
{
    ROM (rx)  : ORIGIN = 0x60000000, LENGTH = 1M
    RAM (rwx) : ORIGIN = 0x70000000, LENGTH = 128M
}

SECTIONS
{
    .vectors : {
        *(.vectors)
    } > ROM

    .text : {
        _text_start = .;
        *(.text*)
        *(.rodata*)
        _text_end = .;
        . = ALIGN(4);
    } > ROM

    .data : AT(ADDR(.text) + SIZEOF(.text)) {
        _data_start = .;
        *(.data*)
        _data_end = .;
        . = ALIGN(4);
    } > RAM

    _data_loadaddr = LOADADDR(.data);

    .bss : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
        . = ALIGN(4);
    } > RAM

    .stack (NOLOAD) : {
        . = ALIGN(8);
        . = . + 0x1000; /* 4KB stack */
        _stack_top = .;
    } > RAM
} 