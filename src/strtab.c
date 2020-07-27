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
#define _GNU_SOURCE
#include "search.h"
#undef _GNU_SOURCE
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

static const char* esc_for_buf(uint8_t val) {
    switch (val) {
        case '\n':
            return "\\n";
        case '\r':
            return "\\r";
        case '"':
            return "\\\"";
        default:
            return NULL;
    };
}

static
bool strtab_dec_msg(const struct dict_node* dict, const uint8_t** msg, uint8_t* bits, int* nbits,
    char* dst, size_t* len, size_t maxlen, int* err, const uint8_t* rom_end) {
    assert(!is_leaf(dict));

    while (true) { /* While msg not decoded */
        const struct dict_node* n = dict;

        if (*len >= maxlen) {
err:
            *err = 1;
            return false;
        }

        while (true) { /* While leaf not reached */
            if (*nbits == 8) {
                *nbits = 0;
                *msg = *msg + 1;
                if (*msg > rom_end)
                    goto err;
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

            if ((uint8_t*)n > rom_end - sizeof(struct dict_node))
                goto err;

            *bits <<= 1;
            *nbits = *nbits + 1;

            if (is_leaf(n))
                goto leaf;
        }

leaf:
        // fprintf(stderr, "decoded leaf 0x%x len %zu\n", n->val & 0xff, *len);
        assert(n->offs_l == UINT32_MAX && n->offs_r == UINT32_MAX &&
            "Leaves must have UINT32_MAX offsets");

        if (esc_for_buf(n->val)) {
            if (*len + 2 >= maxlen)
                goto err;
            memcpy(dst, esc_for_buf(n->val), 2);
            dst += 2;
            *len += 2;

            if (n->val == '\n')
                return true;

            continue;
        }

        if (n->val == '\0')
            return false;

        *dst++ = n->val;
        *len = *len + 1;
    }
}

#define MSG_OFFS_SZ 3

static bool chk_hdr(const struct strtab_header* hdr, const uint8_t* rom_end) {
    assert(hdr->dict_offs == sizeof(struct strtab_header));
    assert(hdr->null == 0);

    return (size_t)rom_end >= sizeof(*hdr) &&
        (size_t)rom_end >= hdr->dict_offs &&
        (size_t)rom_end >= hdr->msgs_offs &&
        (uint8_t*)hdr < rom_end - sizeof(*hdr) &&
        (uint8_t*)hdr < rom_end - hdr->dict_offs &&
        (uint8_t*)hdr < rom_end - hdr->msgs_offs;
}

/**
 * We don't know size of strtab until we have reached last byte of last msg, but for a malformed
 * strtab finding it can lead to an out-of-bounds read. We thus use rom_end to at least ensure
 * we stay within the ROM buffer.
 */
bool strtab_dec_str(const uint8_t* strtab, const uint8_t* rom_end, uint32_t idx, char* out,
    size_t out_sz, size_t* nwritten, iconv_t conv, bool should_conv) {
    const struct strtab_header* hdr = (const struct strtab_header*)strtab;

    if (!chk_hdr(hdr, rom_end))
        return false;

    if (idx >= hdr->nentries)
        return false;

    char buf[DEC_BUF_SZ_SJIS];

    if (out_sz < SJIS_TO_U8_MIN_SZ(sizeof(buf))) {
        fprintf(stderr, "Decode buffer too short\n");
        return false;
    }

    size_t len = 0;

    const uint8_t* msg_offsp = &strtab[hdr->msgs_offs + MSG_OFFS_SZ * idx];
    if (msg_offsp >= rom_end) {
        fprintf(stderr, "Past ROM end msg_offsp\n");
        return false;
    }

    uint32_t msg_offs = 0;
    memcpy(&msg_offs, msg_offsp, MSG_OFFS_SZ);

    const uint8_t* msg = &strtab[hdr->msgs_offs + msg_offs];
    const struct dict_node* dict = (void*)&strtab[hdr->dict_offs];
    if (msg > rom_end || (uint8_t*)dict > rom_end) {
        fprintf(stderr, "msg or dict past rom end\n");
        return false;
    }

    uint8_t bits = *msg;
    int nbits = 0;
    int err = 0;

    while (strtab_dec_msg(dict, &msg, &bits, &nbits, &buf[len], &len, sizeof(buf), &err, rom_end))
        ;
    /* Do not continue if we're out of space */
    if (err)
        return false;

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

bool strtab_dump(const uint8_t* rom, size_t rom_sz, uint32_t vma, uint32_t idx, bool has_idx,
    FILE* fout) {
    if (VMA2OFFS(vma) >= rom_sz) {
        fprintf(stderr, "Past EOF strtab vma 0x%x\n", vma);
        return false;
    }
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
            if (!strtab_dec_str(strtab, rom + rom_sz, i, buf, sizeof(buf), &nwritten, conv, true)) {
                fprintf(stderr, "Failed to decode string at %zu\n", i);
                ret = false;
                goto done;
            } else
                fprintf(fout, "%zu: %s\n", i, buf);
    else {
        if (!strtab_dec_str(strtab, rom + rom_sz, idx, buf, sizeof(buf), &nwritten, conv, true)) {
            fprintf(stderr, "Failed to decode string at %u\n", idx);
            ret = false;
            goto done;
        } else
            fprintf(fout, "%u: %s\n", idx, buf);
    }

done:
    if (conv != (iconv_t)-1) {
        if (!ret) {
            if (idx >= ((const struct strtab_header*)strtab)->nentries)
                fprintf(stderr, "Index %u is too large (table has %u entries)\n", idx,
                    ((const struct strtab_header*)strtab)->nentries);
            else
                perror("iconv");
        }
        iconv_close(conv);
    }
    return ret;
}

/* Upper bound for the original script strtab */
#define DICT_SZ_MAX 500

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

static uint32_t adds32(uint32_t a, uint32_t b) {
    uint32_t c = a + b;

    if (c < a)
        c = -1;
    return c;
}

static bool make_dict(const uint8_t** strs, size_t nstrs, size_t* nentries) {
    /* We use a temporary frequency array here to make sure the dictionary array isn't sparse */
    static uint32_t char_freqs[UINT8_MAX + 1];
    memset(char_freqs, 0, sizeof(char_freqs));

    /* First, make leaves for each char encountered in strs */
    for (size_t i = 0; i < nstrs; i++) {
        for (const uint8_t* str = strs[i]; *str; str++) {
            uint8_t freq = char_freqs[(size_t)*str];
            char_freqs[(size_t)*str] = adds32(freq, 1);
        }
        /* Add NUL as well */
        char_freqs[0] = adds32(char_freqs[0], 1);
    }

    size_t dict_nitems = 0;
    for (size_t i = 0; i < sizeof(char_freqs) / sizeof(*char_freqs); i++) {
        if (char_freqs[i]) {
            dict[dict_nitems] = (struct dict_node_inter){
                {.val = i, .offs_l = UINT32_MAX, .offs_r = UINT32_MAX},
                .freq = char_freqs[i],
                .has_parent = false
            };
            if (dict_nitems + 1 >= DICT_SZ_MAX)
                return false;
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

static UNUSED void dump_dict(const struct dict_node_inter* root) {
    // if (root->has_parent)
    //     fprintf(stderr, "0x%zx -> ", root->parent_idx);
    fprintf(stderr, "0x%lx: ", root - dict);
    if (!is_leaf(&root->node)) {
        fprintf(stderr, "0x%x, 0x%x\n", root->node.offs_l, root->node.offs_r);
        dump_dict(&dict[root->node.offs_l]);
        dump_dict(&dict[root->node.offs_r]);
    }
    else
        fprintf(stderr, "0x%x\n", root->node.val & UINT8_MAX);
}

#define NBYTES_PER_CHAR_MAX 2
struct char_bits {
    uint8_t bytes[NBYTES_PER_CHAR_MAX];
    int nbits;
};

static_assert(CHAR_BIT == 8, "Where are we?");

static bool bits_for_char(char c, struct char_bits* dst, size_t dict_sz) {
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
        size_t byte_idx = dst->nbits / 8;
        if (byte_idx >= NBYTES_PER_CHAR_MAX)
            return false;

        const struct dict_node_inter* parent = &dict[dict[leaf_idx].parent_idx];
        if (parent->node.offs_l == leaf_idx)
            dst->bytes[byte_idx] &= ~(uint8_t)(1 << (dst->nbits % 8));
        else
            dst->bytes[byte_idx] |= 1 << (dst->nbits % 8);

        leaf_idx = dict[leaf_idx].parent_idx;
        dst->nbits++;
    }

    return true;
}

bool make_strtab(const uint8_t** strs, size_t nstrs, uint8_t* dst, size_t dst_sz, size_t* nwritten) {
    size_t dict_nentries;
    size_t dst_sz_init = dst_sz;

    if (nstrs == 0) {
        fprintf(stderr, "Cannot encode zero strings\n");
        return false;
    }

    struct hsearch_data msgs_htab;
    memset(&msgs_htab, '\0', sizeof(msgs_htab));
    if (hcreate_r(nstrs /* worst case */, &msgs_htab) == 0) {
        perror("hcreate");
        return false;
    }

    if (!make_dict(strs, nstrs, &dict_nentries)) {
        fprintf(stderr, "Failed to create dictionary\n");
        goto fail;
    }

    // dump_dict(dict);

    if (dict_nentries * sizeof(struct dict_node) + sizeof(struct strtab_header) > dst_sz) {
        fprintf(stderr, "Out of space writing dictionary\n");
        goto fail;
    }

    struct char_bits bits_for_chars[UINT8_MAX + 1];
    for (size_t i = 0; i < sizeof(bits_for_chars) / sizeof(*bits_for_chars); i++)
        bits_for_chars[i].nbits = 0;

    for (size_t i = 0; i < nstrs; i++)
        for (const uint8_t* str = strs[i]; ; str++) {
            if (!bits_for_char(*str, &bits_for_chars[(size_t)*str], dict_nentries)) {
                fprintf(stderr, "Failed to encode char 0x%x\n", *str & UINT8_MAX);
                goto fail;
            }

            if (!*str)
                break;
        }

    /* Now that we have our dict, check if header + all the entries fit */
    if (dst_sz < sizeof(struct strtab_header) + sizeof(struct dict_node) * dict_nentries)
        return false;

    memcpy(dst, &(struct strtab_header){
            .dict_offs = sizeof(struct strtab_header),
            .msgs_offs = sizeof(struct strtab_header) + dict_nentries * sizeof(struct dict_node),
            .nentries = nstrs
        }, sizeof(struct strtab_header));

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

    size_t nstrs_unique = 0;
    for (size_t i = 0; i < nstrs; i++) {
        ENTRY query = {.key = (void*)strs[i], .data = NULL};
        ENTRY* entry;
        hsearch_r(query, FIND, &entry, &msgs_htab);
        if (!entry)
            nstrs_unique++;
        if (hsearch_r(query, ENTER, &entry, &msgs_htab) == 0) {
            perror("hsearch");
            goto fail;
        }
    }

#define MSG_OFFS_MAX ((1 << (8 * MSG_OFFS_SZ)) - 1)

    if (MSG_OFFS_SZ * nstrs_unique > dst_sz) {
        fprintf(stderr, "Out of space writing message offsets\n");
        goto fail;
    }

    uint8_t* msg_offsets = dst + ((struct strtab_header*)dst)->msgs_offs;
    uint8_t* msg = msg_offsets + MSG_OFFS_SZ * nstrs;

    for (size_t i = 0; i < nstrs; i++) {
        size_t nbits = 0;
        uint8_t val = 0;

        ENTRY query = {.key = (void*)strs[i], .data = NULL};
        ENTRY* entry;
        hsearch_r(query, FIND, &entry, &msgs_htab);

        assert(entry && "String not found in msgs_htab");

        /* Write a three-byte offset from msgs to the message */
        uint32_t msg_offs = (msg - msg_offsets) & MSG_OFFS_MAX;
        if (entry->data) /* Already encoded */
            msg_offs = ((uint8_t*)entry->data - msg_offsets) & MSG_OFFS_MAX;

        if (msg_offs > MSG_OFFS_MAX) {
            fprintf(stderr, "Message offset 0x%x is too large to be encoded\n", msg_offs);
            goto fail;
        }

        memcpy(msg_offsets + MSG_OFFS_SZ * i, &msg_offs, MSG_OFFS_SZ);
        dst_sz -= MSG_OFFS_SZ;

        if (entry->data)
            continue;

        entry->data = (void*)msg;

        /* For each char in str, make and write bytes out of its encoding bits */
        for (const uint8_t* str = strs[i]; ; str++) {
            struct char_bits bits = bits_for_chars[(size_t)*str];

            assert(bits.nbits > 0 && "Invalid code for character");

            for (int j = 0; j < bits.nbits; j++) {
                size_t bit_pos = (bits.nbits - j % 8 - 1) % 8;

                assert((size_t)j / 8 <= sizeof(bits.bytes));

                // fprintf(stderr, "%c: bits.bytes[%zu] & (1 << %zu) = 0x%x\n", *str, j/8, bit_pos,
                    // bits.bytes[j / 8] & (1 << bit_pos));

                if (bits.bytes[(bits.nbits - 1 - j) / 8] & (1 << bit_pos))
                    val |= 1;

                nbits++;

                // fprintf(stderr, "val(0x%x) = 0x%x, nbits = %zu\n", *str & UINT8_MAX, val, nbits);

                if (nbits > 0 && nbits % 8 == 0) {
                    // fprintf(stderr, "writing val=0x%x\n", val);
                    if (dst_sz < 1) {
                        fprintf(stderr, "Out of space writing bits for string at %zu\n", i);
                        return false;
                    }

                    *msg++ = val;
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
            // fprintf(stderr, "writing (padded with %zu) val=0x%x\n", 8 - nbits % 8, val);
            dst_sz--;
        }

        // fprintf(stderr, "encoded as 0x%x\n", *(uint8_t*)(&msg_offsets[msg_offs]));
        // fprintf(stderr, "Writing msg at offs 0x%x\n", msg_offs & 0xfff);
    }

#undef MSG_OFFS_MAX

    hdestroy_r(&msgs_htab);
    *nwritten = dst_sz_init - dst_sz + 1;
    return true;
fail:
    hdestroy_r(&msgs_htab);
    return false;
}

static bool is_esc(const char* s) {
    return *s == '\\' || !strncmp(s, u8"¥", 2);
}

static size_t esclen(const char* s) {
    assert(is_esc(s));

    if (*s == '\\')
        return 1;
    return 2;
}

/* Offset of NUL if not found */
static size_t next_esc(const char* str) {
    size_t i = 0;
    for (; str[i]; i++)
        if (is_esc(&str[i]))
            return i;
    return i;
}

static const char* buf_for_esc(const char* esc, size_t* cons, size_t* prod) {
    size_t el = esclen(esc);
    esc += el;
    if (*esc == '\0')
        return NULL;

    if (*esc == 'n') {
        *cons = 1 + el;
        *prod = 1;
        return "\n";
    } else if (*esc == 'r') {
        *cons = 1 + el;
        *prod = 1;
        return "\r";
    } else if (*esc == '"') {
        *cons = 1 + el;
        *prod = 1;
        return "\"";
    } else if (*esc == 'x') {
        static char hbuf[4];
        char* end = 0;
        uint32_t val = strtoul(esc + 1, &end, 16);
        *cons = end - esc + el;

        for (size_t i = 0; i < 4; i++) {
            hbuf[i] = val & (0xffu << (8 * i));
            if (hbuf[i])
                *prod += 1;
        }
        return hbuf;
    }
    fprintf(stderr, "Unrecognised escape sequence at %s\n", esc - el);
    return NULL;
}

/* Limited by buffer size at 0x3002140 */
#define SJIS_LEN_UNTIL_NEWLINE_MAX 512

char* mk_strtab_str(const char* u8str, iconv_t conv) {
#ifdef HAS_ICONV
    assert(conv != (iconv_t)-1);
    assert(!strncmp(u8"¥", (char[]){0xc2, 0xa5}, 2));

    iconv(conv, NULL, NULL, NULL, NULL);

    size_t u8len = strlen(u8str);
    size_t sjislen = u8len;
    char* sjis = malloc(sjislen + 1);
    if (!sjis) {
        perror("malloc");
        return NULL;
    }

    if (u8len == 0) {
        sjis[0] = '\0';
        return sjis;
    }

    const char* u8iter = u8str;
    char* sjisiter = sjis;
    size_t until_newline = 0;

    while (true) {
        size_t esc = next_esc(u8iter);
        u8len = esc;

        const char* sjisiter_old = sjisiter;
        /* Try to convert starting at u8iter until esc */
        size_t cstatus = iconv(conv, (void*)&u8iter, &u8len, &sjisiter, &sjislen);
        if (cstatus == (size_t)-1)
            goto fail_iconv;

        until_newline = sjisiter - sjisiter_old;

        /* We would have converted either until escape char or end of string */
        if (*u8iter) {
            size_t cons, prod;
            const char* escb = buf_for_esc(u8iter, &cons, &prod);
            if (!escb)
                goto fail;
            if (*escb == '\n' && until_newline >= SJIS_LEN_UNTIL_NEWLINE_MAX)
                goto fail;

            u8iter += cons;

            cstatus = iconv(conv, (void*)&escb, &prod, &sjisiter, &sjislen);
            if (cstatus == (size_t)-1)
                goto fail_iconv;
        } else /* Reached end of string */
            break;
    }

    *sjisiter = '\0';

    return sjis;

fail_iconv:
    perror("iconv");
fail:
    free(sjis);
    return NULL;
#else
    return NULL;
#endif
}
