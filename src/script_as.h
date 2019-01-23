#ifndef SCRIPT_AS_H
#define SCRIPT_AS_H

#include <stdbool.h>
#include <stddef.h>

#include "defs.h"
#include "embed.h"
#include "script_disass.h"
#include "script_parse_ctx.h"

bool script_assemble(const struct script_parse_ctx* pctx, uint8_t* dst, size_t dst_sz,
        struct strtab_embed_ctx* strs_sc, struct strtab_embed_ctx* strs_menu, size_t* nwritten,
        struct script_hdr* hdr, iconv_t conv);

#endif
