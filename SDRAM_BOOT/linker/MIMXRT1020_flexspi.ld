ENTRY(Reset_Handler)

MEMORY
{
    FLASH_CONFIG (rx) : ORIGIN = 0x60000000, LENGTH = 0x00001000
    BOOT_HDR     (rx) : ORIGIN = 0x60001000, LENGTH = 0x00001000
    FLASH        (rx) : ORIGIN = 0x60002000, LENGTH = 0x007FE000

    ITCM         (rwx): ORIGIN = 0x00000000, LENGTH = 0x00010000
    DTCM         (rwx): ORIGIN = 0x20000000, LENGTH = 0x00010000
    OCRAM        (rwx): ORIGIN = 0x20200000, LENGTH = 0x00020000
    /* 🚀 32MB SDRAM 영토 공식 선포! */
    SDRAM        (rwx): ORIGIN = 0x80000000, LENGTH = 0x02000000 
}

__STACK_SIZE = 0x00002000;
__HEAP_SIZE  = 0x01000000; /* 16MB Heap */

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

    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* 🚀 수정 1: .boot_text 섹션에 .rodata 추가 */
    .boot_text :
    {
        . = ALIGN(4);
        *(.boot_text*)
        *(.text.Reset_Handler)
        *(.text.SystemInit)
        *(.text.Default_Handler)
        *(.rodata*)   /* 🚀 핵심: 모든 문자열과 상수를 플래시에 고정! */
        . = ALIGN(4);
    } > FLASH

    /* 🚀 수정 2: .text 섹션에서 .rodata 제거 */
    .text :
    {
        . = ALIGN(4);
        __text_start__ = .;
        *(.text*)
        /* *(.rodata*)  <-- 🚨 여기서 이 줄을 완전히 삭제하십시오! */
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame*)
        . = ALIGN(4);
        __text_end__ = .;
    } > SDRAM AT> FLASH
    
    __text_load_start__ = LOADADDR(.text);

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

    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        __HeapBase = .;
        . += __HEAP_SIZE;
        __HeapLimit = .;
    } > SDRAM

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __StackLimit = .;
        . += __STACK_SIZE;
        __StackTop = .;
    } > DTCM

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
