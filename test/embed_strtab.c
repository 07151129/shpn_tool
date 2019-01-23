#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "embed.h"
#include "strtab.h"

void test_embed_ctx_with_file() {
    static const struct {size_t idx; const char* s;} good_strs[] = {
        {0, u8"データがありません。¥n又は入力に誤りがあります。"},
        {1, u8"序章　　「悪夢の始まり」"},
        {4, u8"Кириллица :)"},
        {6, u8"Missing entries should be filled with NUL"},
        {20, u8"That's pretty far"}
    };

    struct strtab_embed_ctx* ectx = strtab_embed_ctx_with_file("test/strtab.good");
    assert(ectx);
    for (size_t i = 0; i < ectx->nstrs; i++)
        if (ectx->strs[i])
            for (size_t j = 0; j < sizeof(good_strs) / sizeof(*good_strs); j++)
                if (i == good_strs[j].idx)
                    assert(!strcmp(ectx->strs[i], good_strs[j].s));

    strtab_embed_ctx_free(ectx);
    free(ectx);

    ectx = strtab_embed_ctx_with_file("test/strtab.bad");
    assert(!ectx);
}

void test_embed_strtabs() {
#define EMBED_BUF_SZ (228*1024llu)
    uint8_t* buf = malloc(EMBED_BUF_SZ);
    if (!buf) {
        perror("malloc");
        assert(false);
    }
    struct strtab_embed_ctx* ectx_script = strtab_embed_ctx_with_file("test/strtab.good");
    ectx_script->rom_vma = ROM_BASE;
    struct strtab_embed_ctx* ectx_menu = strtab_embed_ctx_with_file("test/strtab.good");
    ectx_menu->rom_vma = ROM_BASE + 0x36b64;

    assert(embed_strtabs(buf, EMBED_BUF_SZ, ectx_script, ectx_menu, (iconv_t)-1));

    free(ectx_script);
    free(ectx_menu);
    free(buf);
}

int main() {
    test_embed_ctx_with_file();
    test_embed_strtabs();
}
