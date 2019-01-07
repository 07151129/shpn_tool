#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "script_parse_ctx.h"

int main() {
    struct script_parse_ctx* ctx = malloc(sizeof(*ctx));

    script_parse_ctx_init(ctx, "ShowText(1, \"blah\");");
    assert(script_parse_ctx_parse(ctx));

    script_parse_ctx_free(ctx);
    free(ctx);
}