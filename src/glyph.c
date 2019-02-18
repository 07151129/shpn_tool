#include <ctype.h>
#include <limits.h>
#include <stdio.h>

#include "glyph.h"

static uint16_t hw_to_fw(char c) {
    if ('a' <= c && c <= 'z')
        return 0x8281 + c - 'a';
    if ('A' <= c && c <= 'Z')
        return 0x8260 + c - 'A';
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
    }
    return c;
}

bool glyph_csum(uint16_t sjis, uint16_t* ret) {
    uint32_t buf[7];
    buf[0] = 0;
    buf[1] = 0xad;
    buf[2] = 0x15e;
    buf[3] = 0x1f4;
    buf[4] = 0;
    buf[5] = 0;
    buf[6] = 0x272;

    uint32_t first = sjis & UINT8_MAX;
    uint32_t second = sjis & UINT8_MAX;

    /* Interpret as ascii */
    if (0x21 <= first && first <= 0x7a) {
        uint16_t fw = hw_to_fw(first);
        first = fw >> 8;
        second = fw & UINT8_MAX;
    }

    uint32_t offs_first = 0;
    uint32_t offs_second = second - 0x40;

    if (offs_second > 0x3f)
        offs_second = second - 0x41;

    if (first - 0x88 > 0x17) {
        if (first - 0x81 <= 0x5e) {
            if (first - 0x81 >= sizeof(buf) / sizeof(*buf))
                return false;
            offs_first = buf[first - 0x81];
        }
        else
            offs_first = 0x140c + 0xbc * (first - 0xe0) + 4;
    } else
        offs_first = 0xbc * (first - 0x88) + 0x270;

    *ret = offs_first + offs_second;

    return true;
}

void dump_glyphs_csum(FILE* fout) {
    uint16_t i = 0;
    do {
        uint16_t cs;
        if (glyph_csum(i, &cs))
            fprintf(fout, "0x%x: 0x%x\n", i, cs);
    } while (i++ < UINT16_MAX);
}
