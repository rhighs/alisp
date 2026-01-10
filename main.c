#include <stdio.h>
#include <string.h>

#include <editline/readline.h>

#include "mpc.h"
#include "types.h"

enum { LVAL_NUM, LVAL_ERR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

typedef struct {
    i32 type;
    i64 num;
    i32 err;
} lval;

lval lval_num(i64 num) {
    lval out;
    out.type = LVAL_NUM;
    out.num = num;
    return out;
}

// err: LERR_*
lval lval_err(i32 err) {
    lval out;
    out.type = LVAL_ERR;
    out.err = err;
    return out;
}

lval eval_op(lval x, char* op, lval y) {
    if (x.type == LVAL_ERR) { return x; }
    if (y.type == LVAL_ERR) { return y; }

    i64 xx = x.num;
    i64 yy = y.num;

    if (strcmp(op, "+") == 0) { return lval_num(xx + yy); }
    if (strcmp(op, "-") == 0) { return lval_num(xx - yy); }
    if (strcmp(op, "*") == 0) { return lval_num(xx * yy); }
    if (strcmp(op, "/") == 0) {
        if (yy == 0) {
            return lval_err(LERR_DIV_ZERO);
        }
        return lval_num(xx / yy);
    }
    return lval_err(LERR_BAD_OP);
}

void lval_print(lval v) {
    switch (v.type) {
        case LVAL_NUM: printf("%lli", v.num); break;
        case LVAL_ERR:
                       if (v.err == LERR_DIV_ZERO) {
                           printf("error: division by zero");
                       }

                       if (v.err == LERR_BAD_OP)   {
                           printf("error: invalid operator");
                       }

                       if (v.err == LERR_BAD_NUM)  {
                           printf("error: invalid number");
                       }
                       break;

    }
}

void lval_println(lval v) { lval_print(v); putchar('\n'); }

lval eval(mpc_ast_t *node) {
    if (strstr(node->tag, "number")) {
        errno = 0;
        i64 x = strtol(node->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    char* op = node->children[1]->contents;

    lval x = eval(node->children[2]);

    i32 i = 3;
    while (strstr(node->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(node->children[i]));
        i++;
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
    mpc_parser_t *Operator = mpc_new("operator");
    mpc_parser_t *Expr     = mpc_new("expr");
    mpc_parser_t *Alisp    = mpc_new("alisp");

    mpca_lang(MPCA_LANG_DEFAULT,
            " number   : /-?[0-9]+/ ;                             "
            " operator : '+' | '-' | '*' | '/' ;                  "
            " expr     : <number> | '(' <operator> <expr>+ ')' ;  "
            " alisp    : /^/ <operator> <expr>+ /$/ ;             ",
            Number, Operator, Expr, Alisp);

    puts("Alisp Version 0.0.1");

    while (1) {
        char *input = readline("alisp> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Alisp, &r)) {
            lval_println(eval(r.output));
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        printf("%s\n", input);
        free(input);
    }

    mpc_cleanup(4, Number, Operator, Expr, Alisp);

    return 0;
}
