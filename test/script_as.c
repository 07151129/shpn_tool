#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "defs.h"
#include "embed.h"
#include "script_as.h"
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

size_t fsz(const char* path) {
    struct stat st;
    assert(stat(path, &st) != -1);
    return st.st_size;
}

#define PATH_GOOD "test/strtab.good"

void test_as(struct script_parse_ctx* pctx) {
    assert(HAS_ICONV);
    FILE* good;
    good = fopen(PATH_GOOD, "rb");
    assert(good);

    struct strtab_embed_ctx* ectx_script = strtab_embed_ctx_with_file(good, fsz(PATH_GOOD));
    struct strtab_embed_ctx* ectx_menu = strtab_embed_ctx_with_file(good, fsz(PATH_GOOD));

    iconv_t conv = (iconv_t)-1;
#ifdef HAS_ICONV
    conv = iconv_open("SJIS", "UTF-8");
#endif
    assert(conv != (iconv_t)-1);

    uint8_t* rom = malloc(2048);
    assert(rom);

    script_parse_ctx_init(pctx, u8"ShowText((1001)\"Ｈｅｌｌｏ ｗｏｒｌｄ\");"
        ".begin branch_info .byte 0 .end branch_info");
    assert(script_parse_ctx_parse(pctx));
    assert(script_assemble(pctx, rom, 2048, ectx_script, ectx_menu));

    // fprintf(stderr, "nwritten %zu\n", nwritten);
    // for (size_t i = 0; i < nwritten; i++)
    //     fprintf(stderr, "%02x", rom[i]);
    // fprintf(stderr, "\n");

    strtab_embed_ctx_free(ectx_script);
    strtab_embed_ctx_free(ectx_menu);
    free(rom);
    iconv_close(conv);
}

int main() {
    struct script_parse_ctx* ctx = malloc(sizeof(*ctx));

    test_syntax(ctx);
    test_as(ctx);

    free(ctx);
}
