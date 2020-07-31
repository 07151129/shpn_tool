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
#include "glyph.h"
#include "script_as.h"
#include "script_parse_ctx.h"
#include "strtab.h"

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
            if (!res) {
                fprintf(stderr, "failed to convert string at %zu\n", i);
                return false;
            }
            free(ctx->strs[i]);
            ctx->strs[i] = res;
        }
    }
    ctx->enc = STRTAB_ENC_SJIS;
    return true;
}

static void ctx_hard_wrap(struct strtab_embed_ctx* ctx) {
    for (size_t i = 0; i < ctx->nstrs; i++) {
        assert(ctx->strs[i]);

        if (ctx->allocated[i])
            hard_wrap_sjis(ctx->strs[i]);
    }
    ctx->wrapped = true;
}

iconv_t conv_for_embedding() {
    iconv_t ret = (iconv_t)-1;
#ifdef HAS_ICONV
    ret = iconv_open("SJIS", "UTF-8");
#endif
    if (ret == (iconv_t)-1)
        perror("iconv_open");
    return ret;
}

static bool patch_ptr(uint8_t* rom, size_t rom_sz, uint32_t repl_vma, uint32_t ptr_vma) {
    if (VMA2OFFS(ptr_vma) + sizeof(uint32_t) >= rom_sz) {
        fprintf(stderr, "ROM too small for patching at 0x%x\n", ptr_vma);
        return false;
    }
    *(uint32_t*)&rom[VMA2OFFS(ptr_vma)] = repl_vma;
    return true;
}

bool embed_strtab(uint8_t* rom, size_t rom_sz, struct strtab_embed_ctx* ectx, size_t max_sz,
    uint32_t ptr_vma, iconv_t conv) {
    assert(HAS_ICONV && conv != (iconv_t)-1);
    assert(max_sz + VMA2OFFS(ectx->rom_vma) <= rom_sz);

    /**
     * FIXME: Move length until newline check from ctx_conv to after ctx_hard_wrap, as the latter
     * creates more wraps.
     */
    if (ectx->enc != STRTAB_ENC_SJIS && !ctx_conv(conv, ectx))
        return false;

    if (!ectx->wrapped)
        ctx_hard_wrap(ectx);

    size_t nwritten;
    if (!make_strtab((void*)ectx->strs, ectx->nstrs, &rom[VMA2OFFS(ectx->rom_vma)], max_sz,
        &nwritten))
        return false;

    if (!patch_ptr(rom, rom_sz, ectx->rom_vma, ptr_vma))
        return false;

    fprintf(stderr, "Embedded strtab at 0x%x using %zu B\n", ectx->rom_vma, nwritten);

    return true;
}

bool embed_strtabs(uint8_t* rom, size_t rom_sz, struct strtab_embed_ctx* ectx_script,
    struct strtab_embed_ctx* ectx_menu, size_t strtab_script_sz, size_t strtab_menu_sz,
    iconv_t conv) {
    assert(HAS_ICONV && conv != (iconv_t)-1);

    if (!embed_strtab(rom, rom_sz, ectx_script, strtab_script_sz, STRTAB_SCRIPT_PTR_VMA, conv)) {
        fprintf(stderr, "Failed to embed script strtab\n");
        return false;
    }
    if (!embed_strtab(rom, rom_sz, ectx_menu, strtab_menu_sz, STRTAB_MENU_PTR_VMA, conv)) {
        fprintf(stderr, "Failed to embed menu strtab\n");
        return false;
    }
    return true;
}

struct strtab_embed_ctx* strtab_embed_ctx_new() {
    struct strtab_embed_ctx* ret = malloc(sizeof(struct strtab_embed_ctx));
    if (!ret) {
        perror("malloc");
        return NULL;
    }
    ret->nstrs = 0;
    ret->enc = STRTAB_ENC_UTF8;
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
            /* Seek until end of file or newline, whichever earlier */
            if (i != sz - 1 && fbuf[i] != '\n')
                i++;
            else {
                /* If we haven't encountered newline, copy until end of file */
                if (i == sz - 1 && fbuf[i] != '\n')
                    i = sz;
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
                // fprintf(stderr, "%u: %s\n", sidx, ret->strs[sidx]);
                ret->nstrs = sidx + 1 > ret->nstrs ? sidx + 1 : ret->nstrs;
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
            ret->strs[i] = EMBED_STR_PLACEHOLDER;

done:
    free(fbuf);
    return ret;
}

static bool patch_cksum_sz(uint8_t* rom, size_t rom_sz, size_t script_sz, uint32_t sz_to_patch_vma) {
    if (VMA2OFFS(sz_to_patch_vma) + sizeof(uint32_t) >= rom_sz) {
        fprintf(stderr, "ROM too small for patching at 0x%x\n", sz_to_patch_vma);
        return false;
    }
    if (script_sz > UINT32_MAX) {
        fprintf(stderr, "Assembled script is too large for embedding\n");
        return false;
    }
    // fprintf(stderr, "patching script sz to 0x%x at vma 0x%x\n", (uint32_t)script_sz, sz_to_patch_vma);
    *(uint32_t*)&rom[VMA2OFFS(sz_to_patch_vma)] = (uint32_t)script_sz;
    return true;
}

/**
 * For each ShowText command in the parse context, if the text used by the command argument is too
 * long to fit on screen, split the text and insert extra ShowText commands after it.
 *
 * We will insert new ops into pctx linked list and pass it to script_assemble next.
 *
 * - strtab_ectx needs to contain already hard-wrapped SJIS. Fix by doing SJIS conversion&wrapping
 * before embedding.
 */
static bool split_ShowText(struct script_parse_ctx* pctx, struct strtab_embed_ctx* strtab_ectx) {

}

bool embed_script(uint8_t* rom, size_t rom_sz, size_t script_sz_max, size_t script_offs,
        FILE* fscript, FILE* strtab_scr, FILE* strtab_menu,
        const char* script_path,
        size_t script_fsz, size_t strtab_scr_fsz, size_t strtab_menu_fsz,
        uint32_t strtab_scr_vma, uint32_t strtab_menu_vma,
        uint32_t strtab_scr_sz, uint32_t strtab_menu_sz,
        uint32_t sz_to_patch_vma, uint32_t script_ptr_vma) {
    bool ret = false;
    struct script_parse_ctx* pctx = NULL;
    iconv_t conv = (iconv_t)-1;
    struct strtab_embed_ctx* ectx_scr = NULL, * ectx_menu = NULL;
    struct script_as_ctx* actx = NULL;

    if (!fscript)
        return false;

    rewind(fscript);

    char* fbuf = malloc(script_fsz + 1);
    if (!fbuf) {
        perror("malloc");
        goto done;
    }

    if (fread(fbuf, 1, script_fsz, fscript) < script_fsz) {
        fprintf(stderr, "Failed to fread (error %d)\n", ferror(fscript));
        goto done;
    }
    fbuf[script_fsz] = '\0';

    pctx = malloc(sizeof(*pctx));
    if (!pctx) {
        perror("malloc");
        goto done;
    }

    if (!script_parse_ctx_init(pctx, fbuf)) {
        pctx = NULL;
        goto done;
    }
    pctx->filename = script_path;

    bool parsed = script_parse_ctx_parse(pctx);
    for (size_t i = 0; i < pctx->ndiags; i++)
        fprintf(stderr, "%s:%zu:%zu: %s\n", script_path, pctx->diags[i].line, pctx->diags[i].col,
            pctx->diags[i].msg);

    if (!parsed)
        goto done;

    conv = conv_for_embedding();
    if (conv == (iconv_t)-1)
        goto done;

    ectx_scr = strtab_embed_ctx_with_file(strtab_scr, strtab_scr_fsz);
    ectx_menu = strtab_embed_ctx_with_file(strtab_menu, strtab_menu_fsz);

    if (!ectx_scr || !ectx_menu)
        goto done;

    ectx_scr->rom_vma = strtab_scr_vma;
    ectx_menu->rom_vma = strtab_menu_vma;

    // fprintf(stderr, "ectx_scr %zu menu %zu\n", ectx_scr->nstrs, ectx_menu->nstrs);

    actx = script_as_ctx_new(pctx, &rom[script_offs], script_sz_max, ectx_scr, ectx_menu);
    ret = script_fill_strtabs(actx);

    ret = ret && ctx_conv(conv, ectx_scr) && ctx_conv(conv, ectx_menu);
    if (ret)
        ctx_hard_wrap(ectx_scr);

    size_t script_storage_used = 0;

    ret = ret && script_assemble(actx) &&
        patch_cksum_sz(rom, rom_sz,
            (script_storage_used = script_sz((void*)&rom[script_offs]) + sizeof(struct script_hdr)),
            sz_to_patch_vma) &&
        patch_ptr(rom, rom_sz, OFFS2VMA(script_offs), script_ptr_vma) &&
        embed_strtabs(rom, rom_sz, ectx_scr, ectx_menu, strtab_scr_sz, strtab_menu_sz, conv);

    if (ret)
        fprintf(stderr, "Embedded script at 0x%lx using %zu B\n", OFFS2VMA(script_offs),
            script_sz((void*)&rom[script_offs]) + sizeof(struct script_hdr));
    else
        fprintf(stderr, "Failed to embed script\n");

done:
    if (conv != (iconv_t)-1) {
#ifdef HAS_ICONV
        iconv_close(conv);
#endif
    }
    if (fbuf)
        free(fbuf);
    if (pctx) {
        script_parse_ctx_free(pctx);
        free(pctx);
    }
    if (ectx_scr)
        strtab_embed_ctx_free(ectx_scr);
    if (ectx_menu)
        strtab_embed_ctx_free(ectx_menu);
    if (actx)
        script_as_ctx_free(actx);
    return ret;
}
