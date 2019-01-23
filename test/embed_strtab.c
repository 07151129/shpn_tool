#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "embed.h"

int main() {
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

    ectx = strtab_embed_ctx_with_file("test/strtab.bad");
    assert(!ectx);
}
