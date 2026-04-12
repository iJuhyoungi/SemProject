ENTRY(Reset_Handler)

MEMORY
{
    FLASH_CONFIG (rx) : ORIGIN = 0x60000000, LENGTH = 0x00001000
    BOOT_HDR     (rx) : ORIGIN = 0x60001000, LENGTH = 0x00001000
    FLASH        (rx) : ORIGIN = 0x60002000, LENGTH = 0x007FE000

    ITCM         (rwx): ORIGIN = 0x00000000, LENGTH = 0x00010000
    DTCM         (rwx): ORIGIN = 0x20000000, LENGTH = 0x00010000
    OCRAM        (rwx): ORIGIN = 0x20200000, LENGTH = 0x00020000
}

__STACK_SIZE = 0x00002000;
__HEAP_SIZE  = 0x00002000;

SECTIONS
{
    .flash_config :
    {
        . = ALIGN(4);
        KEEP(*(.flash_config))
        . = ALIGN(4);
    } > FLASH_CONFIG

    .boot_header :
    {
        . = ALIGN(4);
        KEEP(*(.ivt))
        KEEP(*(.boot_data))
        KEEP(*(.dcd_data))
        . = ALIGN(4);
    } > BOOT_HDR

    .text :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame*)
        . = ALIGN(4);
        __etext = .;
    } > FLASH

    .data :
    {
        . = ALIGN(4);
        __data_start__ = .;
        *(.data*)
        . = ALIGN(4);
        __data_end__ = .;
    } > DTCM AT> FLASH
    
    __data_load_start__ = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(8);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end__ = .;
    } > DTCM

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __StackLimit = .;
        . += __STACK_SIZE;
        __StackTop = .;
    } > DTCM

    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        __HeapBase = .;
        . += __HEAP_SIZE;
        __HeapLimit = .;
    } > OCRAM

    /DISCARD/ :
    {
        *(.note*)
        *(.comment)
    }

    .ramfunc :
    {
        . = ALIGN(4);
        __ramfunc_start__ = .;
        *(.ramfunc*)
        . = ALIGN(4);
        __ramfunc_end__ = .;
    } > ITCM AT> FLASH

    __ramfunc_load_start__ = LOADADDR(.ramfunc);
    
}
