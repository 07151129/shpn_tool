#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>

#include "defs.h"
#include "embed.h"
#include "strtab.h"

/**
 * TODO: Embedding strtab, script, fonts, patching addresses, checksums etc...
 */

void strtab_embed_ctx_free(struct strtab_embed_ctx* ctx) {
    for (size_t i = 0; i < ctx->nstrs; i++)
        if (ctx->strs[i] && ctx->allocated[i])
            free(ctx->strs[i]);
    free(ctx);
}

static bool ctx_conv(iconv_t conv, struct strtab_embed_ctx* ctx) {
    for (size_t i = 0; i < ctx->nstrs; i++) {
        assert(ctx->strs[i]);

        if (ctx->allocated[i]) {
            char* res = mk_strtab_str(ctx->strs[i], conv);
            if (!res)
                return false;
            free(ctx->strs[i]);
            ctx->strs[i] = res;
        }
    }
    return true;
}

size_t strtab_embed_min_rom_sz() {
    return STRTAB_SCRIPT_SZ + STRTAB_MENU_SZ;
}

iconv_t conv_for_embedding() {
    iconv_t ret = (iconv_t)-1;
#ifdef HAS_ICONV
    ret = iconv_open("SJIS", "UTF-8");
#endif
    if (ret == (iconv_t)-1)
        perror("iconv");
    return ret;
}

bool embed_strtab(uint8_t* rom, size_t rom_sz, struct strtab_embed_ctx* ectx, size_t max_sz,
    iconv_t conv) {
    assert(HAS_ICONV && conv != (iconv_t)-1);

    size_t space_left;
    if (max_sz > 0)
        space_left = max_sz;
    else
        space_left = rom_sz >= VMA2OFFS(ectx->rom_vma) ? rom_sz - VMA2OFFS(ectx->rom_vma) : 0;

    if (space_left < STRTAB_MENU_SZ) {
        fprintf(stderr, "ROM too small for the table (%zu < %zu)\n", space_left,
            strtab_embed_min_rom_sz());
        return false;
    }

    if (!ctx_conv(conv, ectx))
        return false;

    size_t nwritten;
    if (!make_strtab((void*)ectx->strs, ectx->nstrs, &rom[VMA2OFFS(ectx->rom_vma)], space_left,
        &nwritten))
        return false;

    return true;
}

bool embed_strtabs(uint8_t* rom, size_t rom_sz, struct strtab_embed_ctx* ectx_script,
    struct strtab_embed_ctx* ectx_menu, iconv_t conv) {
    assert(HAS_ICONV && conv != (iconv_t)-1);
    assert(ectx_menu->rom_vma > ectx_script->rom_vma);

    /**
     * FIXME: Maximum sizes hardcoded for now. If strtab doesn't fit in the ROM, we should
     * try to embed strtab at some other location and patch pointers to it instead of aborting.
     */
    if (!embed_strtab(rom, rom_sz, ectx_script, STRTAB_SCRIPT_SZ, conv) ||
        !embed_strtab(rom, rom_sz, ectx_menu, STRTAB_MENU_SZ, conv))
        return false;
    return true;
}

struct strtab_embed_ctx* strtab_embed_ctx_new() {
    struct strtab_embed_ctx* ret = malloc(sizeof(struct strtab_embed_ctx));
    if (!ret) {
        perror("malloc");
        return NULL;
    }
    memset(ret->allocated, '\0', sizeof(ret->allocated));
    return ret;
}

struct strtab_embed_ctx* strtab_embed_ctx_with_file(FILE* fin, size_t sz) {
    if (!fin)
        return NULL;

    rewind(fin);

    char* fbuf = malloc(sz);
    if (!fbuf) {
        perror("malloc");
        return NULL;
    }

    if (fread(fbuf, 1, sz, fin) < (size_t)sz) {
        fprintf(stderr, "Failed to fread (error %d)\n", ferror(fin));
        free(fbuf);
        return NULL;
    }

    struct strtab_embed_ctx* ret = strtab_embed_ctx_new();
    if (!ret) {
        free(fbuf);
        return NULL;
    }

    bool at_nl = true;
    size_t line = 0;
    uint32_t sidx = 0;
    size_t cidx = 0;
    ret->nstrs = 0;

    for (size_t i = 0; i < sz;) {
        if (at_nl) {
            char* end;
            sidx = strtoul(&fbuf[i], &end, 0);
            if (sidx >= EMBED_STRTAB_SZ) {
                fprintf(stderr, "Index %u exceeds strtab limit %d\n", sidx, EMBED_STRTAB_SZ);
                ret = NULL;
                free(ret);
                goto done;
            }

            if (*end != ':') {
                fprintf(stderr, "Failed to parse input at line %zu\n", line);
                free(ret);
                ret = NULL;
                goto done;
            }
            i += (size_t)(end - &fbuf[i] + 1);

            at_nl = false;
            for (cidx = i; fbuf[cidx] == ' '; cidx++)
                ;
        } else {
            if (fbuf[i] != '\n')
                i++;
            else {
                assert((size_t)i >= cidx);
                size_t len = i - cidx;
                ret->strs[sidx] = malloc(len + 1);
                if (!ret->strs[sidx]) {
                    perror("malloc");
                    free(ret);
                    ret = NULL;
                    goto done;
                }

                memcpy(ret->strs[sidx], &fbuf[cidx], len);
                ret->strs[sidx][len] = '\0';
                ret->nstrs = sidx > ret->nstrs ? sidx : ret->nstrs;
                ret->allocated[sidx] = true;

                i++;
                at_nl = true;
                line++;
            }
        }
    }

    /**
     * FIXME: We pretend to have a continuous array of strings by inserting placeholders, but it
     * would be much better if we could shuffle the actual strings. However, it would require us
     * to keep track of strings being relocated so that the script doesn't break, and is probably
     * not worth the effort, given the fact the original tables are full of placeholders anyway.
     *
     * Alternatively, we could pass struct strtab_embed_ctx to make_strtab
     */
    for (size_t i = 0; i < ret->nstrs; i++)
        if (!ret->allocated[i])
            ret->strs[i] = "";
    ret->nstrs++;

done:
    free(fbuf);
    return ret;
}
