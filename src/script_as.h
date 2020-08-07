#ifndef SCRIPT_AS_H
#define SCRIPT_AS_H

#include <stdbool.h>
#include <stddef.h>

#include "defs.h"
#include "embed.h"
#include "script_disass.h"
#include "script_parse_ctx.h"

struct script_as_ctx* script_as_ctx_new(struct script_parse_ctx* pctx, uint8_t* dst,
    size_t dst_sz, struct strtab_embed_ctx* strs_sc, struct strtab_embed_ctx* strs_menu);
void script_as_ctx_free(struct script_as_ctx* actx);

/**
 * script_fill_strtabs must be called before script_assemble in order to populate strtabs with
 * strings encountered in pctx. This operation will also convert every ARG_TY_STR arg to
 * ARG_TY_NUMBERED_STR. script_assemble will then simply write ARG_TY_NUMBERED_STR to dst buffer.
 */
bool script_fill_strtabs(struct script_as_ctx* actx);
bool script_assemble(struct script_as_ctx* actx);

bool split_ShowText_stmts(struct script_as_ctx* actx);
bool split_ShowText_stmt(struct script_as_ctx* actx, struct script_stmt* stmt,
    struct strtab_embed_ctx* strtab, struct script_stmt** next);

#endif
