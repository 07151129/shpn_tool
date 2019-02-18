#ifndef GLYPH_H
#define GLYPH_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

void dump_glyphs_csum(FILE* fout);
bool glyph_csum(uint16_t sjis, uint16_t* csum);

#endif
