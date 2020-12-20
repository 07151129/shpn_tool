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
    ROM_PATCH_MENU_ENTRY (rx) : ORIGIN = 0x8004F90, LENGTH = (0x8005134 - 0x8004F90)
    ROM_PATCH_CLEAR_OAM (rx) : ORIGIN = 0x8007200, LENGTH = (0x80072EC - 0x8007200)
    ROM_PATCH_RENDER_BACKLOG (rx) : ORIGIN = 0x800774C, LENGTH = (0x8007B90 - 0x800774C)
    ROM_PATCH_RENDER_LOAD_MENU (rx) : ORIGIN = 0x800A190, LENGTH = (0x800A39A - 0x800A190)
    ROM_PATCH (rx) : ORIGIN = 0x87962EC, LENGTH = 4K
}

SECTIONS {
    .entry : {
        KEEP(*(.entry))
    } > ROM_PATCH_ENTRY = 0xFF

    .entry_menu : {
        KEEP(*(.entry_menu))
    } > ROM_PATCH_MENU_ENTRY = 0xFF

    .clear_oam : {
        KEEP(*(.clear_oam))
    } > ROM_PATCH_CLEAR_OAM = 0xFF

    .render_backlog_controls : {
        KEEP(*(.render_backlog_controls))
    } > ROM_PATCH_RENDER_BACKLOG = 0xFF

    .render_load_menu : {
        KEEP(*(.render_load_menu))
    } > ROM_PATCH_RENDER_LOAD_MENU = 0xFF

    .text : {
        /* *(.init) */
        *(.text)
        KEEP(*(.text))

        /* *(.ctors) */
        /* *(.dtors) */
        *(.rodata)
        *(.rodata.cst16)
        /* *(.fini) */
        /* *(.data) */
        *(COMMON)
        *(.bss)
    } > ROM_PATCH = 0xFF

    /DISCARD/ : {
        *(*)
    }
}
