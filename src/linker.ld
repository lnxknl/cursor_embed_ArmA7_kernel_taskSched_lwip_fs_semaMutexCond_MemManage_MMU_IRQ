ENTRY(_start)

MEMORY
{
    ROM (rx)  : ORIGIN = 0x60000000, LENGTH = 1M
    RAM (rwx) : ORIGIN = 0x70000000, LENGTH = 128M
}

SECTIONS
{
    .text : {
        _text_start = .;
        *(.vectors)
        *(.text*)
        *(.rodata*)
        _text_end = .;
    } > ROM

    .data : AT(ADDR(.text) + SIZEOF(.text)) {
        _data_start = .;
        *(.data*)
        _data_end = .;
    } > RAM

    .bss : {
        . = ALIGN(8);
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } > RAM

    .stack : {
        . = ALIGN(8);
        . = . + 0x1000; /* 4KB stack */
        _stack_top = .;
    } > RAM

    _data_loadaddr = LOADADDR(.data);
} 