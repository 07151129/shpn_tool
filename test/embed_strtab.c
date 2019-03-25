#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "embed.h"
#include "strtab.h"

void test_embed_ctx_with_file(FILE* good, size_t good_sz, FILE* bad, size_t bad_sz) {
    static const struct {size_t idx; const char* s;} good_strs[] = {
        {0, u8"データがありません。¥n又は入力に誤りがあります。"},
        {1, u8"序章　　「悪夢の始まり」"},
        {4, u8"Кириллица :)"},
        {6, u8"Missing entries should be filled with NUL"},
        {20, u8"That's pretty far"}
    };

    struct strtab_embed_ctx* ectx = strtab_embed_ctx_with_file(good, good_sz);
    assert(ectx);
    for (size_t i = 0; i < ectx->nstrs; i++)
        if (ectx->allocated[i]) {
            for (size_t j = 0; j < sizeof(good_strs) / sizeof(*good_strs); j++)
                if (i == good_strs[j].idx)
                    assert(!strcmp(ectx->strs[i], good_strs[j].s));
        } else
            assert(!strcmp(ectx->strs[i], ""));

    strtab_embed_ctx_free(ectx);

    ectx = strtab_embed_ctx_with_file(bad, bad_sz);
    assert(!ectx);
}

#define EMBED_BUF_SZ 4096

void test_embed_strtabs(FILE* good, size_t good_sz, iconv_t conv) {
    uint8_t* buf = malloc(EMBED_BUF_SZ);
    if (!buf) {
        perror("malloc");
        assert(false);
    }
    struct strtab_embed_ctx* ectx_script = strtab_embed_ctx_with_file(good, good_sz);
    assert(ectx_script);

    ectx_script->rom_vma = ROM_BASE;
    struct strtab_embed_ctx* ectx_menu = strtab_embed_ctx_with_file(good, good_sz);
    assert(ectx_menu);

    ectx_menu->rom_vma = ROM_BASE + 0x36b64;

    assert(embed_strtabs(buf, EMBED_BUF_SZ, ectx_script, ectx_menu, EMBED_BUF_SZ, EMBED_BUF_SZ, conv));

    strtab_embed_ctx_free(ectx_script);
    strtab_embed_ctx_free(ectx_menu);
    free(buf);
}

size_t fsz(const char* path) {
    struct stat st;
    assert(stat(path, &st) != -1);
    return st.st_size;
}

#define PATH_GOOD "test/strtab.good"
#define PATH_BAD "test/strtab.bad"

int main() {
    assert(HAS_ICONV);
    FILE* good, * bad;
    good = fopen(PATH_GOOD, "rb");
    bad = fopen(PATH_BAD, "rb");

    assert(good && bad);

    iconv_t conv = conv = conv_for_embedding();
    test_embed_ctx_with_file(good, fsz(PATH_GOOD), bad, fsz(PATH_BAD));
#if 0
    test_embed_strtabs(good, fsz(PATH_GOOD), conv);
#endif

    fclose(good);
    fclose(bad);
#ifdef HAD_ICONV
    iconv_close(conv);
#endif
}
