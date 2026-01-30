#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "alisp.h"
#include "util.h"
#include "mpc.h"

#define LVAL_ALLOC() ((lval *)malloc(sizeof(lval)))

static
char* ltype_name(i32 t) {
    switch(t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

static
lval* lval_num(i64 num) {
    lval *out = malloc(sizeof(lval));
    out->type = LVAL_NUM;
    out->num = num;
    return out;
}

static
void lenv_def(lenv *e, lval *k, lval *v) {
    while (e->parent) { e = e->parent; }
    lenv_put(e, k, v);
}

static
lval* lval_copy(lval *v) {
    lval *copy = LVAL_ALLOC();
    copy->type = v->type;

    switch (v->type) {
        case LVAL_NUM:
            copy->num = v->num; break;
        case LVAL_FUN:
            copy->fun = v->fun; break;

        case LVAL_SYM:
            copy->sym = (char *)malloc(strlen(v->sym) + 1); break;
        case LVAL_ERR:
            copy->err = (char *)malloc(strlen(v->err) + 1); break;

        case LVAL_QEXPR:
        case LVAL_SEXPR: {
            lval **cc = malloc(sizeof(lval *) * v->count);
            for (i32 i = 0; i < v->count; i++) {
                cc[i] = v->cell[i];
            }
            copy->cell = cc;
            copy->count = v->count;
            break;
        }
    }

    return copy;
}

static
lval* lval_err(char *fmt, ...) {
    lval *out = malloc(sizeof(lval));
    out->type = LVAL_ERR;

    va_list va;
    va_start(va, fmt);

    out->err = (char *)malloc(512);
    vsnprintf(out->err, 511, fmt, va);
    out->err = realloc(out->err, strlen(out->err) + 1);
    va_end(va);

    DBG_LOG("an error was created -> lval_err(\"%s\")\n", out->err);
    return out;
}

static
lval* lval_sym(char *sym) {
    lval *out = malloc(sizeof(lval));
    out->type = LVAL_SYM;
    out->sym = (char *)malloc(strlen(sym) + 1);
    strcpy(out->sym, sym);
    return out;
}

static
lval* lval_fun(lbuiltin fun) {
    lval *out = malloc(sizeof(lval));
    out->type = LVAL_FUN;
    out->fun = fun;
    return out;
}

static
lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

static
lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

static
lval* lval_add(lval *v, lval *x) {
    v->count++;
    v->cell = realloc(v->cell, v->count * sizeof(lval*));
    v->cell[v->count - 1] = x;
    return v;
}

static
lval* lval_pop(lval *v, i32 i) {
    lval *x = v->cell[i];
    memmove(&v->cell[i], &v->cell[i + 1],
            sizeof(lval*) * (v->count - i - 1));
    v->count--;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

static
lval* lval_take(lval* v, i32 i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

static
lval* lval_lambda(lval *formals, lval *body) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;

  v->fun = NULL;
  v->env = lenv_new();
  v->formals = formals;
  v->body = body;
  return v;
}

static
lval *builtin_lambda(lenv *e, lval *v) {
    LASSERT_NARGS("\\", v, 2);
    LASSERT_TYPE("\\", v, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", v, 1, LVAL_QEXPR);

    for (i32 i = 0; i < v->cell[0]->count; i++) {
        LASSERT(v,
                (v->cell[0]->cell[i]->type == LVAL_SYM),
                "Cannot define non-symbol. Got %s, Expected %s",
                ltype_name(v->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }

    lval *formals = lval_pop(v, 0);
    lval *body = lval_pop(v, 0);
    lval_del(v);

    return lval_lambda(formals, body);
}

static
lval* builtin_op(lenv *e, lval *first, char* op) {
    for (int i = 0; i < first->count; i++) {
        if (first->cell[i]->type != LVAL_NUM) {
            lval_del(first);
            return lval_err("Cannot operate on non-number!");
        }
    }

    lval* x = lval_pop(first, 0);
    if ((strcmp(op, "-") == 0) && first->count == 0) {
        x->num = -x->num;
    }

    while (first->count > 0) {
        lval* y = lval_pop(first, 0);

        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y == 0) {
                lval_del(x); lval_del(y);
                lval_err("division by zero"); break;
            }
            x->num /= y->num;
        }

        lval_del(y);
    }
    lval_del(first);

    return x;
}

static
lval* builtin_head(lenv *e, lval *v) {
    LASSERT(v, v->count == 1, "'head' too many arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
            "'head' incorrect type for argument 0. Got %s, Expected %s", ltype_name(v->cell[0]->type), ltype_name(LVAL_QEXPR));
    LASSERT(v, v->cell[0]->count != 0, "'head' cannot work on empty qexpr {}");
    lval *result = lval_take(v, 0);
    while (result->count > 1) { lval_del(lval_pop(result, 1)); }
    return result;
}

static
lval* builtin_tail(lenv *e, lval *v) {
    LASSERT(v, v->count == 1, "'tail' too many arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
            "'tail' incorrect type for argument 0. Got %s, Expected %s",
            ltype_name(v->cell[0]->type), ltype_name(LVAL_QEXPR));
    LASSERT(v, v->cell[0]->count != 0, "'tail' cannot work on empty qexpr {}");
    lval *result = lval_take(v, 0);
    lval_del(lval_pop(result, 0));
    return result;
}

static
lval* builtin_eval(lenv *e, lval *v) {
    LASSERT(v, v->count == 1, "'eval' too many arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
            "'eval' incorrect type for argument 0. Got %s, Expected %s",
            ltype_name(v->cell[0]->type), ltype_name(LVAL_QEXPR));
    lval *x = lval_take(v, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

static
lval *lval_join(lval *v1, lval *v2) {
    while (v2->count) {
        lval *popped = lval_pop(v2, 0);
        v1 = lval_add(v1, popped);
    }
    lval_del(v2);
    return v1;
}


static
lval* builtin_join(lenv *e, lval *v) {
    for (i32 i = 0; i < v->count; i++) {
        LASSERT(v, v->cell[i]->type == LVAL_QEXPR,
                "'join' incorrect type for argument %d. Got %s, Expected %s",
                i, ltype_name(v->cell[i]->type), ltype_name(LVAL_QEXPR));
    }

    lval *x = lval_pop(v, 0);
    while (v->count) {
        lval *popped = lval_pop(v, 0);
        x = lval_join(x, popped);
    }

    lval_del(v);
    return x;
}

static
lval* builtin_cons(lenv *e, lval *v) {
    LASSERT(v, v->count == 2, "'cons' needs 2 arguments");

    lval *v1 = lval_pop(v, 0);
    LASSERT(v1, v1->type == LVAL_NUM || v1->type == LVAL_SYM,
            "'cons' incorrect type for argument 0. Got %s, Expected %s or %s",
            ltype_name(v1->type), ltype_name(LVAL_NUM), ltype_name(LVAL_SYM));

    lval *v2 = lval_pop(v, 0);
    LASSERT(v2, v2->type == LVAL_QEXPR,
            "'cons' incorrect type for argument 1. Got %s, Expected %s",
            ltype_name(v2->type), ltype_name(LVAL_QEXPR));

    lval *out = lval_qexpr();
    lval_add(out, v1);
    out = lval_join(out, v2);
    return out;
}

static
lval* builtin_init(lenv *e, lval *v) {
    LASSERT(v, v->count == 1, "'init' too many arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
            "'init' incorrect type for argument 0. Got %s, Expected %s",
            ltype_name(v->cell[0]->type), ltype_name(LVAL_QEXPR));
    lval *x = lval_pop(v, 0);
    lval *out = lval_pop(x, x->count - 1);
    lval_del(out);
    return x;
}

static
lval* builtin_len(lenv *e, lval *v) {
    LASSERT(v, v->count == 1, "'len' too many arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
            "'len' incorrect type for argument 0. Got %s, Expected %s",
            ltype_name(v->cell[0]->type), ltype_name(LVAL_QEXPR));
    i64 len = v->cell[0]->count;
    lval_del(v);
    return lval_num(len);
}

static
lval* builtin_def(lenv* e, lval* v) {
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
            "'def' incorrect type for argument 0. Got %s, Expected %s",
            ltype_name(v->cell[0]->type), ltype_name(LVAL_QEXPR));

    lval *syms = v->cell[0];
    for (i32 i = 0; i < syms->count; i++) {
        LASSERT(v, syms->cell[i]->type == LVAL_SYM,
                "'def' incorrect type for symbol %d. Got %s, Expected %s",
                i, ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    }

    LASSERT(v, syms->count == v->count - 1,
            "'def' cannot define incorrect number of values to symbols");

    for (i32 i = 0; i < syms->count; i++) {
        lenv_put(e, syms->cell[i], v->cell[i + 1]);
    }

    lval_del(v);
    return lval_sexpr();
}

static lval *builtin_list(lenv *e, lval *v) {
    v->type = LVAL_QEXPR;
    return v;
}

static lval *builtin_add(lenv *e, lval *v) { return builtin_op(e, v, "+"); }
static lval *builtin_sub(lenv *e, lval *v) { return builtin_op(e, v, "-"); }
static lval *builtin_mul(lenv *e, lval *v) { return builtin_op(e, v, "*"); }
static lval *builtin_div(lenv *e, lval *v) { return builtin_op(e, v, "/"); }

static
lval *builtin_var(lenv *e, lval *a, char *func) {
    LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

    lval *syms = a->cell[0];
    for (int i = 0; i < syms->count; i++)
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
                "Function '%s' cannot define non-symbol. "
                "Got %s, Expected %s.",
                func,
                ltype_name(syms->cell[i]->type),
                ltype_name(LVAL_SYM));

    LASSERT(a, (syms->count == a->count - 1),
            "Function '%s' passed too many arguments for symbols. "
            "Got %i, Expected %i.",
            func, syms->count, a->count - 1);

    for (int i = 0; i < syms->count; i++) {
        if (strcmp(func, "def") == 0)
            lenv_def(e, syms->cell[i], a->cell[i + 1]);

        if (strcmp(func, "=") == 0)
            lenv_put(e, syms->cell[i], a->cell[i + 1]);
    }

    lval_del(a);
    return lval_sexpr();
}

static
lval* builtin_put(lenv* e, lval* a) {
  return builtin_var(e, a, "=");
}

lval* lval_call(lenv* e, lval* f, lval* v) {
    if (f->fun) { return f->fun(e, v); }

    i32 given = v->count;
    i32 total_formal = f->formals->count;

    while (v->count) {
        if (f->formals->count == 0) {
            lval_del(v); return lval_err(
                    "function call received too many arguments. Got %i, Expected %i",
                    given, total_formal);
        }

        lval *sym = lval_pop(f->formals, 0);
        lval *val = lval_pop(v, 0);

        lenv_put(f->env, sym, val);
        lval_del(sym); lval_del(val);
    }

    lval_del(v);

    if (f->formals->count == 0) {
        f->env->parent = e;
        return builtin_eval(f->env,
                lval_add(lval_sexpr(), lval_copy(f->body)));
    } else {
        return lval_copy(f);
    }
}

lval* lval_eval_sexpr(lenv *e, lval *v) {
    for (i32 i = 0; i < v->count; i++)
        v->cell[i] = lval_eval(e, v->cell[i]);

    for (int i = 0; i < v->count; i++)
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }

    if (v->count == 0) { return 0; }
    if (v->count == 1) { return lval_take(v, 0); }

    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval_del(f); lval_del(v);
        return lval_err("S-expression does not start with a function");
    }

    lval *result = lval_call(e, f, v);
    lval_del(f);
    return result;
}

static
void lval_expr_print(char open, lval *v, char close) {
    putchar(open);
    for (i32 i = 0; i < v->count; i++) {
        lval_print((lval *)v->cell[i]);
        if (i != (v->count-1)) {
            putchar( ' ');
        }
    }
    putchar(close);
}

static
lval *lval_read_num(mpc_ast_t *t) {
    errno = 0;
    i64 v = strtol(t->contents, NULL, 10);
    if (errno == ERANGE) {
        return lval_err("invalid_number");
    }
    return lval_num(v);
}

lenv* lenv_new(void) {
    lenv *out = (lenv *)malloc(sizeof(lenv));
    out->parent = NULL;
    out->count = 0;
    out->syms = NULL;
    out->vals = NULL;
    return out;
}

void lenv_add_builtin(lenv *e, char *name, lbuiltin fun) {
    lval *k = lval_sym(name);
    lval *v = lval_fun(fun);
    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv *e) {
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "cons", builtin_cons);
    lenv_add_builtin(e, "len", builtin_len);
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);
    lenv_add_builtin(e, "init", builtin_init);

    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);

    lenv_add_builtin(e, "\\", builtin_lambda);
}

void lenv_del(lenv* e) {
    for (i32 i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms); free(e->vals); free(e);
}

lval* lenv_get(lenv* e, lval* k) {
    LASSERT(k, k->type == LVAL_SYM, "query value must be of type symbol");
    for (i32 i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }

    if (e->parent) {
        return lenv_get(e->parent, k);
    } else {
        return lval_err("unbound symbol");
    }
}

lenv* lenv_copy(lenv *e) {
    lenv *copy = lenv_new();
    copy->parent = e->parent;
    copy->count = e->count;
    copy->syms = (char **)malloc(sizeof(char *) * copy->count);
    copy->vals = (lval **)malloc(sizeof(lval *) * copy->count);

    for (i32 i = 0; i < copy->count; i++) {
        copy->syms[i] = (char *)malloc(strlen(e->syms[i]) + 1);
        strcpy(copy->syms[i], e->syms[i]);
        copy->vals[i] = lval_copy(e->vals[i]);
    }

    return copy;
}

void lenv_put(lenv *e, lval *k, lval *v) {
    for (i32 i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    e->count++;
    e->syms = realloc(e->syms, sizeof(char *) * e->count);
    e->vals = realloc(e->vals, sizeof(lval *) * e->count);

    e->vals[e->count - 1] = lval_copy(v);
    e->syms[e->count - 1] = (char *)malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
}

lval* lval_read(mpc_ast_t *node) {
    if (strstr(node->tag, "number")) return lval_read_num(node);
    if (strstr(node->tag, "symbol")) return lval_sym(node->contents);

    lval *x = NULL;
    if (strcmp(node->tag, ">") == 0) { x = lval_sexpr(); };
    if (strstr(node->tag, "sexpr"))  { x = lval_sexpr(); };
    if (strstr(node->tag, "qexpr"))  { x = lval_qexpr(); }

    for (i32 i=0; i < node->children_num; i++) {
        if (strcmp(node->children[i]->contents, "(") == 0)      continue;
        if (strcmp(node->children[i]->contents, ")") == 0)      continue;
        if (strcmp(node->children[i]->contents, "{") == 0)      continue;
        if (strcmp(node->children[i]->contents, "}") == 0)      continue;
        if (strcmp(node->children[i]->tag,      "regex") == 0)  continue;
        x = lval_add(x, lval_read(node->children[i]));
    }

    return x;
}

void lval_del(lval *v) {
    switch (v->type) {
        case LVAL_FUN: 
            if (!v->fun) {
                lval_del(v->formals);
                lval_del(v->body);
                lenv_del(v->env);
            }
            break;
        case LVAL_NUM: break;

        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        case LVAL_QEXPR:
        case LVAL_SEXPR: {
                             for (i32 i=0; i< v->count; i++) {
                                 lval_del((lval *)v->cell[i]);
                             }
                             free(v->cell);
                             break;
                         }
    }
}

void lval_print(lval *v) {
    switch (v->type) {
        case LVAL_FUN:   printf("<function>");         break;
        case LVAL_NUM:   printf("%lli", v->num);       break;
        case LVAL_ERR:   printf("error: %s", v->err);  break;
        case LVAL_SYM:   printf("%s",  v->sym);        break;
        case LVAL_QEXPR: lval_expr_print('{', v, '}'); break;
        case LVAL_SEXPR: lval_expr_print('(', v, ')'); break;
    }
}

void lval_println(lval *v) { lval_print(v); putchar('\n'); }

lval* lval_eval(lenv *e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval *x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
    return v;
}
