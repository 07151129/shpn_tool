MEMORY {
    BIOS (rx) : ORIGIN = 0x00000000, LENGTH = 16K
    EWRAM (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
    IWRAM (rwx) : ORIGIN = 0x03000000, LENGTH = 32K
    IO (rw) : ORIGIN = 0x04000000, LENGTH = 1K - 1
    PAL (rw) : ORIGIN = 0x05000000, LENGTH = 1K
    VRAM (rw) : ORIGIN = 0x06000000, LENGTH = 96K
    OAM (rw) : ORIGIN = 0x07000000, LENGTH = 1K
    ROM (rx) : ORIGIN = 0x8000000, LENGTH = 16M
    ROM_PATCH_ENTRY (rx) : ORIGIN = 0x8004DA4, LENGTH = (0x8004F70 - 0x8004DA4)
    ROM_PATCH (rx) : ORIGIN = 0x87962EC, LENGTH = (0x87962EC - 0x87A0000)
}

SECTIONS {
    .entry : {
        KEEP(*(.entry))
    } > ROM_PATCH_ENTRY = 0xFF

    .text : {
        *(.init)
        *(.text)
        KEEP(*(.text))

        *(.ctors)
        *(.dtors)
        *(.rodata)
        *(.fini)
        *(.data)
        *(COMMON)
        *(.bss)
    } > ROM_PATCH = 0xFF

    /DISCARD/ : {
        *(*)
    }
}
