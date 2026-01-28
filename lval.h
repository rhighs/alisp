#pragma once

#include "types.h"
#include "mpc.h"

#define LASSERT(arg, cond, err) \
    if (!(cond)) { lval_del(arg); return lval_err(err); }

typedef struct lval {
    i32 type;
    i64 num;

    char *err;
    char *sym;

    i32 count;
    struct lval **cell;
} lval;

enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

void lval_del(lval *v);
lval* lval_eval(lval* v);

void  lval_print(lval* v);
void  lval_println(lval* v);

lval* lval_read(mpc_ast_t* node);
