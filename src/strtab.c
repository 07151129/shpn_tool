#include <assert.h>
#include <limits.h>
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

        if (*len >= maxlen)
            return false;

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
                assert(n + 1 == (struct dict_node*)&(((uint8_t*)dict)[n->offs_l]) &&
                    "Left child must follow its parent immediately");
                n++;
                // n = (struct dict_node*)&(((uint8_t*)dict)[n->offs_l]);
            }

            *bits <<= 1;
            *nbits = *nbits + 1;

            if (is_leaf(n))
                goto leaf;
        }

leaf:
        assert(n->offs_l == UINT32_MAX && n->offs_r == UINT32_MAX &&
            "Leaves must have UINT32_MAX offsets");
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

bool strtab_dec_str(const uint8_t* strtab, uint32_t idx, char* out, size_t out_sz, size_t* nwritten,
    iconv_t conv, bool should_conv) {
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

    if (!should_conv) {
        memcpy(out, buf, len + 1);
        return true;
    }

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

        *nwritten = out_sz_old - out_sz + 1;
        out = out_old;
        out[*nwritten - 1] = '\0';
#endif
    } else {
        out[0] = '\0';
        *nwritten = 1;
        for (size_t i = 0; i < len; i++) {
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
            if (!strtab_dec_str(strtab, i, buf, sizeof(buf), &nwritten, conv, true)) {
                fprintf(stderr, "Failed to decode string at %zu\n", i);
                ret = false;
                goto done;
            } else
                fprintf(fout, "%zu: %s\n", i, buf);
    else {
        if (!strtab_dec_str(strtab, idx, buf, sizeof(buf), &nwritten, conv, true)) {
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

#define DICT_SZ_MAX 1000

/* Intermediate dict_node format used for building a dictionary */
static struct dict_node_inter {
    struct dict_node node;
    unsigned long freq;
    bool has_parent;
    size_t parent_idx;
} dict [DICT_SZ_MAX];

static size_t cpy_pre_order(const struct dict_node_inter* root, struct dict_node_inter* dst,
    size_t idx) {
    dst[idx] = *root;

    if (!is_leaf(&root->node)) {
        size_t lidx = cpy_pre_order(&dict[root->node.offs_l], dst, idx + 1);
        size_t ridx = cpy_pre_order(&dict[root->node.offs_r], dst, lidx + 1);

        dst[idx].node.offs_l = idx + 1;
        dst[idx].node.offs_r = lidx + 1;

        dst[idx + 1].parent_idx = idx;
        dst[lidx + 1].parent_idx = idx;

        idx = ridx;
    }

    return idx;
}

static bool make_dict(const uint8_t** strs, size_t nstrs, size_t* nentries) {
    /* We use a temporary frequency array here to make sure the dictionary array isn't sparse */
    uint8_t char_freqs[UINT8_MAX + 1];
    memset(char_freqs, 0, sizeof(char_freqs));

    /* First, make leaves for each char encountered in strs */
    for (size_t i = 0; i < nstrs; i++)
        for (const uint8_t* str = strs[i]; *str; str++)
            char_freqs[(size_t)*str]++;

    /* Add NUL as well */
    char_freqs[0]++;

    size_t dict_nitems = 0;
    for (size_t i = 0; i < sizeof(char_freqs) / sizeof(*char_freqs); i++) {
        if (char_freqs[i]) {
            dict[dict_nitems] = (struct dict_node_inter){
                {.val = i, .offs_l = UINT32_MAX, .offs_r = UINT32_MAX},
                .freq = char_freqs[i]
            };
            dict_nitems++;
        }
    }

    size_t nroots = dict_nitems;

    if (nroots < 2)
        return false;

    while (nroots > 1) {
        /* Find two roots with least frequencies */
        size_t least_idx = 0;
        unsigned long least_freq = ULONG_MAX;

        for (size_t i = 0; i < dict_nitems; i++)
            if (!dict[i].has_parent && dict[i].freq <= least_freq) {
                least_idx = i;
                least_freq = dict[i].freq;
            }

        size_t second_least_idx = 0;
        least_freq = ULONG_MAX;

        for (size_t i = 0; i < dict_nitems; i++)
            if (i != least_idx && !dict[i].has_parent && dict[i].freq <= least_freq) {
                second_least_idx = i;
                least_freq = dict[i].freq;
            }

        if (dict_nitems + 1 > DICT_SZ_MAX)
            return false;

        /* Merge the two roots */
        dict[dict_nitems] = (struct dict_node_inter){
            .node = {.tag = UINT32_MAX, .offs_l = least_idx, .offs_r = second_least_idx},
            .freq = dict[least_idx].freq + dict[second_least_idx].freq
        };

        dict[least_idx].has_parent = true;
        dict[least_idx].parent_idx = dict_nitems;

        dict[second_least_idx].has_parent = true;
        dict[second_least_idx].parent_idx = dict_nitems;

        dict_nitems++;
        nroots = nroots - 2 + 1;
    }

    assert(!dict[dict_nitems - 1].has_parent);

    /**
     * FIXME: Look into creating dictionary in pre-order immediately, or at least doing this in
     * place
     */
    struct dict_node_inter* dict_pre_order = malloc(sizeof(struct dict_node_inter[dict_nitems]));
    if (!dict_pre_order)
        return false;

    cpy_pre_order(&dict[dict_nitems - 1], dict_pre_order, 0);
    memcpy(dict, dict_pre_order, sizeof(struct dict_node_inter[dict_nitems]));

    free(dict_pre_order);

    *nentries = dict_nitems;
    return true;
}

static void dump_dict(const struct dict_node_inter* root) {
    // if (root->has_parent)
    //     fprintf(stderr, "0x%zx -> ", root->parent_idx);
    fprintf(stderr, "0x%lx: ", root - dict);
    if (!is_leaf(&root->node)) {
        fprintf(stderr, "0x%x, 0x%x\n", root->node.offs_l, root->node.offs_r);
        dump_dict(&dict[root->node.offs_l]);
        dump_dict(&dict[root->node.offs_r]);
    }
    else
        fprintf(stderr, "%c\n", root->node.val);
}

#define NBYTES_PER_CHAR_MAX 2
struct char_bits {
    uint8_t bytes[NBYTES_PER_CHAR_MAX];
    size_t nbits;
};

static_assert(CHAR_BIT == 8, "Where are we?");

static bool bits_for_char(char c, struct char_bits* dst, size_t nbytes_max, size_t dict_sz) {
    bool leaf_found = false;
    size_t leaf_idx = 0;

    for (size_t i = 0; i < dict_sz; i++)
        if (is_leaf(&dict[i].node) && dict[i].node.val == c) {
            leaf_found = true;
            leaf_idx = i;
            break;
        }

    /* As we never attempt to encode a character that is not a leaf, it's safe to do this */
    if (!leaf_found)
        return true;

    dst->nbits = 0;

    assert(dict[leaf_idx].has_parent);

    while (dict[leaf_idx].has_parent) {
        if (dst->nbits / 8 > nbytes_max)
            return false;

        size_t byte_idx = dst->nbits / 8;

        const struct dict_node_inter* parent = &dict[dict[leaf_idx].parent_idx];
        if (parent->node.offs_l == leaf_idx)
            dst->bytes[byte_idx] &= ~(uint8_t)(1 << (dst->nbits % 8));
        else
            dst->bytes[byte_idx] |= 1 << (dst->nbits % 8);

        leaf_idx = dict[leaf_idx].parent_idx;
        dst->nbits++;
    }

    // fprintf(stderr, "%c: 0x%x, nbits = %zu\n", c, dst->bytes[0], dst->nbits);

    return true;
}

/* FIXME: Need to check somewhere if strlen of each part until \n <= 512 */
bool make_strtab(const uint8_t** strs, size_t nstrs, uint8_t* dst, size_t dst_sz, size_t* nwritten) {
    size_t dict_nentries;
    size_t dst_sz_init = dst_sz;

    if (!make_dict(strs, nstrs, &dict_nentries))
        return false;

    // dump_dict(dict);

    if (dict_nentries * sizeof(struct dict_node) + sizeof(struct strtab_header) > dst_sz)
        return false;

    struct char_bits bits_for_chars[UINT8_MAX + 1];

    for (size_t i = 0; i < nstrs; i++)
        for (const uint8_t* str = strs[i]; ; str++) {
            if (!bits_for_char(*str, &bits_for_chars[(size_t)*str], NBYTES_PER_CHAR_MAX, dict_nentries))
                return false;
            if (!*str)
                break;
        }

    /* Now that we have our dict, check if header + all the entries fit */
    if (dst_sz < sizeof(struct strtab_header) + sizeof(struct dict_node) * dict_nentries)
        return false;

    *(struct strtab_header*)dst = (struct strtab_header){
        .dict_offs = sizeof(struct strtab_header),
        .msgs_offs = sizeof(struct strtab_header) + dict_nentries * sizeof(struct dict_node),
        .nentries = nstrs
    };

    for (size_t i = 0; i < dict_nentries; i++) {
        if (!is_leaf(&dict[i].node)) {
            /* Fix references to be byte offsets instead of offsets dict_node_inter[] */
            dict[i].node.offs_l *= sizeof(dict[i].node);
            dict[i].node.offs_r *= sizeof(dict[i].node);
        }

        memcpy(dst + sizeof(struct strtab_header) + i * sizeof(struct dict_node), &dict[i].node,
                sizeof(struct dict_node));
    }

    dst_sz -= dict_nentries * sizeof(struct dict_node) + sizeof(struct strtab_header);

    if (3 * dict_nentries > dst_sz)
        return false;

    uint8_t* msg_offsets = dst + ((struct strtab_header*)dst)->msgs_offs;
    uint8_t* msg = msg_offsets + 3 * nstrs;

    for (size_t i = 0; i < nstrs; i++) {
        size_t nbits = 0;
        uint8_t val = 0;

        if (dst_sz < 3)
            return false;

        /* Write a three-byte offset from msgs to the message */
        uint32_t msg_offs = (msg - msg_offsets) & 0xffffff;

        if (msg_offs > MSG_OFFS_MAX)
            return false;

        memcpy(msg_offsets + 3 * i, &msg_offs, 3);
        dst_sz -= 3;

        assert(strs[i]);

        /* For each char in str, make and write bytes out of its encoding bits */
        for (const uint8_t* str = strs[i]; ; str++) {
            struct char_bits bits = bits_for_chars[(size_t)*str];

            for (size_t j = 0; j < bits.nbits; j++) {
                size_t bit_pos = bits.nbits - j % 8 - 1;

                // fprintf(stderr, "%c: bits.bytes[%zu] & (1 << %zu) = 0x%x\n", *str, j/8, bit_pos,
                    // bits.bytes[j / 8] & (1 << bit_pos));

                if (bits.bytes[j / 8] & (1 << bit_pos))
                    val |= 1;

                nbits++;

                // fprintf(stderr, "val(%c) = 0x%x, nbits = %zu\n", *str, val, nbits);

                if (nbits > 0 && nbits % 8 == 0) {
                    // fprintf(stderr, "writing val=0x%x\n", val);
                    *msg++ = val;
                    if (dst_sz < 1)
                        return false;
                    val = 0;
                    dst_sz--;
                }

                val <<= 1;
            }

            if (*str == '\0')
                break;
        }

        /* Pad last byte with zeroes and write it if needed */
        if (nbits % 8 != 0) {
            val >>= 1;
            val <<= 8 - nbits % 8;
            if (dst_sz < 1)
                return false;
            *msg++ = val;
            dst_sz--;
        }

        // fprintf(stderr, "encoded as 0x%x\n", *(uint8_t*)(&msg_offsets[msg_offs]));
        // fprintf(stderr, "Writing msg at offs 0x%x\n", msg_offs & 0xfff);
    }

    *nwritten = dst_sz_init - dst_sz + 1;
    return true;
}
