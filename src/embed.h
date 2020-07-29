#ifndef EMBED_H
#define EMBED_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "defs.h"

#define EMBED_STRTAB_SZ 10000
#define EMBED_STR_PLACEHOLDER ""
struct strtab_embed_ctx {
    enum {STRTAB_ENC_UTF8, STRTAB_ENC_SJIS} enc;
    bool wrapped;
    size_t nstrs; /* in total incl. placeholders */
    char* strs[EMBED_STRTAB_SZ];
    uint32_t rom_vma;
    bool allocated[EMBED_STRTAB_SZ];
};

#define STRTAB_SCRIPT_PTR_VMA 0x8004B9C
#define STRTAB_MENU_PTR_VMA 0x8004C24

iconv_t conv_for_embedding();
bool embed_strtab(uint8_t* rom, size_t rom_sz, struct strtab_embed_ctx* ectx, size_t max_sz,
    uint32_t ptr_vma, iconv_t conv);
bool embed_strtabs(uint8_t* rom, size_t rom_sz, struct strtab_embed_ctx* ectx_script,
    struct strtab_embed_ctx* ectx_menu, size_t strtab_script_sz, size_t strtab_menu_sz,
    iconv_t conv);
struct strtab_embed_ctx* strtab_embed_ctx_with_file(FILE* fin, size_t sz);
void strtab_embed_ctx_free(struct strtab_embed_ctx* ctx);
size_t strtab_embed_min_rom_sz();

bool embed_script(uint8_t* rom, size_t rom_sz, size_t script_sz_max, size_t script_offs,
        FILE* fscript, FILE* strtab_scr, FILE* strtab_menu,
        const char* script_path,
        size_t script_fsz, size_t strtab_scr_fsz, size_t strtab_menu_fsz,
        uint32_t strtab_scr_vma, uint32_t strtab_menu_vma,
        uint32_t strtab_scr_sz, uint32_t strtab_menu_sz,
        uint32_t sz_to_patch_vma, uint32_t script_ptr_vma);

#endif
