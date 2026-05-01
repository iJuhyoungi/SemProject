ENTRY(Reset_Handler)

/*
 * Stage 1 immutable bootloader linker script.
 *
 * Phase 1 의 stand-alone 빌드용 — 아직 IVT 가 Stage 1 을 가리키지 않음.
 * Phase 2 에서 IVT entry 를 Stage 1 reset handler 로 변경하면서 활성화.
 *
 * 메모리 맵 (B-3 분리 후):
 *   0x60000000 ~ 0x60001000  FCB        (현재 bootloader/ 가 정의)
 *   0x60001000 ~ 0x60002000  IVT        (현재 bootloader/ 가 정의)
 *   0x60002000 ~ 0x60004000  STAGE 1    (이 binary, 8KB)
 *   0x60004000 ~ 0x60040000  STAGE 2    (Phase 2 에서 bootloader/ LMA shift)
 */

MEMORY
{
    FLASH        (rx) : ORIGIN = 0x60002000, LENGTH = 0x00002000
    ITCM         (rwx): ORIGIN = 0x00000000, LENGTH = 0x00010000
    DTCM         (rwx): ORIGIN = 0x20000000, LENGTH = 0x00010000
}

__STACK_SIZE = 0x00000800;   /* 2KB stack — Stage 1 동작 매우 단순 */

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
