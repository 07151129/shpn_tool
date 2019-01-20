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

static uint32_t sub_800281C(char a1, int a2) {
    uint32_t result; // r0

    assert(false);

    result = (uint32_t)(0x10000u << a1) >> 16;
    if (!(a2 << 16))
        result = ~result & UINT16_MAX;
    return result;
}

static uint32_t sub_80027DC(size_t a1, const struct script_state* state) {
    uint32_t v1; // r1

    assert(false && "choices_0 is never used");
#if 0
    size_t idx = (a1 << 16) >> 19;
    assert(idx < SCRIPT_CHOICES_SZ);

    fprintf(stderr, "0x%x: choices_0[0x%lx]\n", state->cmd_offs, idx);

    v1 = state->choices_0[idx] & sub_800281C(((a1 << 16) & 0x70000u) >> 16, 1);
    return (-v1 | v1) >> 31;
#endif
}

static uint32_t sub_800280C(uint32_t a1, const struct script_state* state) {
    size_t idx = 2 * a1 & 0x1FFFF;

    assert(idx < SCRIPT_CHOICES_SZ);

    fprintf(stderr, "0x%x: choices_1[0x%lx]\n", state->cmd_offs, idx);

    return *(uint16_t*)&(state->choice_ctx.choices[idx]);
}

static int sub_8006B54(const char* a1) {
    int v2; // r6
    int v3; // r4

    v2 = 0;
    while (1) {
        v3 = *a1++;
        if (!v3)
            break;
        if (isdigit(v3))
            v2 = 10 * v2 - 48 + v3;
    }
    return v2;
}

const char* sub_8006A48(const char* info, uint16_t* retb, const struct script_state* state) {
    int v4; // r2
    int v5; // r4
    int v6; // r10
    int v7; // r4
    int v8; // r5
    const char* v9; // r6
    uint16_t v10; // r0
    uint16_t v11; // r0
    uint16_t v12; // r0
    const char* result; // r0
    int v14; // r5
    uint16_t v15; // r4
    char v16; // r2
    int v17; // r1

    v4 = *info;
    if (v4 != 102 && v4 != 115) {
        if (isdigit(v4)) {
            v14 = 0;
            v15 = 0;
            do {
                if (!isdigit(*info))
                    break;
                v16 = *info;
                *((char*)retb + v15) = *info;
                v14 = ((10 * v14 & 0xFFFF) + (v16 & 0xF)) & 0xFFFF;
                v17 = (v15 + 1) << 16;
                ++info;
                v15 = (v15 + 1) & 0xFFFF;
            } while (v17 >> 16 <= 14);
            *((char*)retb + (uint16_t)v15) = 0;
            retb[9] = v14;
            result = info;
        } else {
            result = 0;
        }
    } else {
        *(char*)retb = v4;
        v5 = 1;
        v6 = (uint16_t)v4 << 16;
        while (1) {
            v7 = v5 << 16;
            v8 = v7 >> 16;
            v9 = &info[v7 >> 16];
            if (!isdigit(*v9))
                break;
            *((char*)retb + v8) = *v9;
            *((char*)retb + v8 + 1) = 0;
            v5 = (v8 + 1) & 0xFFFF;
        }
        if (v6 >> 16 == 102) {
            v10 = sub_8006B54((const char*)retb + 1);
            v11 = sub_80027DC(v10, state);
        } else {
            v12 = sub_8006B54((const char*)retb + 1);
            v11 = sub_800280C(v12, state);
        }
        retb[9] = v11;
        result = &info[v7 >> 16];
    }
    return result;
}

const char* sub_80069E4(const char* info, uint16_t* retb, const struct script_state* state) {
    int v4; // r0
    int v6; // r0
    const char* v7; // r4

    *retb = 0;
    retb[9] = 0;
    retb[8] = 0;
    if (isdigit(*info) || (((v4 = *info, v4 == 'f') || v4 == 's') && isdigit(info[1]))) {
        info = sub_8006A48(info, retb, state);
        if (!info)
            return 0;
    }
    while (*info == ' ')
        ++info;
    retb[8] = *info;
    v7 = info + 1;
    if (*info) {
        while (*v7 == ' ')
            ++v7;
    } else {
        --v7;
    }
    return v7;
}

static uint32_t sub_8006800(uint16_t a1) {
    int16_t v1; // r4
    signed int result; // r0
    unsigned int v3; // r0

    v1 = a1;
    if (isalpha(a1))
        return 145;
    v3 = (int16_t)(v1 - '!');
    if (v3 > 0x5B)
        return 247;
    switch (v3) {
        case 0u:
            result = '\x11';
            break;
        case 4u:
            result = 35;
            break;
        case 5u:
            result = 97;
            break;
        case 7u:
            result = 1;
            break;
        case 8u:
            result = 241;
            break;
        case 9u:
            result = 33;
            break;
        case 10u:
            result = 49;
            break;
        case 12u:
            result = 50;
            break;
        case 14u:
            result = 34;
            break;
        case 27u:
            result = 65;
            break;
        case 28u:
            result = 81;
            break;
        case 29u:
            result = 66;
            break;
        case 59u:
            result = 82;
            break;
        case 61u:
            result = 113;
            break;
        case 91u:
            result = 129;
            break;
        default:
            return 247;
    }
    return result;
}

int32_t sub_802C828(uint32_t a1, uint32_t a2) {
    int v2; // r12
    unsigned int v3; // r3
    int v4; // r2
    int result; // r0

    if (a2) {
        v2 = a1 ^ a2;
        v3 = 1;
        v4 = 0;
        if ((a2 & 0x80000000) != 0)
            a2 = -a2;
        if ((a1 & 0x80000000) != 0)
            a1 = -a1;
        if (a1 >= a2) {
            while (a2 < 0x10000000 && a2 < a1) {
                a2 *= 16;
                v3 *= 16;
            }
            while (a2 < 0x80000000 && a2 < a1) {
                a2 *= 2;
                v3 *= 2;
            }
            while (1) {
                if (a1 >= a2) {
                    a1 -= a2;
                    v4 |= v3;
                }
                if (a1 >= a2 >> 1) {
                    a1 -= a2 >> 1;
                    v4 |= v3 >> 1;
                }
                if (a1 >= a2 >> 2) {
                    a1 -= a2 >> 2;
                    v4 |= v3 >> 2;
                }
                if (a1 >= a2 >> 3) {
                    a1 -= a2 >> 3;
                    v4 |= v3 >> 3;
                }
                if (!a1)
                    break;
                v3 >>= 4;
                if (!v3)
                    break;
                a2 >>= 4;
            }
        }
        result = v4;
        if (v2 < 0)
            result = -v4;
    } else {
        result = 0;
    }
    return result;
}

static uint32_t __ROR4__(uint32_t v, uint32_t n) { return (v >> n | v << (8 * sizeof(v) - n)); }

static uint32_t sub_802C8C0(unsigned int result, unsigned int a2) {
    unsigned int v2; // r3
    int v3; // r2
    unsigned int v4; // r12
    unsigned int v5; // r2
    signed int v6; // [sp-8h] [bp-8h]

    v2 = 1;
    if (a2) {
        if ((a2 & 0x80000000) != 0)
            a2 = -a2;
        v6 = result;
        if ((result & 0x80000000) != 0)
            result = -result;
        if (result >= a2) {
            while (a2 < 0x10000000 && a2 < result) {
                a2 *= 16;
                v2 *= 16;
            }
            while (a2 < 0x80000000 && a2 < result) {
                a2 *= 2;
                v2 *= 2;
            }
            while (1) {
                v3 = 0;
                if (result >= a2)
                    result -= a2;
                if (result >= a2 >> 1) {
                    result -= a2 >> 1;
                    v3 = __ROR4__(v2, 1);
                }
                if (result >= a2 >> 2) {
                    result -= a2 >> 2;
                    v3 |= __ROR4__(v2, 2);
                }
                if (result >= a2 >> 3) {
                    result -= a2 >> 3;
                    v3 |= __ROR4__(v2, 3);
                }
                v4 = v2;
                if (!result)
                    break;
                v2 >>= 4;
                if (!v2)
                    break;
                a2 >>= 4;
            }
            v5 = v3 & 0xE0000000;
            if (v5) {
                if (v5 & __ROR4__(v4, 3))
                    result += a2 >> 3;
                if (v5 & __ROR4__(v4, 2))
                    result += a2 >> 2;
                if (v5 & __ROR4__(v4, 1))
                    result += a2 >> 1;
            }
        }
        if (v6 < 0)
            result = -result;
    } else {
        result = 0;
    }
    return result;
}

static int32_t sub_80064F8(uint16_t* a1, uint16_t* a2) {
    uint16_t* v2; // r4
    uint16_t* v3; // r2
    unsigned int v4; // r0
    uint16_t v5; // r1
    int16_t v6; // r0
    int16_t v7; // r3
    int v8; // r0
    int v10; // [sp+8h] [bp-4h]

    v2 = a1;
    v3 = a2;
    v4 = (int16_t)(a1[2] - 17);
    if (v4 <= 0x80) {
        switch (v4) {
            case 0u:
                v5 = 0;
                if (!*v3)
                    v5 = 1;
                goto LABEL_22;
            case 0x10u:
                v6 = *v2 * *a2;
                goto LABEL_30;
            case 0x11u:
                if (!*a2)
                    *a2 = 1;
                v6 = sub_802C828((int16_t)*v2, (int16_t)*a2);
                goto LABEL_30;
            case 0x12u:
                if (!*a2)
                    *a2 = 1;
                v6 = sub_802C8C0((int16_t)*v2, (int16_t)*a2);
                goto LABEL_30;
            case 0x20u:
                v6 = *v2 + *a2;
                goto LABEL_30;
            case 0x21u:
                v6 = *v2 - *a2;
                goto LABEL_30;
            case 0x30u:
                v7 = 0;
                if ((int16_t)*v2 < (int16_t)*a2)
                    goto LABEL_17;
                goto LABEL_18;
            case 0x31u:
                v7 = 0;
                if ((int16_t)*v2 > (int16_t)*a2)
                LABEL_17:
                    v7 = 1;
            LABEL_18:
                *v2 = v7;
                return v10;
            case 0x40u:
                v6 = 0;
                if (*v2 == *a2)
                    v6 = 1;
                goto LABEL_30;
            case 0x41u:
                v8 = (int16_t)*v2 ^ (int16_t)*a2;
                v5 = (-v8 | (unsigned int)v8) >> 31;
                goto LABEL_22;
            case 0x50u:
                v5 = 0;
                if (*v2)
                    v5 = (-(int16_t)*v3 | (unsigned int)(int16_t)*v3) >> 31;
                goto LABEL_22;
            case 0x60u:
                v6 = *v2 ^ *a2;
                goto LABEL_30;
            case 0x70u:
                v5 = 0;
                if (*v2 || *v3)
                    v5 = 1;
            LABEL_22:
                *v2 = v5;
                return v10;
            case 0x80u:
                return v10;
            default:
                break;
        }
    }
    v6 = *a2;
LABEL_30:
    *v2 = v6;
    return v10;
}

static const char* sub_80063B8(const char* info, uint16_t* retb, const struct script_state* state);

static const char* sub_80063FC(const char* info, uint16_t* retb, const struct script_state* state) {
    uint16_t v11[10];
    uint16_t v12[3];

    uint32_t v5;
    const char* v6;
    uint8_t v7, v8, v14, v10;

    do {
        info = sub_80069E4(info, v11, state);
        if (!info)
            return NULL;
        uint32_t v4 = sub_8006800(v11[8]) << 16;
        v5 = v4 >> 16;
        if (v4 >> 16 == 1) {
            if (v11[0] & UINT8_MAX)
                return 0;
            v6 = sub_80063B8(info, v12, state);
            if (v12[2] != 241)
                return 0;
            info = sub_80069E4(v6, v11, state);
            if (!info || v11[0] & UINT8_MAX)
                return 0;
            v5 = sub_8006800(v11[8]) & 0xFFFF;
        } else {
            *v12 = v11[9];
        }
        v12[2] = v5;
        v12[1] = v11[8];
        v7 = retb[2] & 0xF0;
        if ((uint8_t)(v5 & 0xF0) < v7) {
            v8 = retb[2] & 0xF0;
            do {
                v14 = v8;
                info = sub_80063FC(info, v12, state);
                v8 = v14;
                if (!info)
                    return 0;
            } while ((uint8_t)(v12[2] & 0xF0) < v14);
        }
        sub_80064F8(retb, v12);
        v10 = v12[2];
        retb[2] = v12[2];
        retb[1] = v12[1];
    } while (v7 == (v10 & 0xF0) && v7 != 240);
    return info;
}

static const char* sub_80063B8(const char* info, uint16_t* retb, const struct script_state* state) {
    retb[0] = 0;
    retb[2] = 0xf1;

    const char* ret = sub_80063FC(info, retb, state);
    if (!ret)
        return 0;

    uint16_t idx = retb[1];
    if (state->branch_info_unk[idx] & 8 || idx == 0x29)
        return NULL;

    return ret;
}

uint32_t is_branch_taken(const char* info, const struct script_state* state) {
    uint16_t ret[3];

    if (sub_80063B8(info, ret, state))
        return ret[0] & UINT16_MAX;
    return UINT32_MAX;
}

static void skip_cmd(const struct script_state* state, uint16_t* dst) {
    *dst += 2 + 2 + sizeof(uint16_t) * ((union script_cmd*)((uint8_t*)state->cmds + *dst))->arg;
}

void branch_dst(const struct script_state* state, uint16_t* dst) {
    while (1) {
        union script_cmd* cmd = (void*)((uint8_t*)state->cmds + *dst);

        /* Seek until next branch (op 5 or 6) or nop (7) */
        if (cmd_can_be_branched_to(cmd)) {
            skip_cmd(state, dst);
            continue;
        }
    }
}
