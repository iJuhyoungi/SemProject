ENTRY(Reset_Handler)

MEMORY
{
    FLASH        (rx) : ORIGIN = 0x60004000, LENGTH = 0x0003C000
    ITCM         (rwx): ORIGIN = 0x00000000, LENGTH = 0x00010000
    DTCM         (rwx): ORIGIN = 0x20000000, LENGTH = 0x00010000

}

__STACK_SIZE = 0x00002000;

SECTIONS
{
    /DISCARD/ :
    {
        *(.ivt)
        *(.boot_data)
        *(.dcd_data)
    }
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH

    /*  부트로더는 코드가 작으므로 SDRAM으로 옮기지 않고 플래시에서 직접 실행(XIP)합니다! */
    .text :
    {
        . = ALIGN(4);
        *(.boot_text*)
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /*
     * qspi_flash_config 의 dead data 사본. Stage 1 의 0x60000000 FCB 와는 별도.
     * BootCtrl_CopyConfigToRam 이 link 시점에 이 주소를 reference 하여
     * RAM 으로 copy 후 사용. RT1020 boot ROM 은 Stage 1 의 FCB 만 보니 dup 무해.
     */
    
    .flash_config_dup : { *(.flash_config) } > FLASH

    /* : ROM API 호출 코드를 ITCM 으로 옮겨 flash erase/program 중 안전 보장 */
    .ramfunc :
    {
        . = ALIGN(4);
        __ramfunc_start__ = .;
        *(.ramfunc*)
        . = ALIGN(4);
        __ramfunc_end__ = .;
    } > ITCM AT > FLASH
    __ramfunc_load_start__ = LOADADDR(.ramfunc);

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