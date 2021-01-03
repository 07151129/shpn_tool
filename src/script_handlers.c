#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "script_disass.h"
#include "strtab.h"

struct script_cmd_handler script_handlers[SCRIPT_NOPS];

/* FIXME: Improve error handling: signal error instead of aborting at asserts */

uint32_t script_next_cmd_arg(uint16_t a1, uint16_t w, const struct script_state* state) {
    if (a1 & (0x8000 >> (w - 1))) {
        assert(false);
        // return state->arg_tab[state->arg_tab_idx - next_cmd_arg(0, w, state)];
    }

    return *(uint32_t*)(&((uint8_t*)state->args)[2 * w - 2]);
}

static uint16_t handler_stub(UNUSED uint16_t arg0, UNUSED uint16_t arg1,
    UNUSED struct script_state* state) {
    return 0;
}

static uint16_t handler_Jump(uint16_t arg0, uint16_t arg1, struct script_state* state) {
    if (arg1 != 1)
        return UINT16_MAX;

    assert(arg0 == 0);

    uint16_t dst = script_next_cmd_arg(arg0, 1, state);
    // assert(dst % 2 == 0 && "Misaligned jump destination");

    if (!state->dumping)
        make_label(dst, state);
    else {
        assert(has_label(dst, state) && "Out-of-sync labels between phases");

        assert(state->va_ctx.buf && state->va_ctx.sz > sizeof("L_0xffff"));
        sprintf(state->va_ctx.buf, "L_0x%x", dst);
    }

    return 0;
}

static bool strtab_print_str(char* buf, size_t sz, const uint8_t* strtab, const uint8_t* rom_end,
        uint16_t line_idx, size_t* nprinted, iconv_t conv) {
    assert(buf && sz > sizeof("(65535)\""));
    *nprinted = sprintf(buf,  "(%u)\"", line_idx);
    size_t dec_len = 0;

    uint16_t ret = strtab_dec_str(strtab, rom_end, line_idx, &buf[*nprinted], sz - *nprinted,
        &dec_len, conv, true);
    *nprinted += dec_len;

    if (!ret)
        fprintf(stderr, "Failed to decode line %d\n", line_idx);

    assert(*nprinted + sizeof("\"") <= sz);
    *nprinted += snprintf(&buf[*nprinted - 1], sz - *nprinted, "\"");

    return ret;
}

static uint16_t handler_ShowText(uint16_t arg0, uint16_t arg1, struct script_state* state) {
    assert(arg0 == 0 && arg1 == 1);

    uint32_t v3 = 0; // v3 != 0 => new frame?
    if (arg1 == 2)
        v3 = script_next_cmd_arg(arg0, 2, state);
    uint16_t line_idx = script_next_cmd_arg(arg0, 1, state);

    if (state->dumping) {
        size_t nprinted;
        state->has_err = !strtab_print_str(state->va_ctx.buf, state->va_ctx.sz, state->strtab,
            state->rom_end, line_idx, &nprinted, state->conv);
    }

    return 0;
}

static uint16_t handler_LoadBackground(uint16_t arg0, uint16_t has_bg, struct script_state* state) {
    uint16_t bg_idx = 0;

    if (state->dumping) {
        if (has_bg) {
            bg_idx = script_next_cmd_arg(arg0, 1, state);
            snprintf(state->va_ctx.buf, state->va_ctx.sz, "0x%x", bg_idx);
        } else
            state->va_ctx.buf[0] = '\0';
    }

    return bg_idx;
}

static uint16_t handler_LoadEffect(uint16_t arg0, UNUSED uint16_t arg1,
    struct script_state* state) {
    assert(arg0 == 0);

    uint16_t idx0 = script_next_cmd_arg(arg0, 1, state);
    uint16_t idx1 = script_next_cmd_arg(arg0, 2, state);

    if (state->dumping) {
        snprintf(state->va_ctx.buf, state->va_ctx.sz, "0x%x, 0x%x", idx0, idx1);
    }

    return 0;
}

static bool print_choice(size_t start, uint32_t mask, uint16_t arg0, size_t nargs, bool print_dst,
        uint32_t dst, struct script_state* state) {
    char* va_buf = state->va_ctx.buf;
    size_t va_buf_sz = state->va_ctx.sz;
    size_t nprinted = 0;

    bool ok = true;

    if (print_dst) {
        nprinted = sprintf(va_buf, "0x%x, ", dst);

        va_buf += nprinted;
        va_buf_sz -= nprinted;
    }

    for (size_t i = start; i < nargs; i++) {
        uint32_t line_idx = script_next_cmd_arg(arg0, mask >> 16, state);
        mask += UINT16_MAX + 1;

        ok &= strtab_print_str(va_buf, va_buf_sz, state->strtab_menu, state->rom_end,
            line_idx, &nprinted, state->conv);

        if (!ok)
            return false;

        va_buf += nprinted - 1;
        va_buf_sz -= nprinted + 1;

        if (i == nargs - 1)
            break;

        if (va_buf_sz >= sizeof(", ")) {
            snprintf(va_buf, va_buf_sz, ", ");
            va_buf += sizeof(", ") - 1;
            va_buf_sz -= sizeof(", ") + 1;
        } else
            return false;
    }

    return true;
}

static uint16_t handler_Choice(uint16_t arg0, uint16_t arg1, struct script_state* state) {
    assert(arg1 <= 10);
    uint32_t mask = UINT16_MAX * 2 + 1;

    if (state->dumping)
        state->has_err = !print_choice(0, mask, arg0, arg1, false, 0, state);

    return 0;
}

static uint16_t handler_ChoiceIdx(uint16_t arg0, uint16_t nargs, struct script_state* state) {
    uint32_t dst = script_next_cmd_arg(arg0, 1, state) & UINT16_MAX;
    assert(nargs > 1);
    uint32_t mask = UINT16_MAX * 2 + 1 + UINT16_MAX + 1;

    if (state->dumping)
        state->has_err = !print_choice(1, mask, arg0, nargs, true, dst, state);

    return 0;
}

void branch_dst(struct script_state* state, uint16_t* dst);
uint32_t is_branch_taken(const char* info, const struct script_state* state);

static uint16_t handler_Branch(uint16_t arg0, UNUSED uint16_t arg1, struct script_state* state) {
    assert(arg0 == 0);

    uint16_t idx = script_next_cmd_arg(arg0, 1, state);

    size_t nprinted = 0;

    if (state->dumping) {
        assert(state->va_ctx.buf && state->va_ctx.sz > sizeof("0xffff"));
        nprinted += sprintf(state->va_ctx.buf, "0x%x", idx);
    }

    uint16_t dst = state->cmd_offs_next;
    branch_dst(state, &dst);

    if (!state->dumping)
        make_label(dst, state);
    else {
        assert(state->va_ctx.sz - nprinted > sizeof("L_0xffff"));
        sprintf(&state->va_ctx.buf[nprinted], ", L_0x%x", dst);
    }

    return 0;
}

static uint16_t handler_Nop(UNUSED uint16_t a1, uint16_t a2, UNUSED struct script_state* state) {
    if (/* a1 << 16 || */ a2)
        return UINT16_MAX;
    return 0;
}

static uint16_t handler_ShowMovie(uint16_t a1, UNUSED uint16_t a2, struct script_state* state) {
    assert(a2 == 1);

    if (state->dumping) {
        assert(state->va_ctx.buf && state->va_ctx.sz > sizeof("0xffff"));
        sprintf(state->va_ctx.buf, "0x%x", script_next_cmd_arg(a1, 1, state) & UINT16_MAX);
    }
    return 0;
}

/* It's a no-op, but we prohibit disassembly of this instruction */
static uint16_t handler_0x30(UNUSED uint16_t a1, UNUSED uint16_t a2, UNUSED struct
    script_state* state) {
    return UINT16_MAX;
}

void init_script_handlers() {
    static bool did_init;
    if (did_init)
        return;

    for (size_t i = 0; i < SCRIPT_NOPS; i++) {
        script_handlers[i] = (struct script_cmd_handler){.name = NULL, .handler = handler_stub,
            .has_va = false/*, .nargs = 2*/};
    }

    script_handlers[0].handler = handler_Nop;
    script_handlers[0].name = "Nop0";

    script_handlers[7].handler = handler_Nop;
    script_handlers[7].name = "Nop7";

    script_handlers[1].name = "Jump";
    script_handlers[1].handler = handler_Jump;
    script_handlers[1].has_va = true;

    for (int i = 4; i < 7; i++) {
        script_handlers[i].handler = handler_Branch;
        script_handlers[i].has_va = true;
    }
    script_handlers[4].name = "Branch4";
    script_handlers[5].name = "Branch5";
    script_handlers[6].name = "Branch6";

    script_handlers[0xc].name = "ShowText";
    script_handlers[0xc].handler = handler_ShowText;
    script_handlers[0xc].has_va = true;

    script_handlers[0xd].name = "ShowMovie";
    script_handlers[0xd].handler = handler_ShowMovie;
    // script_handlers[0xd].has_va = true;

    script_handlers[0x10].name = "HandleInput";

    script_handlers[0x69].name = "LoadBackground";
    // script_handlers[0x69].has_va = true;
    script_handlers[0x69].handler = handler_LoadBackground;

    script_handlers[0x6d].name = "LoadEffect";
    // script_handlers[0x6d].has_va = true;
    script_handlers[0x6d].handler = handler_LoadEffect;

    script_handlers[0x11].name = "Choice";
    script_handlers[0x11].has_va = true;
    script_handlers[0x11].handler = handler_Choice;

    script_handlers[0x30].handler = handler_0x30;

    script_handlers[0x35].name = "ChoiceIdx";
    script_handlers[0x35].has_va = true;
    script_handlers[0x35].handler = handler_ChoiceIdx;

    script_handlers[0x5f].name = "PlayCredits";

    script_handlers[0x60].name = "GiveCard";

    script_handlers[0x61].name = "Puzzle";

    script_handlers[0x63].name = "Stop";
    // script_handlers[0x63].handler = handler_Stop;

    did_init = true;
}

bool cmd_is_jump(const union script_cmd* cmd) {
    return cmd->op == 1;
}

bool cmd_is_branch(const union script_cmd* cmd) {
    return (cmd->op >= 4 && cmd->op <= 6);
}

bool cmd_can_be_branched_to(const union script_cmd* cmd) {
    return 5 <= cmd->op && cmd->op <= 7;
}

bool cmd_uses_menu_strtab(const union script_cmd* cmd) {
    return cmd->op == 0x11 || cmd->op == 0x35;
}

bool cmd_uses_script_strtab(const union script_cmd* cmd) {
    return cmd->op == 0xc;
}

bool cmd_is_choice_idx(const union script_cmd* cmd) {
    return cmd->op == 0x35;
}
