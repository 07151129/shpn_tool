#ifndef EMBED_H
#define EMBED_H

#include <stddef.h>
#include <stdint.h>

#include <stdbool.h>

#define EMBED_STRTAB_SZ 10000
struct strtab_embed_ctx {
    size_t nstrs;
    char* strs[EMBED_STRTAB_SZ];
    uint32_t rom_vma;
    bool allocated[EMBED_STRTAB_SZ];
};

struct strtab_embed_ctx* strtab_embed_ctx_with_file(const char* path);
void strtab_embed_ctx_free(const struct strtab_embed_ctx* ctx);

#endif
