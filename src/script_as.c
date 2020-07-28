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
#include "embed.h"
#include "script_disass.h"
#include "script_parse_ctx.h"
#include "strtab.h"

struct script_as_ctx {
    const struct script_parse_ctx* pctx;
    uint8_t* dst;
    uint8_t* dst_start;
    size_t dst_sz;
    iconv_t conv;
    struct strtab_embed_ctx* strs_sc;
    struct strtab_embed_ctx* strs_menu;
    struct jump_refs_ctx* refs;
    const uint8_t* branch_info_begin, * branch_info_end;
};

FMT_PRINTF(4, 5)
static void log(bool err, const struct script_stmt* stmt, const struct script_parse_ctx* pctx,
        const char* msg, ...) {
    if (pctx->filename)
        fprintf(stderr, "%s:", pctx->filename);
    if (stmt)
        fprintf(stderr, "%zu: ", stmt->line);
    fprintf(stderr, "%s: ", err ? "error" : "warning");

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

static bool emit_byte(const struct script_stmt* stmt, struct script_as_ctx* actx) {
    assert(stmt->ty == STMT_TY_BYTE);

    if ((size_t)stmt->byte.n > actx->dst_sz) {
        log(true, stmt, actx->pctx, "too many bytes to emit");
        return false;
    }
    if ((1ull << (8 * stmt->byte.n)) - 1 < stmt->byte.val) {
        log(true, stmt, actx->pctx, "0x%x takes more than %d bytes to store", stmt->byte.val,
            stmt->byte.n);
        return false;
    }

    memcpy(actx->dst, &stmt->byte.val, stmt->byte.n);
    actx->dst += stmt->byte.n;
    actx->dst_sz -= stmt->byte.n;
    return true;
}

static bool emit_arg_num(const struct script_stmt* stmt, const struct script_arg* arg,
        struct script_as_ctx* actx) {
    assert(arg->type == ARG_TY_NUM);

    if (actx->dst_sz < sizeof(uint16_t)) {
        log(true, stmt, actx->pctx, "too many bytes to emit argument 0x%x", arg->num);
        return false;
    }

    memcpy(actx->dst, &arg->num, sizeof(arg->num));
    actx->dst += sizeof(uint16_t);
    actx->dst_sz -= sizeof(uint16_t);
    return true;
}

static const struct script_stmt* find_labeled_stmt(const struct script_parse_ctx* pctx,
        const char* label) {
    for (size_t i = 0; i < pctx->nstmts; i++)
        if (pctx->stmts[i].label && !strcmp(pctx->stmts[i].label, label))
            return &pctx->stmts[i];
    return NULL;
}

static bool emit_arg_label(const struct script_stmt* stmt, const struct script_arg* arg,
        struct script_as_ctx* actx) {
    assert(arg->type == ARG_TY_LABEL && stmt->ty == STMT_TY_OP);

    if (!cmd_is_jump(&(union script_cmd){.op = stmt->op.idx}))
        return true;

    uint16_t bdst = UINT16_MAX;

    /**
     * Check if destination label occured before the jump. In that case, it must have been added
     * to the refs already.
     */
    for (size_t i = 0; i < actx->refs->nrefs; i++) {
        if (stmt == actx->refs->refs[i].jump) {
            bdst = actx->refs->refs[i].emitted_label;
            break;
        }
    }
    /**
     * If destination label is somewhere ahead, write zero for now and overwrite the value when we
     * try to emit statement at that label.
     */
    memcpy(actx->dst, &bdst, sizeof(bdst));
    if (bdst == UINT16_MAX) {
        const struct script_stmt* labeled_stmt = find_labeled_stmt(actx->pctx, arg->label);
        if (!labeled_stmt) {
            log(true, stmt, actx->pctx, "label %s not found", arg->label);
            return false;
        }
        if (!jump_refs_add(actx->refs, stmt, labeled_stmt, actx->dst, 0))
            log(true, stmt, actx->pctx, "too many jumps in the script");
    }

    if (actx->dst_sz < sizeof(uint16_t)) {
        log(true, stmt, actx->pctx, "no space to write jump destination");
        return false;
    }

    actx->dst_sz -= sizeof(uint16_t);
    actx->dst += sizeof(uint16_t);
    return true;
}

static bool emit_arg_str(const struct script_stmt* stmt, const struct script_arg* arg,
    UNUSED struct strtab_embed_ctx* strs, struct script_as_ctx* actx) {
    assert(arg->type == ARG_TY_STR);

    log(true, stmt, actx->pctx, "unprocessed unnumbered string");
    return false;
}

static bool emit_arg_numbered_str(const struct script_stmt* stmt, const struct script_arg* arg,
        struct strtab_embed_ctx* strs, struct script_as_ctx* actx) {
    assert(arg->type == ARG_TY_NUMBERED_STR);

    if (!strs->allocated[arg->numbered_str.num]) {
        log(true, stmt, actx->pctx, "unprocessed string at %d strtab %p", arg->numbered_str.num,
            strs);
        return false;
    }

    if (actx->dst_sz < sizeof(arg->numbered_str.num)) {
        log(true, stmt, actx->pctx, "no space to write string table index");
        return false;
    }
    memcpy(actx->dst, &arg->numbered_str.num, sizeof(arg->numbered_str.num));
    actx->dst += sizeof(arg->numbered_str.num);
    actx->dst_sz -= sizeof(arg->numbered_str.num);

    return true;
}

static bool emit_arg(const struct script_stmt* stmt, const struct script_arg* arg,
        struct script_as_ctx* actx) {
    assert(stmt->ty == STMT_TY_OP);

    struct strtab_embed_ctx* strs = cmd_uses_menu_strtab(&(union script_cmd){.op = stmt->op.idx}) ?
        actx->strs_menu : actx->strs_sc;

    switch (arg->type) {
        case ARG_TY_NUM:
            return emit_arg_num(stmt, arg, actx);
        case ARG_TY_LABEL:
            return emit_arg_label(stmt, arg, actx);
        case ARG_TY_STR:
            return emit_arg_str(stmt, arg, strs, actx);
        case ARG_TY_NUMBERED_STR:
            return emit_arg_numbered_str(stmt, arg, strs, actx);
        default:
            assert(false);
    }
    return false;
}

static bool emit_op(const struct script_stmt* stmt, struct script_as_ctx* actx) {
    assert(stmt->ty == STMT_TY_OP);

    union script_cmd cmd = {.op = stmt->op.idx, .arg = stmt->op.args.nargs};
    if (sizeof(cmd) + cmd.arg * sizeof(uint16_t) > actx->dst_sz) {
        log(true, stmt, actx->pctx, "command too large to store");
        return false;
    }
    // fprintf(stderr, "OP at line %zu at offs 0x%tx\n", stmt->line, actx->dst - actx->dst_start);

    /* For branch the second argument is not emitted */
    if (cmd_is_branch(&cmd))
        cmd.arg--;

    memcpy(actx->dst, &cmd, sizeof(cmd));
    actx->dst += sizeof(union script_cmd);
    actx->dst_sz -= sizeof(union script_cmd);

    for (size_t i = 0; i < cmd.arg; i++)
        if (!emit_arg(stmt, &stmt->op.args.args[i], actx))
            return false;

    return true;
}

enum stmt_order {
    STMT_ORD_LT,
    STMT_ORD_EQ,
    STMT_ORD_GT
};

static enum stmt_order stmt_order(const struct script_stmt* first, const struct script_stmt* second) {
    if (first == second)
        return STMT_ORD_EQ;

    while (first) {
        if (first->next == second)
            return STMT_ORD_LT;
        first = first->next;
    }
    return STMT_ORD_GT;
}

static bool branch_src(const struct script_stmt* stmt, const struct script_stmt** bsrc) {
    for (; *bsrc; *bsrc = (*bsrc)->next) {
        if ((*bsrc)->ty != STMT_TY_OP)
            continue;

        union script_cmd cmd = {.op = (*bsrc)->op.idx};
        if (cmd_is_branch(&cmd) || cmd_is_jump(&cmd)) {
            for (int j = 0; j < (*bsrc)->op.args.nargs; j++)
                if ((*bsrc)->op.args.args[j].type == ARG_TY_LABEL &&
                    !strcmp((*bsrc)->op.args.args[j].label, stmt->label)) {
                // fprintf(stderr, "%zu -> %zu\n", src->line, stmt->line);
                return true;
            }
        }
    }

    return false;
}

static bool section_stmt(const struct script_stmt* stmt, struct script_as_ctx* actx) {
    assert(stmt->ty == STMT_TY_BEGIN_END);

    if (!stmt->begin_end.section) {
        log(true, stmt, actx->pctx, "missing section name");
        return false;
    }

    if (!strcmp(stmt->begin_end.section, "branch_info")) {
        if (stmt->begin_end.begin)
            actx->branch_info_begin = actx->dst;
        else
            actx->branch_info_end = actx->dst;
        return true;
    }

    log(true, stmt, actx->pctx, "unsupported section %s", stmt->begin_end.section);
    return false;
}

static bool fixup_jumps(const struct script_stmt* stmt, struct script_as_ctx* actx) {
    assert(stmt->label);

    /* For all jumps to stmt emitted before stmt, set their dst to stmt emitted loc */
    for (size_t i = 0; i < actx->refs->nrefs; i++) {
        if (actx->refs->refs[i].label == stmt) {
            if (actx->dst - actx->dst_start > UINT16_MAX) {
                log(true, stmt, actx->pctx,
                    "jump to this location from line %zu cannot be encoded",
                    actx->refs->refs[i].jump->line);
                return false;
            }
            *(uint16_t*)actx->refs->refs[i].emitted_jump =
                (uint16_t)(actx->dst - actx->dst_start);
        }
    }

    return true;
}

static bool process_label_refs(const struct script_stmt* stmt, struct script_as_ctx* actx) {
    assert(stmt->label);

    const struct script_stmt* bsrc = &actx->pctx->stmts[0];
    bool checked_refs = false;

    do {
        if (!branch_src(stmt, &bsrc)) {
            if (!checked_refs)
                log(false, stmt, actx->pctx, "label %s unreferenced", stmt->label);
        } else if (cmd_is_branch(&(union script_cmd){.op = bsrc->op.idx})) {
            /* Check if stmt is reachable for branching from src */
            if (stmt_order(stmt, bsrc) != STMT_ORD_GT) {
                log(true, stmt, actx->pctx, "cannot branch from line %zu backwards to label here",
                    bsrc->line);
                return false;
            }
            for (struct script_stmt* stmt_bsrc = bsrc->next; stmt_bsrc && stmt_bsrc != stmt;
                stmt_bsrc = stmt_bsrc->next) {
                if (cmd_can_be_branched_to(&(union script_cmd){.op = stmt_bsrc->op.idx})) {
                    log(true, stmt, actx->pctx, "branching to label here from line %zu would branch \
                        to line %zu instead",
                        bsrc->line, stmt_bsrc->line);
                    return false;
                }
            }
            if (!cmd_can_be_branched_to(&(union script_cmd){.op = stmt->op.idx})) {
                /* Emit nop just before stmt so we can branch here */
                struct script_stmt nop = {.ty = STMT_TY_OP, .line = stmt->line,
                    .op = {.idx = 7, .args = {.nargs = 0}}
                };
                if (!emit_op(&nop, actx)) {
                    log(true, stmt, actx->pctx, "failed to emit nop for branching here");
                    return false;
                }
            }
        } else if (cmd_is_jump(&(union script_cmd){.op = bsrc->op.idx})) {
            /* Label occurs before a jump to it; create source-dst entry to be handled by jump later */
            if (stmt_order(stmt, bsrc) == STMT_ORD_LT) {
                if ((actx->dst - actx->dst_start) > UINT16_MAX) {
                    log(true, stmt, actx->pctx, "jump to this location from line %zu cannot be encoded",
                        bsrc->line);
                    return false;
                }
                if (!jump_refs_add(actx->refs, bsrc, stmt, NULL, (uint16_t)(actx->dst - actx->dst_start))) {
                    log(true, stmt, actx->pctx, "too many jumps in the script");
                    return false;
                }
            }
        }
        checked_refs = true;
        /* Keep looking for more refs to the label */
        if (bsrc)
            bsrc = bsrc->next;
    } while (bsrc);

    return true;
}

static bool emit_stmt(const struct script_stmt* stmt, struct script_as_ctx* actx) {
    /* There's a label at this statement and it's referenced by a Branch/Jump op */
    if (stmt->label)
        if (!fixup_jumps(stmt, actx) || !process_label_refs(stmt, actx))
            return false;

    switch (stmt->ty) {
        case STMT_TY_OP:
            return emit_op(stmt, actx);

        case STMT_TY_BYTE:
            return emit_byte(stmt, actx);

        case STMT_TY_BEGIN_END:
            return section_stmt(stmt, actx);

        default:
            assert(false && "Unsupported stmt type");
    }
    return false;
}

static bool arg_numbered_str_to_strtab(struct script_stmt* stmt, struct script_as_ctx* actx,
    struct strtab_embed_ctx* strs, struct script_arg* arg) {
    assert(arg->type == ARG_TY_NUMBERED_STR);

    if (arg->numbered_str.num >= EMBED_STRTAB_SZ) {
        log(true, stmt, actx->pctx, "string index too large");
        return false;
    }
    if (strs->allocated[arg->numbered_str.num])
        log(false, stmt, actx->pctx, "overwriting existing string table entry at %u",
            arg->numbered_str.num);

    if (strs->allocated[arg->numbered_str.num])
        free(strs->strs[arg->numbered_str.num]);

    strs->strs[arg->numbered_str.num] = strdup(arg->numbered_str.str);

    /* If we're inserting at index past nstrs-1, add placeholders in between */
    for (size_t i = strs->nstrs; i < arg->numbered_str.num; i++) {
        strs->strs[i] = EMBED_STR_PLACEHOLDER;
        strs->allocated[i] = false;
    }

    // fprintf(stderr, "Adding %s at %d\n", arg->numbered_str.str, arg->numbered_str.num);

    if (strs->nstrs < arg->numbered_str.num + 1)
        strs->nstrs = arg->numbered_str.num + 1;

    strs->allocated[arg->numbered_str.num] = strs->strs[arg->numbered_str.num] != NULL;

    if (!strs->strs[arg->numbered_str.num]) {
        log(true, stmt, actx->pctx, "failed to copy string");
        perror("strdup");
        return false;
    }

    return true;
}

static bool arg_str_to_strtab(struct script_stmt* stmt, struct script_as_ctx* actx,
    struct strtab_embed_ctx* strs, struct script_arg* arg) {
    assert(arg->type == ARG_TY_STR);

    if (strs->nstrs >= EMBED_STRTAB_SZ) {
        log(true, stmt, actx->pctx, "too many strings in program");
        return false;
    }

    uint16_t i = 0;
    for (; i < EMBED_STRTAB_SZ; i++)
        if (!strs->allocated[i])
            break;

    /* Convert arg to NUMBERED_STR */
    const char* str = arg->str;
    arg->numbered_str.num = i;
    arg->numbered_str.str = str;
    arg->type = ARG_TY_NUMBERED_STR;

    return arg_numbered_str_to_strtab(stmt, actx, strs, arg);
}

static bool stmt_str_to_strtab(struct script_stmt* stmt, struct script_as_ctx* actx) {
    struct strtab_embed_ctx* strtab = NULL;
    if (cmd_uses_menu_strtab(&(union script_cmd){.op = stmt->op.idx}))
        strtab = actx->strs_menu;
    else
        strtab = actx->strs_sc;

    struct script_arg_list* args = &stmt->op.args;
    bool ret = true;

    for (int i = 0; i < args->nargs; i++) {
        switch (args->args[i].type) {
            case ARG_TY_STR: {
                ret &= arg_str_to_strtab(stmt, actx, strtab, &args->args[i]);
                break;
            }
            case ARG_TY_NUMBERED_STR: {
                ret &= arg_numbered_str_to_strtab(stmt, actx, strtab, &args->args[i]);
                break;
            }
            default:
                continue;
        }

        if (!ret)
            log(true, stmt, actx->pctx, "failed to add %dth string arg to strtab", i);
    }

    return ret;
}

bool script_fill_strtabs(struct script_parse_ctx* pctx, struct script_as_ctx* actx) {
    bool ret = true;

    struct script_stmt* stmt = &pctx->stmts[0];
    size_t nstmts_left = pctx->nstmts;

    while (ret && stmt) {
        if (nstmts_left > 0)
            nstmts_left--;
        assert(!stmt->next || nstmts_left > 0);
        assert(nstmts_left == 0 || stmt->next);

        if (stmt->ty == STMT_TY_OP)
            ret &= stmt_str_to_strtab(stmt, actx);
        stmt = stmt->next;
    }

    return ret;
}

struct script_as_ctx* script_as_ctx_new(const struct script_parse_ctx* pctx, uint8_t* dst,
    size_t dst_sz, struct strtab_embed_ctx* strs_sc, struct strtab_embed_ctx* strs_menu) {
    assert(pctx->ndiags == 0 && "Trying to assemble script with parse errors");

    if (dst_sz < sizeof(struct script_hdr))
        return NULL;

    struct jump_refs_ctx* refs = malloc(sizeof(struct jump_refs_ctx));
    if (!refs) {
        perror("malloc");
        return NULL;
    }
    refs->nrefs = 0;

    struct script_as_ctx* actx = malloc(sizeof(struct script_as_ctx));
    if (!actx) {
        perror("malloc");
        goto fail;
    }

    *actx = (struct script_as_ctx){
        .pctx = pctx,
        .dst = dst + sizeof(struct script_hdr),
        .dst_sz = dst_sz - sizeof(struct script_hdr),
        .dst_start = dst + sizeof(struct script_hdr),
        .strs_sc = strs_sc,
        .strs_menu = strs_menu,
        .refs = refs,
        .branch_info_begin = NULL,
        .branch_info_end = NULL
    };

    return actx;
fail:
    if (refs)
        free(refs);
    if (actx)
        free(actx);
    return NULL;
}

void script_as_ctx_free(struct script_as_ctx* actx) {
    if (actx) {
        free(actx->refs);
        free(actx);
    }
}

bool script_assemble(const struct script_parse_ctx* pctx, struct script_as_ctx* actx) {
    assert(actx);

    bool ret = true;
    const struct script_stmt* stmt = &pctx->stmts[0];
    size_t nstmts_left = pctx->nstmts;

    while (ret && stmt) {
        if (nstmts_left > 0)
            nstmts_left--;
        assert(!stmt->next || nstmts_left > 0);
        assert(nstmts_left == 0 || stmt->next);

        ret &= emit_stmt(stmt, actx);
        stmt = stmt->next;
    }

    if (ret && (!actx->branch_info_begin || !actx->branch_info_end)) {
        log(true, NULL, pctx, "missing branch_info section");
        ret = false;
    }

    if (ret)
        memcpy(actx->dst_start - sizeof(struct script_hdr), &(struct script_hdr){
            .branch_info_offs = actx->branch_info_begin - actx->dst_start,
            .branch_info_sz = actx->branch_info_end - actx->branch_info_begin,
            .bytes_to_end = actx->dst - actx->branch_info_end
        }, sizeof(struct script_hdr));

    return ret;
}
