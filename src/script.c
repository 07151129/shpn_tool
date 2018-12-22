#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "script.h"
#include "strtab.h"

static struct script_desc scripts[] = {
    {
        .name = "Harry",
        .vma = 0x82316DC,
        .strtab_vma = 0x853E908,
        .sz = 0xd3e4,
        .cksum = 0xba64
    }
};

extern struct script_cmd_handler script_handlers[SCRIPT_OP_MAX];

static uint16_t dump_cmd(const union script_cmd* cmd, struct script_state* state, FILE* fout) {
    if (cmd->op > SCRIPT_OP_MAX) {
        fprintf(stderr, "Out-of-bounds op 0x%x\n", cmd->op);
        state->has_err = 1;
        goto err;
    }

    const struct script_cmd_handler* handler = &script_handlers[cmd->op];

    uint16_t ret = handler->handler(cmd->arg >> 16, cmd->arg, state);

err:
    if (state->has_err) {
        fprintf(stderr, "Error decoding op 0x%x at cmd_offs=0x%x, stopping\n", cmd->op,
            state->cmd_offs);
        ret = UINT16_MAX;

        return ret;
    }

    if (handler->name)
        fprintf(fout, "%s(", handler->name);
    else
        fprintf(fout, "OP_0x%x(", cmd->op);

    if (handler->nargs == 2)
        fprintf(fout, "0x%x, 0x%x", cmd->arg >> 16, cmd->arg);
    else if (handler->nargs == 1)
        fprintf(fout, "0x%x", cmd->arg >> 16);

    if (handler->has_va)
        fprintf(fout, "%s%s", handler->nargs > 0 ? ", " : "", state->va_ctx.buf);
    fprintf(fout, "); // 0x%x: %08x\n", state->cmd_offs, cmd->ival);

    return ret;
}

#define SCRIPT_CKSUM_SEED 0x5678

static uint16_t cksum(const uint8_t* buf, size_t sz, uint32_t seed) {
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
    const union script_cmd* cmds) {
    assert(state);

    static uint8_t arg_tab_default[0x56 * sizeof(uint32_t)]; /* FIXME: Size, r/w access? */
    static char va_buf_default[VA_BUF_DEFAULT_SZ];

    memset(state, 0, sizeof(*state));
    state->arg_tab = arg_tab_default;

    state->va_ctx.buf = va_buf_default;
    state->va_ctx.sz = sizeof(va_buf_default);

    state->strtab = strtab;
    state->strtab_menu = strtab_menu;

    state->cmds = cmds;
}

#define SCRIPT_DUMP_NCMDS_MAX 3059u

bool script_dump(const uint8_t* rom, size_t rom_sz, const struct script_desc* desc, FILE* fout) {
    uint16_t cks = cksum((uint8_t*)&rom[VMA2OFFS(desc->vma)], desc->sz, SCRIPT_CKSUM_SEED);
    if (cks != desc->cksum)
        fprintf(stderr, "Ignoring script checksum mismatch (computed 0x%x != 0x%x)\n",
            cks, desc->cksum);

    /* FIXME: Script header */
    size_t ndec = 0;
    const union script_cmd* cmds = (union script_cmd*)&rom[VMA2OFFS(desc->vma) + 6];

    struct script_state state;
    script_state_init(&state, &rom[VMA2OFFS(desc->strtab_vma)], &rom[VMA2OFFS(STRTAB_MENU_VMA)],
        cmds);

    while (true) {
        const union script_cmd* cmd = (union script_cmd*)&((uint8_t*)cmds)[state.cmd_offs_next];

        assert((uint8_t*)cmd < rom + rom_sz);

        if (ndec + 1 > SCRIPT_DUMP_NCMDS_MAX) {
            fprintf(stderr, "Dump length exceeds %u, stopping...\n", SCRIPT_DUMP_NCMDS_MAX);
            break;
        }

        state.cmd_offs = state.cmd_offs_next;

        /**
         * Updated before dispatching. Because some commands might change it arbitrarily, we keep
         * a copy.
         */
        state.cmd_offs_next += sizeof(union script_cmd) + 2 * cmd->arg;
        state.args = cmd + 1;

        if (dump_cmd(cmd, &state, fout) == UINT16_MAX)
            break;

        ndec++;
    }

    return true;
}
