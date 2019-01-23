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

void strtab_embed_ctx_free(const struct strtab_embed_ctx* ctx) {
    for (size_t i = 0; i < ctx->nstrs; i++)
        if (ctx->strs[i] && ctx->allocated[i])
            free(ctx->strs[i]);
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

#define STRTAB_SCRIPT_SZ 0x36b64
#define STRTAB_MENU_SZ 0xcf0

bool embed_strtabs(uint8_t* rom, size_t rom_sz, struct strtab_embed_ctx* ectx_script,
    struct strtab_embed_ctx* ectx_menu, iconv_t conv) {
    assert(HAS_ICONV);
    assert(ectx_menu->rom_vma > ectx_script->rom_vma);

    if (rom_sz - VMA2OFFS(ectx_script->rom_vma) < (size_t)(STRTAB_MENU_SZ + STRTAB_SCRIPT_SZ)) {
        fprintf(stderr, "ROM too small for the tables\n");
        return false;
    }

    bool had_iconv = conv != (iconv_t)-1;
#ifdef HAS_ICONV
    if (!had_iconv)
        conv = iconv_open("SJIS", "UTF-8");
#endif
    if (conv == (iconv_t)-1) {
        perror("iconv");
        return false;
    }

    if (!ctx_conv(conv, ectx_script) || !ctx_conv(conv, ectx_menu))
        return false;

    bool ret = true;
    uint8_t* strtab_script = malloc(STRTAB_SCRIPT_SZ);
    if (!strtab_script) {
        perror("malloc");
        ret = false;
        goto fail_script;
    }

    uint8_t* strtab_menu = malloc(STRTAB_MENU_SZ);
    if (!strtab_menu) {
        perror("malloc");
        ret = false;
        goto fail_menu;
    }

    size_t nwritten_script, nwritten_menu;
    if (!make_strtab((void*)ectx_script->strs, ectx_script->nstrs, strtab_script, STRTAB_SCRIPT_SZ,
        &nwritten_script)) {
        fprintf(stderr, "Failed to make script strtab\n");
        ret = false;
        goto fail;
    }

    if (!make_strtab((void*)ectx_menu->strs, ectx_menu->nstrs, strtab_menu, STRTAB_MENU_SZ,
        &nwritten_menu)) {
        fprintf(stderr, "Failed to make menu strtab\n");
        ret = false;
        goto fail;
    }

    memcpy(&rom[VMA2OFFS(ectx_script->rom_vma)], strtab_script, nwritten_script);
    memcpy(&rom[VMA2OFFS(ectx_menu->rom_vma)], strtab_menu, nwritten_menu);

fail:
    free(strtab_menu);
fail_menu:
    free(strtab_script);
fail_script:
#ifdef HAS_ICONV
    if (!had_iconv)
        iconv_close(conv);
#endif
    return ret;
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

struct strtab_embed_ctx* strtab_embed_ctx_with_file(const char* path) {
    if (!path)
        return NULL;

    struct stat st;
    if (stat(path, &st) == -1) {
        perror("stat");
        return NULL;
    }

    char* fbuf = malloc(st.st_size);
    if (!fbuf) {
        perror("malloc");
        return NULL;
    }

    {
        FILE* f = fopen(path, "rb");
        if (!f) {
            perror("fopen");
            free(fbuf);
            return NULL;
        }

        if (fread(fbuf, 1, st.st_size, f) < (size_t)st.st_size) {
            fprintf(stderr, "Failed to fread %s (error %d)\n", path, ferror(f));
            fclose(f);
            free(fbuf);
            return NULL;
        }
        fclose(f);
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

    for (off_t i = 0; i < st.st_size;) {
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
                fprintf(stderr, "Failed to parse %s at line %zu\n", path, line);
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
