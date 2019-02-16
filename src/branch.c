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

#include "script_disass.h"

static void skip_cmd(const struct script_state* state, uint16_t* dst) {
    *dst += 2 + 2 + sizeof(uint16_t) * ((union script_cmd*)((uint8_t*)state->cmds + *dst))->arg;
}

void branch_dst(const struct script_state* state, uint16_t* dst) {
    while (1) {
        union script_cmd* cmd = (void*)((uint8_t*)state->cmds + *dst);

        /* This will place a label at branch_info, which will not be disassembled */
        if ((char*)cmd >= state->branch_info && !state->dumping) {
            fprintf(stderr, "Cannot find branch destination for op at 0x%x\n", state->cmd_offs);
            break;
        }

        /* Seek until next branch (op 5 or 6) or nop (7) */
        if (!cmd_can_be_branched_to(cmd))
            skip_cmd(state, dst);
        else
            break;
    }
}
