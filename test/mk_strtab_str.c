#undef NDEBUG
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "strtab.h"

int main() {
    static struct {const char* test, * expected;} strs[] = {
        {"No escape", "No escape"},
        {"Has a\\nnewline", "Has a\nnewline"},
        {"Has a\\x0anewline as well", "Has a\x0anewline as well"},
        {u8"Has a¥nnewline", "Has a\nnewline"},
        {u8"Has a¥x0anewline as well", "Has a\x0anewline as well"}
    };

    iconv_t conv;
#ifdef HAS_ICONV
    conv = iconv_open("SJIS", "UTF-8");
    if (!conv) {
        perror("iconv");
        assert(false);
    }
#else
    assert(false && "iconv needed for this test");
#endif

    for (size_t i = 0; i < sizeof(strs) / sizeof(*strs); i++) {
        char* ret = mk_strtab_str(strs[i].test, conv);
        assert(ret);
        assert(!strcmp(ret, strs[i].expected));
    }

    iconv_close(conv);
}
