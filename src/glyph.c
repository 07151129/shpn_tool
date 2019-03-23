#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

#include "agb/config.h"
#include "agb/glyph_margins.h"
#include "glyph.h"

static unsigned word_end(const char* sjis, unsigned xoffs) {
    /* Maintain starting coordinate of a glyph */
    unsigned ret = xoffs;

    struct glyph_margins margins = {0, 0};
    uint8_t rmargin_prev = 0;

    for (size_t i = 0; sjis[i];) {
        uint32_t first = sjis[i] & UINT8_MAX;
        uint32_t second = sjis[i + 1] & UINT8_MAX;

        if (first == ' ' || first == '\n' || glyph_is_wait_cmd(&sjis[i]))
            break;

        if (glyph_is_hw(first)) {
            margins = glyph_margin(first);
            i++;
        } else {
            margins = glyph_margin((first << 8) | second);
            i += 2;
        }

        if (ret > RENDER_TEXT_LMARGIN)
            ret -= margins.lmargin + rmargin_prev;

        ret += RENDER_GLYPH_DIM;

        rmargin_prev = margins.rmargin;
    }

    /* Return end coordinate of last glyph */
    return ret + margins.lmargin - rmargin_prev;
}

void hard_wrap_sjis(char* sjis) {
    size_t prev_space = 0;
    unsigned xoffs = RENDER_TEXT_LMARGIN;

    for (size_t i = 0; sjis[i];) {
        if (sjis[i] == ' ') {
            prev_space = i++;
            xoffs += RENDER_SPACE_W;
            continue;
        }

        if (glyph_is_wait_cmd(&sjis[i])) {
            i += 2;
            continue;
        }

        if (sjis[i] == '\n') {
            prev_space = 0;
            i++;
            continue;
        }

        unsigned end = word_end(&sjis[i], xoffs);

        // fprintf(stderr, "i: %zu, xoffs %u end at %u\n", i, xoffs, end);
        xoffs = end;

        if (end - RENDER_GLYPH_DIM >= RENDER_TEXT_RMARGIN && prev_space != 0) {
            sjis[prev_space] = '\n';
            xoffs = RENDER_TEXT_LMARGIN;
            // fprintf(stderr, "\n");

            continue;
        }

        /* Next word */
        while (sjis[i] && sjis[i] != ' ')
            if (glyph_is_hw(sjis[i]))
                i++;
            else
                i += 2;
    }
}
