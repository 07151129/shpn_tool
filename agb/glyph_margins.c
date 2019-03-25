#include <stdbool.h>

#include "glyph_margins.h"

#ifdef FREESTANDING
bool isdigit(char c);
#else
#include <ctype.h>
#endif

static struct glyph_margins margins_az[] = {
    {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {3, 6}, {2, 5}, {2, 5}, {5, 8}, {3, 6}, {3, 5}, /* k */
    {4, 8}, {1, 4}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {4, 6}, {2, 5}, {3, 5}, {2, 5}, {2, 5}, /* v */
    {1, 4}, {2, 5}, {2, 5}, {2, 5} /* z */
};
_Static_assert(sizeof(margins_az) / sizeof(*margins_az) == 'z' - 'a' + 1, "");

static struct glyph_margins margins_AZ[] = {
    {1, 4}, {2, 5}, {2, 4}, {2, 4}, {2, 5}, {2, 5}, {2, 4}, {2, 5}, {5, 8}, {2, 5}, {2, 4}, /* K */
    {2, 5}, {1, 4}, {2, 4}, {2, 4}, {2, 5}, {2, 4}, {2, 5}, {2, 5}, {1, 4}, {2, 4}, {1, 4}, /* V */
    {1, 4}, {1, 4}, {1, 4}, {2, 5} /* Z */
};
_Static_assert(sizeof(margins_AZ) / sizeof(*margins_AZ) == 'Z' - 'A' + 1, "");

static struct glyph_margins margins_cyr_lo[] = {
    {2, 5}, {2, 5}, {2, 5}, {2, 6}, {1, 5}, {2, 5}, {2, 5}, {1, 4}, {2, 5}, {2, 5}, {2, 5}, /* й */
    {2, 6}, {2, 5}, {1, 4}, {2, 5}, {0, 0}, /* Placeholder for 847f */
    {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {3, 5}, {0, 3}, /* ф */
    {2, 5}, {1, 5}, {2, 5}, {1, 4}, {0, 5}, {1, 4}, {1, 5}, {2, 5}, {2, 5}, {1, 4}, {2, 5} /* я */
};
_Static_assert(sizeof(margins_cyr_lo) / sizeof(*margins_cyr_lo) == 33 + 1, "");

static struct glyph_margins margins_cyr_cap[] = {
    {1, 4}, {2, 5}, {2, 5}, {2, 6}, {1, 5}, {2, 5}, {2, 5}, {0, 3}, {2, 5}, {1, 5}, {1, 5}, /* Й */
    {2, 5}, {2, 5}, {1, 4}, {2, 5}, {1, 5}, {2, 5}, {2, 5}, {1, 5}, {2, 5}, {2, 5}, {1, 4}, /* Ф */
    {2, 5}, {1, 5}, {2, 5}, {1, 4}, {0, 4}, {1, 4}, {1, 4}, {2, 5}, {1, 5}, {0, 4}, {2, 5} /* Я */
};
_Static_assert(sizeof(margins_cyr_cap) / sizeof(*margins_cyr_cap) == 33, "");

static struct glyph_margins margins_digit[] = {
    {2, 5}, {5, 8}, {2, 5}, {2, 5}, {2, 4}, {2, 6}, {2, 5}, {2, 5}, {2, 5}, {2, 5}
};
_Static_assert(sizeof(margins_digit) / sizeof(*margins_digit) == '9' - '0' + 1, "");

struct glyph_margins glyph_margin(uint16_t c, bool in_quotes) {
    if ('a' <= c && c <= 'z')
        return margins_az[c - 'a'];
    if ('A' <= c && c <= 'Z')
        return margins_AZ[c - 'A'];
    if (/* a */ 0x8470 <= c && c <= 0x8491 /* я */)
        return margins_cyr_lo[c - 0x8470];
    if (/* А */ 0x8440 <= c && c <= 0x8460 /* Я */)
        return margins_cyr_cap[c - 0x8440];

    if (c == '"')
        c = in_quotes ? 0x8168 : 0x8167;

    /* FIXME: Objects for quotes appear swapped? */
    if (c == 0x8168) /* “ */
        return (struct glyph_margins){0, 9};
    if (c == 0x8167) /* ” */
        return (struct glyph_margins){6, 3};
    if (isdigit(c))
        return margins_digit[c - '0'];
    switch (c) {
        case '!': return (struct glyph_margins){5, 8};
        case '?': return (struct glyph_margins){2, 6};
        case '&': return (struct glyph_margins){2, 5};
        case '(': return (struct glyph_margins){7, 4};
        case ')': return (struct glyph_margins){1, 8};
        case ',': return (struct glyph_margins){0, 12};
        case '.': return (struct glyph_margins){1, 11};
        case '-': return (struct glyph_margins){3, 6};
        case ';': return (struct glyph_margins){4, 8};
        case ':': return (struct glyph_margins){4, 8};
        case '\'': return (struct glyph_margins){0, 12};
    }

    return (struct glyph_margins){0, 0};
}

uint16_t glyph_hw_to_fw(char c, bool in_quotes) {
    if ('a' <= c && c <= 'z')
        return 0x8281 + c - 'a';
    if ('A' <= c && c <= 'Z')
        return 0x8260 + c - 'A';
    if (c == '"')
        return in_quotes ? 0x8168 : 0x8167;
    if (isdigit(c))
        return 0x824f + c - '0';
    switch (c) {
        case '!': return 0x8149;
        case '?': return 0x8148;
        case '&': return 0x8195;
        case '(': return 0x8169;
        case ')': return 0x816a;
        case ',': return 0x8143;
        case '.': return 0x8144;
        case '-': return 0x815d;
        case ';': return 0x8147;
        case ':': return 0x8146;
        case '\'': return 0x8166;
    }
    return c;
}

bool glyph_is_hw(char c) {
    return 0x21 <= c && c <= 0x7a;
}

bool glyph_is_wait_cmd(const char* sjis) {
    return sjis[0] == 'W' && isdigit(sjis[1]);
}
