ENTRY(Reset_Handler)

/*
* App A — Stage 2 가 검증(RSA-2048)하고 점프하는 대상.
* Stage 2 와 동일한 image 형식: [code + 256B signature].
*
*   0x60048000 ~ 0x60088000  App A (256KB)
*/

MEMORY
{ 
    FLASH (rx) : ORIGIN = 0x60048000, LENGTH = 0x00040000
    ITCM  (rwx): ORIGIN = 0x00000000, LENGTH = 0x00010000
    DTCM  (rwx): ORIGIN = 0x20000000, LENGTH = 0x00010000
}

__STACK_SIZE = 0x00002000;

SECTIONS
{ 
    /DISCARD/ :
    {
        *(.ivt)
        *(.boot_data)
        *(.dcd_data)
        *(.flash_config)
    }

    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.boot_text*)
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .ramfunc : 
    {
        . = ALIGN(4);
        __ramfunc_start__ = .;
        *(.ramfunc*)
        . = ALIGN(4);
        __ramfunc_end__ = .;
    } > ITCM AT > FLASH
    __ramfunc_load_start__ = LOADADDR(.ramfunc);

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
}
