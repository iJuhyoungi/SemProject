ENTRY(Reset_Handler)

/*
 * Stage 1 immutable bootloader — Secure Boot.
 *
 * 메모리 맵 (2-stage):
 *   0x60000000 ~ 0x60001000  FCB        (4KB,  FlexSPI config block)
 *   0x60001000 ~ 0x60002000  IVT        (4KB,  boot header)
 *   0x60002000 ~ 0x60008000  STAGE 1    (24KB, 이 binary)
 *   0x60008000 ~ 0x60048000  STAGE 2    (256KB, 서명된 image)
 *
 * OTA_AB 대비 변경: Stage 1 영역 8KB -> 24KB. SHA-256 + RSA-2048 서명
 * 검증 코드와 임베드 public key 가 8KB 에 안 들어가므로 확대.
 */

MEMORY
{
    FLASH_CONFIG    (rx) : ORIGIN = 0x60000000, LENGTH = 0x00001000
    BOOT_HDR        (rx) : ORIGIN = 0x60001000, LENGTH = 0x00001000
    FLASH           (rx) : ORIGIN = 0x60002000, LENGTH = 0x00006000
    ITCM            (rwx): ORIGIN = 0x00000000, LENGTH = 0x00010000
    DTCM            (rwx): ORIGIN = 0x20000000, LENGTH = 0x00010000
}

/*
 * 8KB stack — OTA_AB Stage 1 은 2KB 였으나, RSA-2048 modular exponentiation
 * 의 big-number 중간값(2048-bit = 256B 단위)을 스택에 둘 여지를 확보.
 * Step 2 에서 RSA 구현 시 부족하면 재조정.
 */
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
     * Stage 1 자체는 ramfunc 안 씀 (flash write 안 함). 단 startup_MIMXRT1020.c
     * 의 IS_BOOTLOADER 분기가 .ramfunc reloc 코드를 실행하므로 빈 section 이라도
     * 정의 필요. shared/src/bootctrl_rom_api.c 의 .ramfunc 함수들은 Stage 1 의
     * main 에서 호출 안 되므로 --gc-sections 로 제거됨.
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
