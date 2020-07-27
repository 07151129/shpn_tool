#ifndef SCRIPT_PARSE_CTX_H
#define SCRIPT_PARSE_CTX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "defs.h"

struct script_parse_ctx {
    const char* filename;
    const char* script;

#define SCRIPT_PARSE_DIAGS_SZ 10
    struct script_diag {
        enum {/*DIAG_WARN,*/ DIAG_ERR} kind;
        size_t line, col;
        const char* msg;
    } diags[SCRIPT_PARSE_DIAGS_SZ];
    size_t ndiags;

#define SCRIPT_PARSE_CTX_STMTS_SZ 20000
    struct script_stmt {
        /**
         * NOTE: This is the only way to get in-order iterator for stmts!
         * The raw unordered storage can be accessed by indexing stmts[0..nstmts-1].
         */
        struct script_stmt* next, * prev;

        enum {STMT_TY_OP, STMT_TY_BYTE, STMT_TY_BEGIN_END} ty;

        const char* label;

        /* We need to preserve line idx to be able to report diags after we're done parsing */
        size_t line;

        union {
            struct script_op_stmt {
                size_t idx;

                struct script_arg_list {
                    int nargs;
#define SCRIPT_PARSE_CTX_ARGS_SZ 5
                    struct script_arg {
                        enum {ARG_TY_STR, ARG_TY_LABEL, ARG_TY_NUM, ARG_TY_NUMBERED_STR} type;
                        union {
                            const char* str;
                            const char* label;
                            uint16_t num;
                            struct {
                                uint16_t num;
                                const char* str;
                            } numbered_str;
                        };
                    } args[SCRIPT_PARSE_CTX_ARGS_SZ];
                } args;
            } op;

            struct script_byte_stmt {
                int n;
                uint32_t val;
            } byte;

            struct script_begin_end_stmt {
                bool begin;
                const char* section;
            } begin_end;
        };
    } stmts[SCRIPT_PARSE_CTX_STMTS_SZ];
    size_t nstmts;
};

bool script_parse_ctx_init(struct script_parse_ctx* ctx, const char* script);
bool script_parse_ctx_parse(struct script_parse_ctx* ctx);
bool script_parse_ctx_add_diag(struct script_parse_ctx* ctx, const struct script_diag* diag);
bool script_arg_list_add_arg(struct script_arg_list* args, const struct script_arg* arg);
void script_arg_free(const struct script_arg* arg);
void script_stmt_free(struct script_stmt* stmt);
void script_parse_ctx_free(struct script_parse_ctx* ctx);
bool script_op_idx(const char* name, size_t* dst);
bool script_op_idx_chk(size_t idx);
bool script_ctx_add_stmt(struct script_parse_ctx* ctx, const struct script_stmt* stmt);

#endif
