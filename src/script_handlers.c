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

#include "script.h"
#include "strtab.h"

struct script_cmd_handler script_handlers[SCRIPT_OP_MAX + 1];

static uint32_t next_cmd_arg(uint16_t a1, uint16_t w, const struct script_state* state) {
    if (a1 & (0x8000 >> (w - 1))) {
        assert(false);
        // return state->arg_tab[state->arg_tab_idx - next_cmd_arg(0, w, state)];
    }

    return *(uint32_t*)(&((uint8_t*)state->args)[2 * w - 2]);
}

static uint16_t handler_stub(uint16_t arg0, uint16_t arg1, struct script_state* state) {
    return 0;
}

static uint16_t handler_Jump(uint16_t arg0, uint16_t arg1, struct script_state* state) {
    if (arg1 != 1)
        return UINT16_MAX;

    assert(arg0 == 0);

    uint16_t dst = next_cmd_arg(arg0, 1, state);
    assert(dst % 2 == 0 && "Misaligned jump destination");

    if (!state->dumping)
        make_label(dst, state);
    else {
        assert(has_label(dst, state) && "Out-of-sync labels between phases");

        assert(state->va_ctx.buf && state->va_ctx.sz > sizeof("L_0xffff"));
        sprintf(state->va_ctx.buf, "L_0x%x", dst);
    }

    return 0;
}

static bool strtab_print_str(char* buf, size_t sz, const uint8_t* strtab, uint16_t line_idx,
        size_t* nprinted) {
    assert(buf && sz > sizeof("(65535)\""));
    *nprinted = sprintf(buf,  "(%u)\"", line_idx);
    size_t dec_len = 0;

    uint16_t ret = strtab_dec_str(strtab, line_idx, &buf[*nprinted], sz - *nprinted, &dec_len);
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
        v3 = next_cmd_arg(arg0, 2, state);
    uint16_t line_idx = next_cmd_arg(arg0, 1, state);

    if (state->dumping) {
        size_t nprinted;
        state->has_err = !strtab_print_str(state->va_ctx.buf, state->va_ctx.sz, state->strtab, line_idx,
            &nprinted);
    }

    return 0;
}

static uint16_t handler_LoadBackground(uint16_t arg0, uint16_t has_bg, struct script_state* state) {
    uint16_t bg_idx = 0;

    if (state->dumping) {
        if (has_bg) {
            bg_idx = next_cmd_arg(arg0, 1, state);
            snprintf(state->va_ctx.buf, state->va_ctx.sz, "0x%x", bg_idx);
        } else
            state->va_ctx.buf[0] = '\0';
    }

    return bg_idx;
}

static uint16_t handler_LoadEffect(uint16_t arg0, uint16_t arg1, struct script_state* state) {
    assert(arg0 == 0);

    uint16_t idx0 = next_cmd_arg(arg0, 1, state);
    uint16_t idx1 = next_cmd_arg(arg0, 2, state);

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
        uint32_t line_idx =  next_cmd_arg(arg0, mask >> 16, state);
        mask += UINT16_MAX + 1;

        ok &= strtab_print_str(va_buf, va_buf_sz, state->strtab_menu, line_idx, &nprinted);

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

static uint16_t handler_ChoiceIdx(uint16_t arg0, uint16_t arg1, struct script_state* state) {
    uint32_t dst = next_cmd_arg(arg0, 1, state) & UINT16_MAX;
    assert(arg1 > 0);
    uint32_t mask = UINT16_MAX * 2 + 1 + UINT16_MAX + 1;

    if (state->dumping)
        state->has_err = !print_choice(1, mask, arg0, arg1, true, dst, state);

    return 0;
}

static uint16_t handler_Stop(uint16_t arg0, uint16_t arg1, struct script_state* state) {
    return UINT16_MAX;
}

uint32_t branch_dst(char* arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3,
    struct script_state* state, uint16_t* dst);
uint32_t is_branch_taken(const char* info, const struct script_state* state);
uint16_t sub_8002704(struct script_state* state, uint16_t* dst);

static uint16_t handler_0x4(uint16_t arg0, uint16_t arg1, struct script_state* state) {
    assert(arg0 == 0);

    if (state->dumping) {
        assert(state->va_ctx.buf && state->va_ctx.sz > sizeof("0xffff"));
        sprintf(state->va_ctx.buf, "0x%x", next_cmd_arg(arg0, 1, state) & UINT16_MAX);
    }

    /* FIXME: Decoding if branch is taken */
    /* FIXME: Decoding multiple branch destinations for ChoiceIdx? */
    if (state->cmd_offs == 0x1706) {
        uint16_t dst = state->cmd_offs_next;
        uint16_t val = branch_dst((char[]){0x05, 0x00, 0x06, 0x00, 0x07, 0x00}, 3, 0, 4, state, &dst);
        if (val == 7)
            sub_8002704(state, &dst);

        if (!state->dumping)
            make_label(dst, state);

        if (state->dumping)
            ;// FIXME
    }

    return 0;
}

static uint16_t handler_0x0(uint16_t a1, uint16_t a2, struct script_state* state) {
    if (/* a1 << 16 || */ a2)
        return UINT16_MAX;
    return 0;
}

static uint16_t handler_ShowMovie(uint16_t a1, uint16_t a2, struct script_state* state) {
    assert(a2 == 1);

    if (state->dumping) {
        assert(state->va_ctx.buf && state->va_ctx.sz > sizeof("0xffff"));
        sprintf(state->va_ctx.buf, "0x%x", next_cmd_arg(a1, 1, state) & UINT16_MAX);
    }
    return 0;
}

/* It's a no-op, but we prohibit disassembly of this instruction */
static uint16_t handler_0x30(uint16_t a1, uint16_t a2, struct script_state* state) {
    return UINT16_MAX;
}

void init_script_handlers() {
    for (size_t i = 0; i < SCRIPT_OP_MAX; i++) {
        script_handlers[i] = (struct script_cmd_handler){.name = NULL, .handler = handler_stub,
            .has_va = false, .nargs = 2};
    }

    script_handlers[0].handler = handler_0x0;
    script_handlers[7].handler = handler_0x0;

    script_handlers[1].name = "Jump";
    script_handlers[1].handler = handler_Jump;
    script_handlers[1].has_va = true;
    script_handlers[1].nargs = 0;

    script_handlers[4].handler = handler_0x4;
    script_handlers[4].has_va = true;
    script_handlers[4].nargs = 0;

    script_handlers[0xc].name = "ShowText";
    script_handlers[0xc].handler = handler_ShowText;
    script_handlers[0xc].has_va = true;
    script_handlers[0xc].nargs = 0;

    script_handlers[0xd].name = "ShowMovie";
    script_handlers[0xd].handler = handler_ShowMovie;
    script_handlers[0xd].has_va = true;
    script_handlers[0xd].nargs = 0;

    script_handlers[0x10].name = "HandleInput";
    script_handlers[0x10].nargs = 0;

    script_handlers[0x69].name = "LoadBackground";
    script_handlers[0x69].has_va = true;
    script_handlers[0x69].handler = handler_LoadBackground;

    script_handlers[0x6d].name = "LoadEffect";
    script_handlers[0x6d].has_va = true;
    script_handlers[0x6d].handler = handler_LoadEffect;

    script_handlers[0x11].name = "Choice";
    script_handlers[0x11].has_va = true;
    script_handlers[0x11].handler = handler_Choice;

    script_handlers[0x30].handler = handler_0x30;

    script_handlers[0x35].name = "ChoiceIdx";
    script_handlers[0x35].has_va = true;
    script_handlers[0x35].handler = handler_ChoiceIdx;

    script_handlers[0x5f].name = "PlayCredits";
    script_handlers[0x5f].nargs = 0;

    script_handlers[0x60].name = "GiveCard";

    script_handlers[0x63].name = "Stop";
    script_handlers[0x63].nargs = 0;
    // script_handlers[0x63].handler = handler_Stop;
}

bool cmd_is_branch(const union script_cmd* cmd) {
    assert(cmd->op != 0x2 && cmd->op != 0x8);
    return cmd->op == 1 || cmd->op == 4;
}
