#ifndef GLYPH_H
#define GLYPH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void hard_wrap_sjis(char* sjis);
size_t sjis_break_frame_at(const char* sjis);
size_t sjis_nglyphs(const char* sjis);
size_t sjis_nrows(const char* sjis);

#endif
