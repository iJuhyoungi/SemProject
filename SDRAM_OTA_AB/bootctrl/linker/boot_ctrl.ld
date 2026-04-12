MEMORY
{
    BOOT_CTRL (rx) : ORIGIN = 0x603F0000, LENGTH = 0x00001000
}

SECTIONS
{
    .boot_ctrl :
    {
        . = ALIGN(4);
        KEEP(*(.boot_ctrl))
        . = ALIGN(4);
    } > BOOT_CTRL
}

