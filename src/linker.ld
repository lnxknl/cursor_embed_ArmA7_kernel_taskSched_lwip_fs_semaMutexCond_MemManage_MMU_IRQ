ENTRY(_start)

MEMORY
{
    ROM (rx)  : ORIGIN = 0x60000000, LENGTH = 1M
    RAM (rwx) : ORIGIN = 0x70000000, LENGTH = 128M
    UNCACHED (rw) : ORIGIN = 0x80000000, LENGTH = 1M
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

    .data : {
        _data_start = .;
        *(.data*)
        _data_end = .;
        . = ALIGN(4);
    } > RAM AT > ROM

    .uncached_data : {
        _uncached_start = .;
        *(.uncached_data*)
        _uncached_end = .;
        . = ALIGN(4);
    } > UNCACHED AT > ROM

    .bss : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
        . = ALIGN(4);
    } > RAM

    .stack : {
        . = ALIGN(8);
        . = . + 0x1000;
        _stack_top = .;
    } > RAM
} 