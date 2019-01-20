#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "script_disass.h"
#include "script_parse_ctx.h"

#define EMBED_STRTAB_SZ 10000
struct strtab_embed_ctx {
    size_t nstrs;
    char* strs[EMBED_STRTAB_SZ];
};

FMT_PRINTF(4, 5)
static void log(bool err, const struct script_stmt* stmt, const struct script_parse_ctx* pctx,
        const char* msg, ...) {
    if (pctx->filename)
        fprintf(stderr, "%s: ", pctx->filename);
    fprintf(stderr, "%zu: %s: ", stmt->line, err ? "error" : "warning");

    va_list va;
    va_start(va, msg);
    vfprintf(stderr, msg, va);
    va_end(va);
    fputc('\n', stderr);
}

struct jump_refs_ctx {
#define JUMP_REFS_SZ 10000
    size_t nrefs;
    struct jump_ref {
        const struct script_stmt* label;
        uint16_t emitted_label;

        const struct script_stmt* jump;
        uint8_t* emitted_jump;
    } refs [JUMP_REFS_SZ];
};

static bool jump_refs_add(struct jump_refs_ctx* refs, const struct script_stmt* jump,
    const struct script_stmt* label, uint8_t* ejump, uint16_t elabel) {
    if (refs->nrefs >= JUMP_REFS_SZ)
        return false;
    refs->refs[refs->nrefs++] = (struct jump_ref){.label = label, .jump = jump,
        .emitted_jump = ejump, .emitted_label = elabel};
    return true;
}

static bool emit_byte(const struct script_stmt* stmt, const struct script_parse_ctx* pctx,
    uint8_t** dst, size_t* dst_sz) {
    assert(stmt->ty == STMT_TY_BYTE);

    if ((size_t)stmt->byte.n > *dst_sz) {
        log(true, stmt, pctx, "too many bytes to emit");
        return false;
    }
    if ((1 << (8 * stmt->byte.n)) - 1 < stmt->byte.val) {
        log(true, stmt, pctx, "0x%x takes more than %d bytes to store", stmt->byte.val,
            stmt->byte.n);
        return false;
    }
    *(uint32_t*)*dst = stmt->byte.val;
    *dst += stmt->byte.n;
    *dst_sz -= stmt->byte.n;
    return true;
}

static bool emit_arg_num(const struct script_stmt* stmt, const struct script_arg* arg,
        const struct script_parse_ctx* pctx,
        uint8_t** dst, size_t* dst_sz) {
    assert(arg->type == ARG_TY_NUM);

    if (*dst_sz < sizeof(uint16_t)) {
        log(true, stmt, pctx, "too many bytes to emit argument 0x%x", arg->num);
        return false;
    }

    *(uint16_t*)*dst = arg->num;
    *dst += sizeof(uint16_t);
    *dst_sz -= sizeof(uint16_t);
    return true;
}

static const struct script_stmt* find_labeled_stmt(const struct script_parse_ctx* pctx,
        const char* label) {
    for (size_t i = 0; i < pctx->nstmts; i++)
        if (!strcmp(pctx->stmts[i].label, label))
            return &pctx->stmts[i];
    return NULL;
}

static bool emit_arg_label(const struct script_stmt* stmt, const struct script_arg* arg,
        const struct script_parse_ctx* pctx,
        uint8_t** dst, size_t* dst_sz,
        struct jump_refs_ctx* refs) {
    assert(arg->type == ARG_TY_LABEL && stmt->ty == STMT_TY_OP);

    if (!cmd_is_jump(&(union script_cmd){.op = stmt->op.idx}))
        return true;

    uint16_t bdst = 0;

    /**
     * Check if destination label occured before the jump. In that case, it must have been added
     * to the refs already.
     */
    for (size_t i = 0; i < refs->nrefs; i++) {
        if (stmt == refs->refs[i].jump) {
            bdst = refs->refs[i].emitted_label;
            break;
        }
    }
    /**
     * If destination label is somewhere ahead, write zero for now and overwrite the value when we
     * try to emit statement at that label.
     */
    *(uint16_t*)*dst = bdst;
    if (bdst == 0) {
        const struct script_stmt* labeled_stmt = find_labeled_stmt(pctx, arg->label);
        if (!labeled_stmt) {
            log(true, stmt, pctx, "label %s not found", arg->label);
            return false;
        }
        if (!jump_refs_add(refs, stmt, labeled_stmt, *dst, 0))
            log(true, stmt, pctx, "too many jumps in the script");
    }

    if (*dst_sz < sizeof(uint16_t)) {
        log(true, stmt, pctx, "no space to write jump destination");
        return false;
    }

    *dst_sz -= sizeof(uint16_t);
    *dst += sizeof(uint16_t);
    return true;
}

static bool emit_arg_str(const struct script_stmt* stmt, const struct script_arg* arg,
        const struct script_parse_ctx* pctx,
        uint8_t** dst, size_t* dst_sz,
        struct strtab_embed_ctx* strs) {
    assert(arg->type == ARG_TY_STR);

    if (strs->nstrs >= EMBED_STRTAB_SZ) {
        log(true, stmt, pctx, "too many strings in program");
        return false;
    }

    uint16_t i = 0;
    for (; i < EMBED_STRTAB_SZ; i++)
        if (!strs->strs[i])
            break;

    if (*dst_sz < sizeof(i)) {
        log(true, stmt, pctx, "no space to write string table index");
        return false;
    }

    *(uint16_t*)*dst = i;
    *dst += sizeof(i);
    *dst_sz -= sizeof(i);

    strs->strs[i] = strdup(arg->str);
    if (!strs->strs[i]) {
        log(true, stmt, pctx, "failed to allocate memory for string");
        return false;
    }
    strs->nstrs++;

    return true;
}

static bool emit_arg_numbered_str(const struct script_stmt* stmt, const struct script_arg* arg,
        const struct script_parse_ctx* pctx,
        uint8_t** dst, size_t* dst_sz,
        struct strtab_embed_ctx* strs) {
    assert(arg->type == ARG_TY_NUMBERED_STR);

    if (strs->strs[arg->numbered_str.num])
        log(false, stmt, pctx, "overwriting existing string table entry");
    if (*dst_sz < sizeof(arg->numbered_str.num)) {
        log(true, stmt, pctx, "no space to write string table index");
        return false;
    }
    *(uint16_t*)*dst = arg->numbered_str.num;
    *dst += sizeof(arg->numbered_str.num);
    *dst_sz -= sizeof(arg->numbered_str.num);

    if (strs->strs[arg->numbered_str.num])
        free(strs->strs[arg->numbered_str.num]);
    strs->strs[arg->numbered_str.num] = strdup(arg->numbered_str.str);
    if (!strs->strs[arg->numbered_str.num]) {
        log(true, stmt, pctx, "failed to allocate memory for string");
        return false;
    }

    return true;
}

static bool emit_arg(const struct script_stmt* stmt, const struct script_arg* arg,
        const struct script_parse_ctx* pctx,
        uint8_t** dst, size_t* dst_sz,
        struct strtab_embed_ctx* strs_sc, struct strtab_embed_ctx* strs_menu,
        struct jump_refs_ctx* refs) {
    assert(stmt->ty == STMT_TY_OP);

    struct strtab_embed_ctx* strs = cmd_uses_menu_strtab(&(union script_cmd){.op = stmt->op.idx}) ?
        strs_menu : strs_sc;

    switch (arg->type) {
        case ARG_TY_NUM:
            return emit_arg_num(stmt, arg, pctx, dst, dst_sz);
        case ARG_TY_LABEL:
            return emit_arg_label(stmt, arg, pctx, dst, dst_sz, refs);
        case ARG_TY_STR:
            return emit_arg_str(stmt, arg, pctx, dst, dst_sz, strs);
        case ARG_TY_NUMBERED_STR:
            return emit_arg_numbered_str(stmt, arg, pctx, dst, dst_sz, strs);
        default:
            assert(false);
    }
    return false;
}

static bool emit_op(const struct script_stmt* stmt, const struct script_parse_ctx* pctx,
        uint8_t** dst, size_t* dst_sz,
        struct strtab_embed_ctx* strs_sc, struct strtab_embed_ctx* strs_menu,
        struct jump_refs_ctx* refs) {
    assert(stmt->ty == STMT_TY_OP);

    union script_cmd cmd = {.op = stmt->op.idx, .arg = stmt->op.args.nargs};
    if (sizeof(cmd) + cmd.arg * sizeof(uint16_t) > *dst_sz) {
        log(true, stmt, pctx, "command too large to store");
        return false;
    }
    *(union script_cmd*)*dst = cmd;
    *dst += sizeof(union script_cmd);
    *dst_sz -= sizeof(union script_cmd);

    /* For branch the second argument is not emitted */
    if (cmd_is_branch(&cmd))
        cmd.arg--;

    for (size_t i = 0; i < cmd.arg; i++)
        if (!emit_arg(stmt, &stmt->op.args.args[i], pctx, dst, dst_sz, strs_sc, strs_menu, refs))
            return false;

    return true;
}

static bool branch_src(const struct script_stmt* stmt,
    const struct script_parse_ctx* pctx, size_t* bsrc_idx) {
    for (size_t i = *bsrc_idx; i < pctx->nstmts; i++) {
        const struct script_stmt* src = &pctx->stmts[i];
        if (src->ty != STMT_TY_OP)
            continue;
        union script_cmd cmd = {.op = src->op.idx};
        if (cmd_is_branch(&cmd) || cmd_is_jump(&cmd)) {
            for (int j = 0; j < src->op.args.nargs; j++)
                if (src->op.args.args[j].type == ARG_TY_LABEL &&
                    !strcmp(src->op.args.args[j].label, stmt->label)) {
                *bsrc_idx = i;
                return true;
            }
        }
    }
    return false;
}

static bool emit_stmt(const struct script_stmt* stmt, const struct script_parse_ctx* pctx,
        uint8_t** dst, uint8_t* dst_start, size_t* dst_sz,
        struct strtab_embed_ctx* strs_sc, struct strtab_embed_ctx* strs_menu,
        struct jump_refs_ctx* refs) {
    /* There's a label at this statement and it's referenced by a Branch/Jump op */
    if (stmt->label) {
        /* For all jumps to stmt emitted before stmt, set their dst to stmt emitted loc */
        for (size_t i = 0; i < refs->nrefs; i++) {
            if (refs->refs[i].label == stmt) {
                if (*dst - dst_start > UINT16_MAX) {
                    log(true, stmt, pctx, "jump to this location from line %zu cannot be encoded",
                        refs->refs[i].jump->line);
                }
                *(uint16_t*)(refs->refs[i].emitted_jump + sizeof(union script_cmd)) =
                    (uint16_t)(*dst - dst_start);
            }
        }

        size_t bsrc_idx = 0;

next_src:
        if (!branch_src(stmt, pctx, &bsrc_idx) && !bsrc_idx)
            log(false, stmt, pctx, "label %s unreferenced", stmt->label);
        else if (cmd_is_branch(&(union script_cmd){.op = pctx->stmts[bsrc_idx].op.idx})) {
            /* Check if stmt is reachable for branching from src */
            if (stmt <= &pctx->stmts[bsrc_idx]) {
                log(true, stmt, pctx, "cannot branch from line %zu backwards to label here",
                    pctx->stmts[bsrc_idx].line);
                return false;
            }
            for (size_t i = bsrc_idx + 1; i < (size_t)(stmt - &pctx->stmts[bsrc_idx]); i++) {
                if (cmd_can_be_branched_to(&(union script_cmd){.op = pctx->stmts[i].op.idx})) {
                    log(true, stmt, pctx, "branching to label here from line %zu would branch \
                        to line %zu instead",
                        pctx->stmts[bsrc_idx].line, pctx->stmts[i].line);
                    return false;
                }
            }
            if (!cmd_can_be_branched_to(&(union script_cmd){.op = stmt->op.idx})) {
                /* Emit nop just before stmt so we can branch here */
                struct script_stmt nop = {.ty = STMT_TY_OP, .line = stmt->line,
                    .op = {.idx = 7, .args = {.nargs = 0}}
                };
                if (!emit_op(&nop, pctx, dst, dst_sz, strs_sc, strs_menu, refs)) {
                    log(true, stmt, pctx, "failed to emit nop for branching here");
                    return false;
                }
            }
        } else if (cmd_is_jump(&(union script_cmd){.op = pctx->stmts[bsrc_idx].op.idx})) {
            const struct script_stmt* jump = &pctx->stmts[bsrc_idx];
            /* Label occurs before a jump to it; create source-dst entry to be handled by jump later */
            if (stmt < jump) {
                if ((*dst - dst_start) > UINT16_MAX) {
                    log(true, stmt, pctx, "jump to this location from line %zu cannot be encoded",
                        jump->line);
                    return false;
                }
                if (!jump_refs_add(refs, jump, stmt, NULL, (uint16_t)(*dst - dst_start))) {
                    log(true, stmt, pctx, "too many jumps in the script");
                    return false;
                }
            }
        }

        /* Keep looking for more refs to the label */
        if (++bsrc_idx < pctx->nstmts)
            goto next_src;
    }

    switch (stmt->ty) {
        case STMT_TY_OP:
            return emit_op(stmt, pctx, dst, dst_sz, strs_sc, strs_menu, refs);

        case STMT_TY_BYTE:
            return emit_byte(stmt, pctx, dst, dst_sz);

        default:
            assert(false);
    }
    return false;
}

/* TODO: Check for parse errors somewhere in script_verbs */
/* FIXME: Need to check somewhere if strlen of each part until \n <= 512 */

bool script_assemble(const struct script_parse_ctx* pctx, uint8_t* dst, size_t dst_sz,
        struct strtab_embed_ctx* strs_sc, struct strtab_embed_ctx* strs_menu) {
    assert(pctx->ndiags == 0 && "Trying to assemble script with parse errors");
    assert(HAS_ICONV);

    struct jump_refs_ctx* refs = malloc(sizeof(struct jump_refs_ctx));
    if (!refs) {
        fprintf(stderr, "Failed to allocate jump_refs_ctx\n");
        return false;
    }

    bool ret = true;
    for (size_t i = 0; i < pctx->nstmts; i++)
        if (!emit_stmt(&pctx->stmts[i], pctx, &dst, dst, &dst_sz, strs_sc, strs_menu, refs)) {
            ret = false;
            break;
        }

    free(refs);
    return ret;
}
