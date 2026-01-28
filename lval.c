#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "lval.h"
#include "util.h"
#include "mpc.h"

static
lval* lval_num(i64 num) {
    lval *out = malloc(sizeof(lval));
    out->type = LVAL_NUM;
    out->num = num;
    return out;
}

static
lval* lval_err(char *err) {
    DBG_LOG("an error was created -> lval_err(\"%s\")\n", err);
    lval *out = malloc(sizeof(lval));
    out->type = LVAL_ERR;
    out->err = (char *)malloc(strlen(err) + 1);
    strcpy(out->err, err);
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
lval* builtin_op(lval *first, char* op) {
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
lval* builtin_head(lval *v) {
    LASSERT(v, v->count == 1, "'head' too many arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR, "'head' incorrect argument type, qexpr expected");
    LASSERT(v, v->cell[0]->count != 0, "'head' cannot work on empty qexpr {}");
    lval *result = lval_take(v, 0);
    while (result->count > 1) { lval_del(lval_pop(result, 1)); }
    return result;
}

static
lval* builtin_tail(lval *v) {
    LASSERT(v, v->count == 1, "'tail' too many arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR, "'tail' incorrect argument type, qexpr expected");
    LASSERT(v, v->cell[0]->count != 0, "'tail' cannot work on empty qexpr {}");
    lval *result = lval_take(v, 0);
    lval_del(lval_pop(result, 0));
    return result;
}

static
lval* builtin_list(lval *v) {
    v->type = LVAL_QEXPR;
    return v;
}

static
lval* builtin_eval(lval *v) {
    LASSERT(v, v->count == 1, "'eval' too many arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR, "'eval' incorrect argument type, qexpr expected");
    lval *x = lval_take(v, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

static
lval* lval_join(lval *v1, lval *v2) {
    while (v2->count) {
        lval *popped = lval_pop(v2, 0);
        v1 = lval_add(v1, popped);
    }
    lval_del(v2);
    return v1;
}

static
lval* builtin_join(lval *v) {
    for (i32 i = 0; i < v->count; i++) {
        LASSERT(v, v->cell[i]->type == LVAL_QEXPR,
                "'join' incorrect argument type, qexpr expected");
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
lval* builtin_cons(lval *v) {
    LASSERT(v, v->count == 2, "'cons' needs 2 arguments");
    lval *v1 = lval_pop(v, 0);
    LASSERT(v1, v1->type == LVAL_NUM || v1->type == LVAL_SYM, "first argument my be of type number of symbol");
    lval *v2= lval_pop(v, 0);
    LASSERT(v2, v2->type == LVAL_QEXPR, "'cons' incorrect second argument type, qexpr expected");

    lval *out = lval_qexpr();
    lval_add(out, v1);
    out = lval_join(out, v2);
    return out;
}

static
lval* builtin_init(lval *v) {
    LASSERT(v, v->count == 1, "'init' too many arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR, "'init' incorrect argument type, qexpr expected");
    lval *x = lval_pop(v, 0);
    lval *out = lval_pop(x, x->count - 1);
    lval_del(out);
    return x;
}

static
lval* builtin_len(lval *v) {
    LASSERT(v, v->count == 1, "'len' too many arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR, "'len' incorrect argument type, qexpr expected");
    i64 len = v->cell[0]->count;
    lval_del(v);
    return lval_num(len);
}

static
lval* builtin(lval* v, char *func) {
    if (strcmp("list", func) == 0) { return builtin_list(v); }
    if (strcmp("head", func) == 0) { return builtin_head(v); }
    if (strcmp("tail", func) == 0) { return builtin_tail(v); }
    if (strcmp("join", func) == 0) { return builtin_join(v); }
    if (strcmp("eval", func) == 0) { return builtin_eval(v); }
    if (strcmp("init", func) == 0) { return builtin_init(v); }
    if (strcmp("len",  func) == 0) { return builtin_len(v);  }
    if (strcmp("cons", func) == 0) { return builtin_cons(v); }
    if (strstr("+/-*", func)) { return builtin_op(v, func); }
    lval_del(v);
    return lval_err("unknown function");
}

static
lval* lval_eval_sexpr(lval* v) {
    for (i32 i = 0; i < v->count; i++)
        v->cell[i] = lval_eval(v->cell[i]);

    for (int i = 0; i < v->count; i++)
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }

    if (v->count == 0) { return 0; }
    if (v->count == 1) { return lval_take(v, 0); }

    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f); lval_del(v);
        return lval_err("S-expression does not start with a symbol");
    }

    lval* result = builtin(v, f->sym);
    lval_del(f);
    return result;
}

static
void lval_expr_print(char open, lval *v, char close) {
    putchar(open);
    for (i32 i = 0; i < v->count; i++) {
        lval_print((lval *)v->cell[i]);
        if (i != (v->count-1)) {
            putchar(' ');
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
        case LVAL_NUM: printf("%lli", v->num); break;
        case LVAL_ERR: printf("error: %s", v->err); break;
        case LVAL_SYM: printf("%s",  v->sym); break;
        case LVAL_QEXPR:  lval_expr_print('{', v, '}'); break;
        case LVAL_SEXPR:  lval_expr_print('(', v, ')'); break;
    }
}

void lval_println(lval *v) { lval_print(v); putchar('\n'); }

lval* lval_eval(lval* v) {
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
    return v;
}
