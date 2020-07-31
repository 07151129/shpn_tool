#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glyph.h"

static void test_break(char* str, size_t* broken_at) {
    while (*broken_at) {
        size_t at = sjis_break_frame_at(str);
        assert(at == *broken_at);
        broken_at++;
        str = &str[at + 1];
    }
}

int main() {
    struct {
        char* str;
        size_t* broken_at;
    } cases[] = {
        {
            "The quick brown fox jumps\nover the lazy dog",
            (size_t[]){0}
        },
        {
            "Text with spaces",
            (size_t[]){0}
        },
        {
            "Lorem ipsum dolor sit amet,\nconsectetur adipisici elit,\nsed do eiusmod tempor\n"
            "incididunt ut labore et\ndolore magna aliqua. Ut\nenim ad minim veniam, qui",
            (size_t[]){125, 0}
        },
        {
            "Longer\nthan\nsix\nrows\nof\ntext\nhere",
            (size_t[]){28, 0}
        },
        {
            "AAAAAAAAAAAAAAAAAAAAA\nAAAAAAAAAAAAAAAAAAAAA\nAAAAAAAAAAAAAAAAAAAAA\n"
            "AAAAAAAAAAAAAAAAAAAAA\nAAAAAAAAAAAAAAAAAAAAA\nAAAAAAAAAAAAAAAAAAAAA\n"
            "BBBBBBBBBBBBBBBBBBBBB\nBBBBBBBBBBBBBBBBBBBBB\nBBBBBBBBBBBBBBBBBBBBB\n"
            "BBBBBBBBBBBBBBBBBBBBB\nBBBBBBBBBBBBBBBBBBBBB\nBBBBBBBBBBBBBBBBBBBBB\n",
            (size_t[]){109, 109, 0}
        }
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++)
        test_break(cases[i].str, cases[i].broken_at);
}
