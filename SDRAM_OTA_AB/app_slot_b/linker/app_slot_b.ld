ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x60200000, LENGTH = 0x001C0000

    ITCM  (rwx) : ORIGIN = 0x00000000, LENGTH = 0x00010000
    DTCM  (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00010000

    SDRAM (rwx) : ORIGIN = 0x80000000, LENGTH = 0x01E00000
}

__STACK_SIZE = 0x00002000;

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .boot_text :
    {
        . = ALIGN(4);
        *(.boot_text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        __text_start__ = .;
        *(.text*)
        . = ALIGN(4);
        __text_end__ = .;
    } > SDRAM AT> FLASH
    __text_load_start__ = LOADADDR(.text);

    .ramfunc :
    {
        . = ALIGN(4);
        __ramfunc_start__ = .;
        *(.ramfunc*)
        . = ALIGN(4);
        __ramfunc_end__ = .;
    } > ITCM AT> FLASH
    __ramfunc_load_start__ = LOADADDR(.ramfunc);

    .data :
    {
        . = ALIGN(4);
        __data_start__ = .;
        *(.data*)
        . = ALIGN(4);
        __data_end__ = .;
    } > SDRAM AT> FLASH
    __data_load_start__ = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(8);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end__ = .;
    } > SDRAM

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __StackLimit = .;
        . += __STACK_SIZE;
        __StackTop = .;
    } > DTCM
}