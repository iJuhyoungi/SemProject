ENTRY(Reset_Handler)

/*
 * Stage 2 — Secure Boot 의 검증 대상 image.
 * 호스트가 서명해 첨부하고, Stage 1 이 검증(CRC32 -> SHA-256 -> RSA-2048)
 * 통과 시 점프한다.
 *
 *   0x60008000 ~ 0x60048000  STAGE 2 (256KB)
 *
 * FCB/IVT 책임은 Stage 1 에 있다. shared/src 가 emit 하는 .ivt/.boot_data/
 * .dcd_data/.flash_config 는 Stage 2 binary 안에선 dead data 이므로 DISCARD.
 */

MEMORY
{
    FLASH (rx) : ORIGIN = 0x60008000, LENGTH = 0x00040000
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

    /* 코드가 작으므로 SDRAM reloc 없이 flash 에서 직접 실행(XIP). */
    .text :
    {
        . = ALIGN(4);
        *(.boot_text*)
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /*
     * demo payload 는 flash write 안 함 -> bootctrl_rom_api.c 의 .ramfunc 는
     * --gc-sections 로 제거되어 size 0. 단 startup 의 IS_BOOTLOADER 분기가
     * reloc 코드를 실행하므로 빈 section 이라도 정의 필요.
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
