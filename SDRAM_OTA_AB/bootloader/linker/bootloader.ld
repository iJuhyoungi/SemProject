ENTRY(Reset_Handler)

MEMORY
{
    FLASH_CONFIG (rx) : ORIGIN = 0x60000000, LENGTH = 0x00001000
    BOOT_HDR     (rx) : ORIGIN = 0x60001000, LENGTH = 0x00001000
    FLASH        (rx) : ORIGIN = 0x60002000, LENGTH = 0x0003E000
    DTCM         (rwx): ORIGIN = 0x20000000, LENGTH = 0x00010000

}

__STACK_SIZE = 0x00002000;

SECTIONS
{
    .flash_config : { KEEP(*(.flash_config)) } > FLASH_CONFIG
    .boot_header : { KEEP(*(.ivt)) KEEP(*(.boot_data)) KEEP(*(.dcd_data)) } > BOOT_HDR
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH

    /* 🚀 부트로더는 코드가 작으므로 SDRAM으로 옮기지 않고 플래시에서 직접 실행(XIP)합니다! */
    .text :
    {
        . = ALIGN(4);
        *(.boot_text*)
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* 변수들은 코어 내부 램(DTCM)을 사용합니다 */
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