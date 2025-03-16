ENTRY(_start)

MEMORY
{
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
    } > RAM

    .data : {
        _data_start = .;
        *(.data*)
        _data_end = .;
    } > RAM

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    .stack : {
        . = ALIGN(8);
        . = . + 0x1000; /* 4KB 的栈空间 */
        _stack_top = .;
    } > RAM
} 