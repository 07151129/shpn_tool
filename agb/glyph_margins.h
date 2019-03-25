#ifndef GLYPH_MARGINS_H
#define GLYPH_MARGINS_H

#include <stdbool.h>
#include <stdint.h>

struct glyph_margins {
    uint8_t lmargin;
    uint8_t rmargin;
};

struct glyph_margins glyph_margin(uint16_t c, bool in_quotes);
uint16_t glyph_hw_to_fw(char c, bool in_quotes);
bool glyph_is_hw(char c);
bool glyph_is_wait_cmd(const char* sjis);

#endif
