#ifndef EMBED_H
#define EMBED_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "defs.h"

#define EMBED_STRTAB_SZ 10000

/**
 * Statically allocate placeholder idx such that idx % 10 == 0.
 * This is used for Choice handler to determine whether the 0th row should be selected by default.
 * Placeholder users must use EMBED_STR_PLACEHOLDER_IDX to get a guaranteed placeholder idx.
 * This value is also used to fill gaps in strtab.
 */
#define EMBED_STR_PLACEHOLDER ""
#define EMBED_STR_PLACEHOLDER_IDX 0

struct strtab_embed_ctx {
    enum {STRTAB_ENC_UTF8, STRTAB_ENC_SJIS} enc;
    bool wrapped;
    size_t nstrs; /* in total incl. placeholders */
    char* strs[EMBED_STRTAB_SZ];
    uint32_t rom_vma;
    struct {
        bool allocated; /* must be freed */
        bool used; /* is referenced */
    } allocated[EMBED_STRTAB_SZ];
};

#define STRTAB_SCRIPT_PTR_VMA 0x8004B9C
#define STRTAB_MENU_PTR_VMA 0x8004C24

iconv_t conv_for_embedding();
bool embed_strtab(uint8_t* rom, size_t rom_sz, struct strtab_embed_ctx* ectx, size_t max_sz,
    uint32_t ptr_vma, iconv_t conv);
bool embed_strtabs(uint8_t* rom, size_t rom_sz, struct strtab_embed_ctx* ectx_script,
    struct strtab_embed_ctx* ectx_menu, size_t strtab_script_sz, size_t strtab_menu_sz,
    iconv_t conv);
bool strtab_embed_ctx_with_file(FILE* fin, size_t sz, struct strtab_embed_ctx* ectx);
struct strtab_embed_ctx* strtab_embed_ctx_new();
void strtab_embed_ctx_free(struct strtab_embed_ctx* ctx);
size_t strtab_embed_min_rom_sz();

bool embed_script(uint8_t* rom, size_t rom_sz, size_t script_sz_max, size_t script_offs,
        bool use_rom_strtab,
        FILE* fscript, FILE* strtab_scr, FILE* strtab_menu,
        const char* script_path,
        size_t script_fsz, size_t strtab_scr_fsz, size_t strtab_menu_fsz,
        uint32_t strtab_scr_vma, uint32_t strtab_menu_vma,
        uint32_t strtab_scr_sz, uint32_t strtab_menu_sz,
        uint32_t sz_to_patch_vma, uint32_t script_ptr_vma);

#endif
