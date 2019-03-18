#ifndef GLYPH_MARGINS_H
#define GLYPH_MARGINS_H

#include <stdint.h>

struct glyph_margins {
    uint8_t lmargin;
    uint8_t rmargin;
};

struct glyph_margins glyph_margin(uint16_t c);

#endif
