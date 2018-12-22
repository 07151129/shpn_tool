#ifndef STRTAB_H
#define STRTAB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

bool strtab_dump(const uint8_t* rom, size_t sz, uint32_t vma, uint32_t idx, bool has_idx, FILE* fout);
bool strtab_dec_str(const uint8_t* strtab, uint32_t idx, char* out, size_t out_sz, size_t* nwritten);

#define DEC_BUF_SZ_SJIS 512
#define SJIS_TO_U8_MIN_SZ(len) (3*(len) + 1) /* Bound for SJIS -> UTF8 */

#endif
