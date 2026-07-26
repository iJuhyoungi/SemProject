ENTRY(Reset_Handler)

/*
  * RT1020 Peripheral 단일 이미지 — flash XIP.
  *
  * 메모리 맵 (single image):
  *   0x60000000 ~ 0x60001000  FCB     (4KB,  FlexSPI config block)
  *   0x60001000 ~ 0x60002000  IVT     (4KB,  boot header)
  *   0x60002000 ~ 0x60800000  FLASH   (~8MB, 이 binary 전체 .text/.rodata)
  *   0x00000000 ~ 0x00010000  ITCM    (64KB)
  *   0x20000000 ~ 0x20010000  DTCM    (64KB, .data/.bss/stack)
  *   
  * stage1.ld 대비: 2-stage 분할/STAGE2 제거. FLASH를 flash 끝까지 단일 확장.
  */

MEMORY
{
    FLASH_CONFIG    (rx) : ORIGIN = 0x60000000, LENGTH = 0x00001000
    BOOT_HDR        (rx) : ORIGIN = 0x60001000, LENGTH = 0x00001000
    FLASH           (rx) : ORIGIN = 0x60002000, LENGTH = 0x007FE000
    ITCM            (rwx): ORIGIN = 0x00000000, LENGTH = 0x00010000
    DTCM            (rwx): ORIGIN = 0x20000000, LENGTH = 0x00010000
}

__STACK_SIZE = 0x00002000;

SECTIONS
{
    .flash_config   : { KEEP(*(.flash_config)) } > FLASH_CONFIG
    .boot_header    : { KEEP(*(.ivt)) KEEP(*(.boot_data)) KEEP(*(.dcd_data)) } > BOOT_HDR

    .isr_vector     : { KEEP(*(.isr_vector)) } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.boot_text*)
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /*
    * 이 이미지는 ramfunc 안 씀 (flash write 없음). 단 startup 의 IS_BOOTLOADER
    * 분기가 .ramfunc reloc 코드를 실행하므로, 빈 section 이라도 __ramfunc_* 심볼
    * 정의는 필요. 실제 들어오는 .ramfunc 함수는 --gc-sections 로 제거됨.
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