#ifndef STRTAB_H
#define STRTAB_H

#include "defs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

bool strtab_dump(const uint8_t* rom, size_t rom_sz, uint32_t vma, uint32_t idx, bool has_idx,
    FILE* fout);
bool strtab_dec_str(const uint8_t* strtab, const uint8_t* rom_end, uint32_t idx, char* out,
    size_t out_sz, size_t* nwritten, iconv_t conv, bool should_conv);

#define DEC_BUF_SZ_SJIS 1024
#define SJIS_TO_U8_MIN_SZ(len) (3 * (len) + 1) /* Bound for SJIS -> UTF8 */
#define U8_TO_SJIS_MIN_SZ(len) (2 * (len) - 2 * (len) / 3 + 1) /* Bound for U8 -> SJIS */

/**
 * Given a set of nstrs embeddable strings pointed to by strs, produce embeddable strtab at
 * dst of size *nwritten bytes.
 */
bool make_strtab(const uint8_t** strs, size_t nstrs, uint8_t* dst, size_t dst_sz, size_t* nwritten);

/**
 * Given a UTF-8 string from script, convert it to SJIS checking if it would fit in strtab.
 * Handles \x \r \n \\ sequences (YEN_SIGN can also be used as escape character).
 */
char* mk_strtab_str(const char* u8str, iconv_t conv);
#endif
