#pragma once

#include "types.h"
#include "mpc.h"

#define LASSERT(arg, cond, fmt, ...)               \
    if (!(cond))                                   \
    {                                              \
        lval *_err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(arg);                             \
        return _err;                               \
    }

#define LASSERT_NARGS(func, args, expected)                                    \
    LASSERT(args, args->count == expected,                                     \
            "'%s' passed incorrect number of arguments. Got %i, expected %i.", \
            func, args->count, num, )

#define LASSERT_TYPE(func, args, index, expect)                     \
    LASSERT(args, args->cell[index]->type == expect,                \
            "Function '%s' passed incorrect type for argument %i. " \
            "Got %s, Expected %s.",                                 \
            func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NOT_EMPTY(func, args, index)     \
    LASSERT(args, args->cell[index]->count != 0, \
            "Function '%s' passed {} for argument %i.", func, index);

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
    i32 type;

    i64 num;
    char *err;
    char *sym;

    lbuiltin fun;
    lenv* env;
    lval* formals;
    lval* body;

    i32 count;
    struct lval **cell;
};

struct lenv {
    lenv *parent;
    i32 count;
    char** syms;
    lval** vals;
};

enum {
    LVAL_ERR,
    LVAL_NUM,
    LVAL_SYM,
    LVAL_FUN,   
    LVAL_SEXPR,   
    LVAL_QEXPR
};

lenv* lenv_new(void);
void lenv_del(lenv *e);
lenv* lenv_copy(lenv *e);
void lenv_put(lenv *e, lval *k, lval *v);
lval* lenv_get(lenv* e, lval* k);
void lenv_add_builtin(lenv *e, char *name, lbuiltin fun);
void lenv_add_builtins(lenv *e);

void lval_del(lval *v);
lval* lval_eval(lenv *e, lval *v);

void lval_print(lval *v);
void lval_println(lval *v);

lval* lval_read(mpc_ast_t *node);
