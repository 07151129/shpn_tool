#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "strtab.h"

static void make_and_cmp(const char** strs, size_t nstrs) {
    static uint8_t strtab[10000];

    size_t nwritten;
    assert(
        make_strtab((void*)strs, nstrs, strtab, sizeof(strtab), &nwritten) &&
        "Failed to make strtab");

    // strtab_dump(strtab, ROM_BASE, 0, false, stderr);

    static char dec_buf[10000];
    for (size_t i = 0; i < nstrs; i++) {
        assert(
            strtab_dec_str(strtab, i, dec_buf, sizeof(dec_buf), &nwritten, (iconv_t)-1, false) &&
            "Failed to decode string");
        assert(!strncmp(strs[i], dec_buf, nwritten) && "Strings mismatch");
    }
}

int main() {
    make_and_cmp((const char* []){"Some", "ASCII", "strings"}, 3);

    static char* sjis_text[] = {"\x83\x66\x81\x5b\x83\x5e\x82\xaa\x82\xa0\x82\xe8\x82\xdc\x82\xb9"
                                "\x82\xf1\x81\x42\x5c\x6e\x96\x94\x82\xcd\x93\xfc\x97\xcd\x82\xc9"
                                "\x8c\xeb\x82\xe8\x82\xaa\x82\xa0\x82\xe8\x82\xdc\x82\xb7\x81\x42",
                                "Blah",
                                "\x91\xe6\x88\xea\x8f\xcd\x81\x40\x81\x40\x81\x75\x95\xa1\x90\x94"
                                "\x82\xcc\x91\xab\x89\xb9\x81\x76"};

    make_and_cmp((void*)sjis_text, 2);
}
