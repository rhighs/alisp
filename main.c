#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <editline/readline.h>

#include "mpc.h"
#include "types.h"

enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

typedef struct {
    i32 type;
    i64 num;

    char *err;
    char *sym;

    i32 count;
    struct lval **cell;
} lval;

lval* lval_num(i64 num) {
    lval *out = malloc(sizeof(lval));
    out->type = LVAL_NUM;
    out->num = num;
    return out;
}

lval* lval_err(char *err) {
    lval *out = malloc(sizeof(lval));
    out->type = LVAL_ERR;
    out->err = (char *)malloc(strlen(err) + 1);
    strcpy(out->err, err);
    return out;
}

lval* lval_sym(char *sym) {
    lval *out = malloc(sizeof(lval));
    out->type = LVAL_SYM;
    out->sym = (char *)malloc(strlen(sym) + 1);
    strcpy(out->sym, sym);
    return out;
}

lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* eval_op(lval *x, char* op, lval *y) {
    if (x->type == LVAL_ERR) { return x; }
    if (y->type == LVAL_ERR) { return y; }

    i64 xx = x->num;
    i64 yy = y->num;

    if (strcmp(op, "+") == 0) { return lval_num(xx + yy); }
    if (strcmp(op, "-") == 0) { return lval_num(xx - yy); }
    if (strcmp(op, "*") == 0) { return lval_num(xx * yy); }
    if (strcmp(op, "/") == 0) {
        if (yy == 0) {
            return lval_err("division by zero");
        }
        return lval_num(xx / yy);
    }
    return lval_err("bad operator");
}

void lval_print(lval *v) {
    switch (v->type) {
        case LVAL_NUM: printf("%lli", v->num); break;
        case LVAL_ERR: printf("error: %s", v->err); break;
        case LVAL_SYM: printf("%s",  v->sym); break;
        case LVAL_SEXPR: {
                             putchar('(');
                             for (i32 i = 0; i < v->count; i++) {
                                 lval_print((lval *)v->cell[i]);
                                 if (i != (v->count-1)) {
                                     putchar(' ');
                                 }
                             }
                             putchar(')');
                             break;
                         }
    }
}

void lval_println(lval *v) { lval_print(v); putchar('\n'); }

void lval_del(lval *v) {
    switch (v->type) {
        case LVAL_NUM: break;

        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        case LVAL_SEXPR: {
                             for (i32 i=0; i< v->count; i++) {
                                 lval_del((lval *)v->cell[i]);
                             }
                             free(v->cell);
                             break;
                         }
    }
}

lval *lval_read_num(mpc_ast_t *t) {
    errno = 0;
    i64 v = strtol(t->contents, NULL, 10);
    if (errno == ERANGE) {
        return lval_err("invalid_number");
    }
    return lval_num(v);
}

lval* lval_add(lval *v, lval *x) {
    v->count++;
    v->cell = realloc(v->cell, v->count * sizeof(lval*));
    v->cell[v->count - 1] = (struct lval*)x;
    return v;
}

lval* lval_read(mpc_ast_t *node) {
    if (strstr(node->tag, "number")) return lval_read_num(node);
    if (strstr(node->tag, "symbol")) return lval_sym(node->contents);

    lval *x = NULL;
    if (strcmp(node->tag, ">") == 0) { x = lval_sexpr(); };
    if (strstr(node->tag, "sexpr"))  { x = lval_sexpr(); };
    printf("%s\n", node->contents);

    for (i32 i=0; i < node->children_num; i++) {
        if (strcmp(node->children[i]->contents, "(") == 0)      continue;
        if (strcmp(node->children[i]->contents, ")") == 0)      continue;
        if (strcmp(node->children[i]->tag,      "regex") == 0)  continue;
        x = lval_add(x, lval_read(node->children[i]));
    }

    return x;
}

i32 num_nodes(mpc_ast_t *node) {
    printf("node -> %s\n", node->tag);
    if (node->children_num == 0) { return 1; };
    if (node->children_num >= 1) {
        i32 total = 1;
        for (i32 i = 0; i < node->children_num; i++) {
            total = total + num_nodes(node->children[i]);
        }
        return total;
    };
    return 0;
}

i32 main(i32 argc, char** argv) {
    // if (argc > 1) {
    //     if (!strcmp(argv[0], "--help")) {
    //         puts("Usage: alisp <source-file>");
    //         return 0;
    //     }
    //     u8 buf[512];
    //     const char *filepath = argv[1];
    //     u64 size = io_read(filepath, (u8 **)&buf);
    //     printf("(%s) file size: %llu\n", filepath, size);
    //     return 0;
    // }

    mpc_parser_t *Number   = mpc_new("number");
    mpc_parser_t *Symbol   = mpc_new("symbol");
    mpc_parser_t *Expr     = mpc_new("expr");
    mpc_parser_t *Sexpr    = mpc_new("sexpr");
    mpc_parser_t *Alisp    = mpc_new("alisp");

    mpca_lang(MPCA_LANG_DEFAULT,
            " number   : /-?[0-9]+/ ;                             "
            " symbol : '+' | '-' | '*' | '/' ;                    "
            " sexpr    : '(' <expr>* ')' ;                        "
            " expr     : <number> | <symbol> | <sexpr> ;          "
            " alisp    : /^/ <expr>* /$/ ;                        ",
            Number, Symbol, Sexpr, Expr, Alisp);

    puts("Alisp Version 0.0.1");

    while (1) {
        char *input = readline("alisp> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Alisp, &r)) {
            lval *x = lval_read(r.output);
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }

    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Alisp);

    return 0;
}
