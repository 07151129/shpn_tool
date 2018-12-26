#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef HAS_ICONV
#include <iconv.h>
#endif

#include "defs.h"
#include "strtab.h"

/**
 * A modified variant of strdec by vit9696.
 */

struct strtab_header {
    uint32_t dict_offs; /* struct dict_node[] */
    uint32_t msgs_offs; /* bit[][] */
    uint32_t nentries;
    uint32_t null;
};

static_assert(sizeof(struct strtab_header) == sizeof(uint32_t[4]), "");

struct dict_node {
    union {
        uint32_t tag;
        char val;
    };
    uint32_t unused;
    uint32_t offs_l;
    uint32_t offs_r;
};

static_assert(sizeof(struct dict_node) == sizeof(uint32_t[4]), "");

static bool is_leaf(const struct dict_node* n) {
    return n->tag != UINT32_MAX;
}

static
bool strtab_dec_msg(const struct dict_node* dict, const uint8_t** msg, uint8_t* bits, int* nbits,
    char* dst, size_t* len, size_t maxlen, int* err) {
    assert(!is_leaf(dict));

    while (true) { /* While msg not decoded */
        const struct dict_node* n = dict;

        assert(*len < maxlen);

err:
        if (*len >= maxlen) {
            *err = 1;
            *dst = '\0';
            return false;
        }

        while (true) { /* While leaf not reached */
            if (*nbits == 8) {
                *nbits = 0;
                *msg = *msg + 1;
                *bits = **msg;
            }

            // fprintf(stderr, "bits: 0x%x\n", *bits);
            if (*bits & 0x80)
                n = (struct dict_node*)&(((uint8_t*)dict)[n->offs_r]);
            else {
                assert(n + 1 == (struct dict_node*)&(((uint8_t*)dict)[n->offs_l]));
                n++;
            }

            *bits <<= 1;
            *nbits = *nbits + 1;

            if (is_leaf(n))
                goto leaf;
        }

leaf:
        /* NOTE: The original implementation replaces \n to \r\0 for rendering, but we don't care */
        if (n->val == '\0' || n->val == '\n') {
            /* Replace \n with \\n in SJIS (will be half-width YEN_SIGN in UTF-8) */
            if (n->val == '\n') {
                if (*len + 2 >= maxlen)
                    goto err;
                *dst++ = '\\';
                *dst++ = 'n';
                *len += 2;
            }

            /* If a message was terminated by newline, we have some more to decode */
            return n->val == '\n';
        }

        *dst++ = n->val;
        *len = *len + 1;
    }
}

bool sjis_to_u8(char *output, size_t szoutput, const char *input, size_t* nwritten);

bool strtab_dec_str(const uint8_t* strtab, uint32_t idx, char* out, size_t out_sz, size_t* nwritten,
    iconv_t conv) {
    const struct strtab_header* hdr = (const struct strtab_header*)strtab;
    assert(hdr->dict_offs == sizeof(struct strtab_header));
    assert(hdr->null == 0);

    if (idx > hdr->nentries)
        return false;

    char buf[DEC_BUF_SZ_SJIS];

    if (out_sz < 3 * sizeof(buf) + 1)
        return false;

    size_t len = 0;

    uint32_t msg_idx = 0;
    memcpy(&msg_idx, &strtab[hdr->msgs_offs + 3 * idx], 3);

    const uint8_t* msg = &strtab[hdr->msgs_offs + msg_idx];
    uint8_t bits = *msg;
    int nbits = 0;
    int err = 0;

    while (strtab_dec_msg((struct dict_node*)&strtab[hdr->dict_offs], &msg, &bits, &nbits, &buf[len],
        &len, sizeof(buf), &err)) {
        /* Do not continue if we're out of space */
        if (err) {
            buf[len] = 0;
            return false;
        }
    }

    buf[len] = 0;

    /* return len > 0 && */ /* Apparently, they're OK with empty strings */

    size_t cstatus = 0;

    if (conv != (iconv_t)-1) {
#ifdef HAS_ICONV
        size_t out_sz_old = out_sz;
        char* out_old = out;
        char* outp = out;
        char* bufp = buf;

        /* Reset state */
        iconv(conv, NULL, NULL, NULL, NULL);

        cstatus = iconv(conv, (void*)&bufp, &len, &outp, &out_sz);

        *nwritten = out_sz_old - out_sz;
        out = out_old;
        out[*nwritten]='\0';
#endif
    } else {
        out[0] = '\0';
        *nwritten = 1;
        for (size_t i = 0; i < len; i++) {
            if (*nwritten >= out_sz)
                break;
            if (*nwritten + sizeof("\\xff") > out_sz) {
                cstatus = (size_t)-1;
                break;
            }
            *nwritten += sprintf(&out[*nwritten - 1], "\\x%02x", (uint8_t)buf[i]);
        }
    }
    return cstatus != (size_t)-1;
}

bool strtab_dump(const uint8_t* rom, uint32_t vma, uint32_t idx, bool has_idx, FILE* fout) {
    const void* strtab = &rom[VMA2OFFS(vma)];

    iconv_t conv = (iconv_t)-1;
#ifdef HAS_ICONV
    conv = iconv_open("UTF-8", "SJIS");
#endif

    if (conv == (iconv_t)-1) {
#ifdef HAS_ICONV
        perror("iconv_open");
#endif
        fprintf(stderr, "iconv_open failed; will dump raw values\n");
    }

    bool ret = true;

    char buf[SJIS_TO_U8_MIN_SZ(DEC_BUF_SZ_SJIS)];
    size_t nwritten = 0;
    if (!has_idx)
        for (size_t i = 0; i < ((const struct strtab_header*)strtab)->nentries; i++)
            if (!strtab_dec_str(strtab, i, buf, sizeof(buf), &nwritten, conv)) {
                fprintf(stderr, "Failed to decode string at %zu\n", i);
                ret = false;
                goto done;
            } else
                fprintf(fout, "%zu: %s\n", i, buf);
    else {
        if (!strtab_dec_str(strtab, idx, buf, sizeof(buf), &nwritten, conv)) {
            fprintf(stderr, "Failed to decode string at %u\n", idx);
            ret = false;
            goto done;
        } else
            fprintf(fout, "%u: %s\n", idx, buf);
    }

done:
    if (conv != (iconv_t)-1) {
        if (!ret)
            perror("iconv");
        iconv_close(conv);
    }
    return ret;
}
