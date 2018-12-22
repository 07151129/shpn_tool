#ifndef SCRIPT_H
#define SCRIPT_H

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define SCRIPT_OP_MAX 118
#define STRTAB_MENU_VMA 0x857546C

union script_cmd {
    struct {
        unsigned op : 12;
        unsigned arg : 20;
    };
    uint32_t ival;
};
static_assert(sizeof(union script_cmd) == sizeof(uint32_t), "");

struct script_desc {
    const char* name;
    uint32_t vma;
    uint32_t strtab_vma;
    size_t sz;
    const uint16_t cksum;
};

struct script_state {
    /* byte offset into cmd buffer */
    uint16_t cmd_offs;
    uint16_t cmd_offs_next;

    const union script_cmd* cmds;

    uint16_t op;

    /* Used in next_cmd_arg */
    const union script_cmd* args; /* 0x3001E58 */
    uint16_t arg_tab_unk0;

    uint16_t arg_tab_idx;
    uint8_t* arg_tab;

    const uint8_t* strtab, * strtab_menu;

    struct {
        char* buf; /* any extra arguments to be logged by a handler */
        size_t sz;
    } va_ctx;

    bool has_err;
};

struct script_cmd_handler {
    const char* name;
    int nargs;
    bool has_va;

    /**
     * arg0 -- Never encountered arg0=1 so far, uses some state
     * arg1 -- If !arg0 it's just a read of uint16[arg1] past the instruction
     *
     * As the actually useful arguments seem to be encoded in state.args, we should eventually
     * omit these in disassembly.
     */
    uint16_t (*handler)(uint16_t, uint16_t, struct script_state*);
};

void init_script_handlers();
bool script_dump(const uint8_t* rom, size_t rom_sz, const struct script_desc* desc, FILE* fout);
const struct script_desc* script_for_name(const char* name);

#endif
