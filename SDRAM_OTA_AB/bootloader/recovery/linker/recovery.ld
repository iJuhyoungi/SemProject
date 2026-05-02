ENTRY(Reset_Handler)

/*
* Recovery application linker script.                                                                                                                                                   
*                                                        
* 메모리 맵:
*   0x603E0000 ~ 0x603F0000  RECOVERY (이 binary, 64 KB)
*   0x603F0000 ~ 0x60400000  boot_ctrl metadata                                                                                                                                         
*
* Stage 1 이 Stage 2 검증 fail 시 0x603E0000 으로 점프.                                                                                                                                 
* Recovery 는 자체 isr_vector + Reset_Handler + main 가짐.                                                                                                                              
*/


MEMORY
{
    FLASH       (rx)    : ORIGIN = 0x603E0000, LENGTH = 0x00010000
    ITCM        (rwx)   : ORIGIN = 0x00000000, LENGTH = 0x00010000
    DTCM        (rwx)   : ORIGIN = 0x20000000, LENGTH = 0x00010000
}

__STACK_SIZE = 0x00001000;

SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.boot_text*)
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /*                                                                                                                                                                                   
    * recovery 도 startup 의 IS_BOOTLOADER 분기 활용 (UART init + .data/.ramfunc reloc).
    * .ramfunc 는 사용 안 하지만 startup 의 reloc 코드를 위해 빈 section 정의 필요.                                                                                                     
    */

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
    } > DTCM AT > FLASH
    __data_load_start__ = LOADADDR(.data);

    .bss(NOLOAD) :
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