#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
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

    bool in_quotes = false;

    for (size_t i = 0; sjis[i];) {
        uint32_t first = sjis[i] & UINT8_MAX;

        if (first == ' ' || first == '\n' || glyph_is_wait_cmd(&sjis[i]))
            break;

        /* Known single-byte half-width char */
        if (glyph_is_hw(first)) {
            margins = glyph_margin(first, in_quotes);
            if (sjis[i] == '"')
                in_quotes = !in_quotes;
            i++;
        } else if (sjis[i + 1]) { /* Possibly known two-byte full-width */
            uint32_t second = sjis[i + 1] & UINT8_MAX;

            margins = glyph_margin((first << 8) | second, in_quotes);
            i += 2;
        } else { /* Unknown single-byte; skip */
            i++;
            continue;
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

    for (size_t i = 0; sjis[i] && (i == 0 || sjis[i + 1]);) {
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
            xoffs = RENDER_TEXT_LMARGIN;
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
        while (sjis[i] && sjis[i] != ' ' && sjis[i] != '\n')
            if (!glyph_is_hw(sjis[i]) && sjis[i + 1])
                i += 2;
            else
                i++;
    }
}

/* We can display max(6 rows, RENDER_NCHARS_MAX glyphs). The latter is more likely. */
size_t sjis_break_frame_at(const char* sjis) {
    size_t i = 0;
    size_t nchars = 0;
    size_t nrows = 0;
    size_t nl_at = 0;

    while (sjis[i] && (i == 0 || sjis[i + 1])) {
        if (sjis[i] == ' ') {
            i++;
            continue;
        }

        if (glyph_is_wait_cmd(&sjis[i])) {
            i += 2;
            continue;
        }

        if (sjis[i] == '\n') {
            nl_at = i;

            if (++nrows == RENDER_NROWS_MAX)
                return i;

            i++;
            continue;
        }

        if (!glyph_is_hw(sjis[i]) && sjis[i + 1])
            i += 2;
        else
            i++;

        if (++nchars == RENDER_NCHARS_MAX)
            return nl_at;
    }

    return 0;
}

size_t sjis_nglyphs(const char* sjis) {
    size_t nglyphs = 0, i = 0;

    while (sjis[i] && (i == 0 || sjis[i + 1])) {
        if (sjis[i] == ' ') {
            i++;
            continue;
        }

        if (glyph_is_wait_cmd(&sjis[i])) {
            i += 2;
            continue;
        }

        if (sjis[i] == '\n') {
            i++;
            continue;
        }

        if (!glyph_is_hw(sjis[i]) && sjis[i + 1])
            i += 2;
        else
            i++;
        nglyphs++;
    }

    return nglyphs;
}

size_t sjis_nrows(const char* sjis) {
    size_t nrows = 1, i = 0;

    while (sjis[i] && (i == 0 || sjis[i + 1])) {
        if (sjis[i] == ' ') {
            i++;
            continue;
        }

        if (glyph_is_wait_cmd(&sjis[i])) {
            i += 2;
            continue;
        }

        if (sjis[i] == '\n') {
            i++;
            nrows++;
            continue;
        }

        if (!glyph_is_hw(sjis[i]) && sjis[i + 1])
            i += 2;
        else
            i++;
    }

    return nrows;
}
