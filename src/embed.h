#ifndef EMBED_H
#define EMBED_H

#include <stddef.h>
#include <stdint.h>

#define EMBED_STRTAB_SZ 10000
struct strtab_embed_ctx {
    size_t nstrs;
    char* strs[EMBED_STRTAB_SZ];
    uint32_t rom_vma;
};

#endif
