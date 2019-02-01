#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "script_disass.h"
#include "strtab.h"

static struct script_desc scripts[] = {
    {
        .name = "Harry",
        .vma = 0x82316DC,
        .strtab_vma = 0x853E908,
        .cksum = 0xba64,
        .patch_info = {.size_vma = 0x80126A0}
    },
    {
        .name = "Cybil",
        .vma = 0x823EAC0,
        .strtab_vma = 0x853E908,
        .cksum = 0xb971,
        .patch_info = {.size_vma = 0x80126AC}
    }
};

extern struct script_cmd_handler script_handlers[SCRIPT_NOPS];

static bool is_valid_cmd(const union script_cmd* cmd) {
    return cmd->op < SCRIPT_NOPS;
}

static uint16_t dis_cmd(const union script_cmd* cmd, struct script_state* state, FILE* fout,
    bool at_label) {
    assert(is_valid_cmd(cmd));

    const struct script_cmd_handler* handler = &script_handlers[cmd->op];

    uint16_t ret = handler->handler(cmd->arg >> 16, cmd->arg, state);

    if (state->has_err) {
        fprintf(stderr, "Error dumping op 0x%x at cmd_offs=0x%x, stopping\n", cmd->op,
            state->cmd_offs);
        ret = UINT16_MAX;

        return ret;
    }

    if (state->dumping && at_label)
        fprintf(fout, "L_0x%x:\n", state->cmd_offs);

    if (ret != UINT16_MAX && state->dumping) {
        if (handler->name)
            fprintf(fout, "%s(", handler->name);
        else
            fprintf(fout, "OP_0x%x(", cmd->op);

        // fprintf(fout, "0x%x, 0x%x", cmd->arg >> 16, cmd->arg);

        if (handler->has_va)
            fprintf(fout, "%s", state->va_ctx.buf);
        else {
            for (size_t i = 0; i < cmd->arg; i++) {
                uint16_t arg = script_next_cmd_arg(cmd->arg >> 16, i + 1, state);
                fprintf(fout, "%s0x%x", i > 0 ? ", " : "", arg);
            }
        }
        fprintf(fout, "); // 0x%x: %08x\n", state->cmd_offs, cmd->ival);
    }

    return ret;
}

bool has_label(uint16_t offs, const struct script_state* state);

static void dump_uint32(const union script_cmd* cmd, const struct script_state* state, FILE* fout) {
    /* Allow dumping valid code as bytes as well as interpretation doesn't matter at that point */
    // assert(!is_valid_cmd(cmd) && "Trying to dump valid cmd as uint32");

    /* Some dead code might perform jumps that make no sense, so ignore this for now */
    // assert(!has_label(state->cmd_offs, state) && "Branch to unrecognised cmd");
    fprintf(fout, ".4byte 0x%x // 0x%x\n", cmd->ival, state->cmd_offs);
}

static void dump_uint8(const uint8_t* b, const struct script_state* state, FILE* fout) {
    fprintf(fout, ".byte 0x%x // 0x%x\n", *b, state->cmd_offs);
}

#define SCRIPT_CKSUM_SEED 0x5678

uint16_t script_cksum(const uint8_t* buf, size_t sz, uint32_t seed) {
    uint64_t ret = (uint64_t)(seed & UINT16_MAX) << 24;
    ret = (ret & ~(uint64_t)UINT32_MAX) | ((ret & (uint64_t)UINT32_MAX) >> 24);

    while (sz) {
        uint32_t b = *buf;
        ret = ((ret & ~(uint64_t)UINT32_MAX) ^ ((uint64_t)b << 32)) | (ret & UINT32_MAX);
        ret = (ret & ~(uint64_t)UINT32_MAX) | ((ret + b) & 0xff);
        buf++;
        sz--;
    }
    return ((ret >> 32) << 8) + ret;
}

const struct script_desc* script_for_name(const char* name) {
    for (size_t i = 0; i < sizeof(scripts) / sizeof(*scripts); i++)
        if (!strncmp(name, scripts[i].name, strlen(scripts[i].name)))
            return &scripts[i];
    return NULL;
}

#define VA_BUF_DEFAULT_SZ (SJIS_TO_U8_MIN_SZ(DEC_BUF_SZ_SJIS) * 4)

void script_state_init(struct script_state* state, const uint8_t* strtab, const uint8_t* strtab_menu,
    const union script_cmd* cmds, uint16_t* labels, const char* branch_info,
    const uint8_t* branch_info_unk) {
    assert(state);

    /* Those are the "default" non thread-safe buffers (OK for now) */
    // static uint8_t arg_tab_default[0x56 * sizeof(uint32_t)]; /* FIXME: Size, r/w access? */
    static char va_buf_default[VA_BUF_DEFAULT_SZ];
    static uint8_t choices[SCRIPT_CHOICES_SZ];

    memset(state, 0, sizeof(*state));
    // state->arg_tab = arg_tab_default;

    state->va_ctx.buf = va_buf_default;
    state->va_ctx.sz = sizeof(va_buf_default);

    state->strtab = strtab;
    state->strtab_menu = strtab_menu;

    state->cmds = cmds;

    state->label_ctx.labels = labels;

    state->branch_info = branch_info;
    state->branch_info_unk = branch_info_unk;

    assert(state->branch_info > (char*)state->cmds &&
        "branch_info is expected to terminate cmds");

    state->choice_ctx.choices = choices;

    state->conv = (iconv_t)-1;

#ifdef HAS_ICONV
    state->conv = iconv_open("UTF-8", "SJIS");
#endif

    if (state->conv == (iconv_t)-1) {
#ifdef HAS_ICONV
        perror("iconv_open");
#endif
        fprintf(stderr, "iconv_open failed; will dump raw values\n");
    }
}

static void script_state_free(struct script_state* state) {
#ifdef HAS_ICONV
    if (state->conv != (iconv_t)-1) {
        iconv_close(state->conv);
    }
#endif
}

#define SCRIPT_DUMP_NCMDS_MAX 15000u

#define NLABELS_MAX SCRIPT_DUMP_NCMDS_MAX /* worst case */
static uint16_t labels[NLABELS_MAX]; /* offsets into cmd buffer */

bool make_label(uint16_t offs, struct script_state* state) {
    /**
     * Because we disassemble instructions at different offsets in the second phase, we might
     * create new labels then, but they are meaningless (and will break the command order in the
     * dump), so this should be prohibited.
     */
    assert(!state->dumping);

    size_t i = 0;

    for (; i < state->label_ctx.nlabels; i++) {
        /* Label already exists */
        if (labels[i] == offs)
            return false;
    }

    assert(i < NLABELS_MAX && "Out of label space");

    state->label_ctx.labels[i] = offs;
    state->label_ctx.nlabels++;

    return true;
}

static bool has_labels(const struct script_state* state) {
    return state->label_ctx.curr_label < state->label_ctx.nlabels;
}

static uint16_t curr_label(struct script_state* state) {
    assert(has_labels(state));

    return state->label_ctx.labels[state->label_ctx.curr_label];
}

static int label_cmp(const void* lhs, const void* rhs) {
    const uint16_t* ulhs = lhs, * urhs = rhs;
    if (*ulhs < *urhs)
        return -1;
    if (*ulhs > *urhs)
        return 1;
    return 0;
}

/* FIXME: Should hash labels to get rid of O(n^2) running time.. */
bool has_label(uint16_t offs, const struct script_state* state) {
    for (size_t i = 0; i < state->label_ctx.nlabels; i++)
        if (state->label_ctx.labels[i] == offs)
            return true;
    return false;
}

/**
 * The disassembler works in two phases:
 * During the first phase it adds a label for location zero, and then starts
 * reading the instructions at that location. For the first branch instruction that it encounters,
 * it creates a label for its destination if there is none yet, and attempts to read as many
 * instructions as possible
 * after the branch instruction. As soon as an invalid instruction is read, or a label is
 * encountered, the process is repeated for the next unexplored label from the label list, and the
 * label that has just been processed is marked as explored. The process repeats until there is no
 * unexplored label in the list.
 *
 * During the second phase, the list of labels is sorted, resulting in a linear map of dumpable
 * code regions, each either terminated by an invalid instruction, branch instruction, or another
 * label. Then the textual representation of instructions is dumped starting at each label address
 * from the list. If an invalid instruction is encountered, then the data starting at that address
 * is dumped as is until the next label is encountered, so there need not be set-location directive.
 *
 * Because there are finitely many branch/jump instructions in a script, finitely many labels will be
 * created, so the procedure will terminate.
 */
bool script_dump(const uint8_t* rom, size_t rom_sz, const struct script_desc* desc, FILE* fout) {
    const struct script_hdr* hdr = (void*)&rom[VMA2OFFS(desc->vma)];
    static_assert(sizeof(*hdr) == sizeof(uint16_t[3]), "");

    uint16_t cks = script_cksum((uint8_t*)hdr, script_sz(hdr) + sizeof(struct script_hdr),
        SCRIPT_CKSUM_SEED);
    if (cks != desc->cksum)
        fprintf(stderr, "Ignoring script checksum mismatch (computed 0x%x != 0x%x)\n",
            cks, desc->cksum);

    const union script_cmd* cmds = (void*)&((uint8_t*)hdr)[sizeof(*hdr)];

    /* We'll dump everything past cmd buffer as bytes */
    const void* cmd_end = &((uint8_t*)cmds)[hdr->branch_info_offs];

    if ((void*)cmds >= cmd_end) {
        fprintf(stderr, "Script is too short\n");
        return false;
    }

    if ((uint8_t*)cmd_end + hdr->branch_info_sz+ hdr->bytes_to_end >= rom_sz + rom) {
        fprintf(stderr, "Script ends past EOF (incorrect script header?)\n");
        return false;
    }

    struct script_state state;
    script_state_init(&state, &rom[VMA2OFFS(desc->strtab_vma)], &rom[VMA2OFFS(STRTAB_MENU_VMA)],
        cmds, labels, cmd_end, &rom[VMA2OFFS(BRANCH_INFO_UMK_VMA)]);

    /* Phase one: read code and create labels */
    make_label(0, &state); /* The initial label */

    size_t ninst = 0;
phase:
    while (has_labels(&state)) {
        state.cmd_offs_next = curr_label(&state);

        /* Should we check if there is a label at cmd_offs? Omit for first cmd at label */
        bool chk_label = false;

        /* Start disassembling instructions at this address */
        while (true) {
            if (ninst == SCRIPT_DUMP_NCMDS_MAX) {
                /* Stop adding labels at this point */
                if (!state.dumping)
                    break;
                fprintf(stderr, "Dump length exceeds %u, stopping...\n", SCRIPT_DUMP_NCMDS_MAX);
                return false;
            }

            state.cmd_offs = state.cmd_offs_next;
            const union script_cmd* cmd = (void*)&((uint8_t*)cmds)[state.cmd_offs];

            bool at_label = has_label(state.cmd_offs, &state);
            bool past_label = state.dumping &&
                state.label_ctx.curr_label + 1 < state.label_ctx.nlabels &&
                state.label_ctx.labels[state.label_ctx.curr_label + 1] <= state.cmd_offs;

            if (cmd >= (union script_cmd*)cmd_end || /* Reached end of command buffer */
                (chk_label && at_label) || /* Encountered a label at this address  */
                past_label) /* When dumping, we would have disassembled past next label */
                break; /* Stop and pick another unprocessed label */

            /* Skip the args buffer */
            state.cmd_offs_next += sizeof(*cmd) + 2 * cmd->arg;
            state.args = cmd + 1;

            uint16_t dis_ret = is_valid_cmd(cmd) ?
                dis_cmd(cmd, &state, fout, at_label) : UINT16_MAX;

            /* Cannot disassemble at this address */
            if (dis_ret == UINT16_MAX || state.cmd_offs_next < state.cmd_offs) {
                /* Just dump it as uint32, we don't really care what it does */
                if (state.dumping)
                    dump_uint32(cmd, &state, fout);
                /* In either phase, don't trust the encoded cmd_offs_next */
                state.cmd_offs_next = state.cmd_offs + sizeof(*cmd);
            }

            chk_label = true;
            ninst++;
        }
        state.label_ctx.curr_label++;
    }

    /* Second phase complete */
    if (state.dumping) {
        /* Dump branch info and remaining bytes */
        const uint8_t* end = (uint8_t*)cmd_end + hdr->branch_info_sz+ hdr->bytes_to_end;
        for (const uint8_t* cmd = (uint8_t*)cmds + state.cmd_offs; cmd < end; ) {
            if (end - cmd >= (ptrdiff_t)sizeof(union script_cmd)) {
                dump_uint32((union script_cmd*)cmd, &state, fout);
                cmd += sizeof(union script_cmd);
                state.cmd_offs += sizeof(union script_cmd);
            } else {
                dump_uint8(cmd, &state, fout);
                cmd++;
                state.cmd_offs++;
            }
        }

        script_state_free(&state);
        return true;
    }

    /* Second phase */
    state.dumping = true;
    state.label_ctx.curr_label = 0;
    qsort(state.label_ctx.labels, state.label_ctx.nlabels, sizeof(*state.label_ctx.labels),
        label_cmp);
    ninst = 0;
    /* Repeat phase one, but now dump commands starting at each label */
    goto phase;

    assert(false); /* Unreachable */
}

size_t script_sz(const struct script_hdr* hdr) {
    return hdr->branch_info_sz + hdr->branch_info_offs + hdr->bytes_to_end;
}
