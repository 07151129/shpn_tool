#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "script_parse_ctx.h"

static void test_syntax(struct script_parse_ctx* ctx) {
    static char* ok[] = {
        "Nop7(); // Zero args stmt",
        "ShowText(\"Unescaped string\");",
        "ShowText(\"String with escaped quote\");",
        u8"ShowText(\"String with ¥\" escaped quote\");",
        "ShowText((0xff)\"String with explicit strtab index\");",
        "Label: OP_0x0a(\"String argument\", (0xab)\"String with index argument\","
            "Identifier, 0xdead, 0xbeef);",
        ".8byte 0xdeafbeef",
        ".byte 123",
        ".begin some_section",
        ".end some_section"
    };

    for (size_t i = 0; i < sizeof(ok) / sizeof(*ok); i++) {
        script_parse_ctx_init(ctx, ok[i]);

        if (!script_parse_ctx_parse(ctx)) {
            fprintf(stderr, "Failed to parse \"%s\", diags:\n", ok[i]);

            for (size_t j = 0; j < ctx->ndiags; j++) {
                fprintf(stderr, "%zu: %zu: %s\n", ctx->diags[j].line, ctx->diags[j].col,
                    ctx->diags[j].msg);
            }
            assert(false);
        }

        script_parse_ctx_free(ctx);
    }

    static char* nok[] = {
        "CertainlyUnrecognisedOP();",
        "ShowText(\"Forgot to escape the \"\");",
        ".NotANum byte",
        "OP_1(\"Too many arguments\", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);",
        "/* Forgot to close the comment"
    };
    for (size_t i = 0; i < sizeof(nok) / sizeof(*nok); i++) {
        script_parse_ctx_init(ctx, nok[i]);

        if (script_parse_ctx_parse(ctx)) {
            fprintf(stderr, "Unexpectedly parsed \"%s\", diags:\n", nok[i]);

            for (size_t j = 0; j < ctx->ndiags; j++) {
                fprintf(stderr, "%zu: %zu: %s\n", ctx->diags[j].line, ctx->diags[j].col,
                    ctx->diags[j].msg);
            }
            assert(false);
        }

        script_parse_ctx_free(ctx);
    }
}

int main() {
    struct script_parse_ctx* ctx = malloc(sizeof(*ctx));

    test_syntax(ctx);

    free(ctx);
}
