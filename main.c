#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <editline/readline.h>

#include "mpc.h"
#include "types.h"

#ifdef DEBUG
#define DBG_LOG(FMT, ...) \
    do{fprintf(stderr, "[%s:%s:%d]: " FMT "\n", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);}while(0)
#else
#define DBG_LOG(FMT, ...) do{}while(0)
#endif

enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

typedef struct lval {
    i32 type;
    i64 num;

    char *err;
    char *sym;

    i32 count;
    struct lval **cell;
} lval;

void fatal(const char* message) {
    fprintf(stderr, "fatal: %s\n", message);
    exit(1);
}

lval* lval_num(i64 num) {
    lval *out = malloc(sizeof(lval));
    out->type = LVAL_NUM;
    out->num = num;
    return out;
}

lval* lval_err(char *err) {
    DBG_LOG("an error was created -> lval_err(\"%s\")\n", err);
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

lval* lval_pop(lval *v, i32 i) {
    lval *x = (lval *)v->cell[i];
    memmove(&v->cell[i], &v->cell[i + 1],
            sizeof(lval*) * (v->count - i - 1));
    v->count--;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, i32 i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

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

lval* lval_eval(lval* v);

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

    lval* result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}

lval* lval_eval(lval* v) {
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
    return v;
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

    for (i32 i=0; i < node->children_num; i++) {
        if (strcmp(node->children[i]->contents, "(") == 0)      continue;
        if (strcmp(node->children[i]->contents, ")") == 0)      continue;
        if (strcmp(node->children[i]->tag,      "regex") == 0)  continue;
        x = lval_add(x, lval_read(node->children[i]));
    }

    return x;
}

static
i32 __read_file_size(const char *filepath) {
    struct stat stat_buf;
    int rc = stat(filepath, &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

u32 io_read_file(const char* filepath, u8 **buf_ptr) {
    FILE *file = fopen(filepath, "rb");

    i32 filesize = __read_file_size(filepath);
    if (filesize == -1) {
        *buf_ptr = NULL;
        return 0;
    }

    u8* buffer = (u8*)malloc(filesize + 1);
    while (!feof(file)) {
        fread(buffer, 1, filesize, file);
    }
    fclose(file);
    buffer[filesize] = '\0';
    *buf_ptr = buffer;
    return filesize;
}

i32 main(i32 argc, char** argv) {
    if (argc > 1) {
        if (!strcmp(argv[0], "--help")) {
            puts("Usage: alisp <source-file>");
            return 0;
        }

        u8 source[512];
        const char *filepath = argv[1];
        u64 size = io_read_file(filepath, (u8 **)&source);
        return 0;
    }

    mpc_parser_t *Number   = mpc_new("number");
    mpc_parser_t *Symbol   = mpc_new("symbol");
    mpc_parser_t *Expr     = mpc_new("expr");
    mpc_parser_t *Sexpr    = mpc_new("sexpr");
    mpc_parser_t *Alisp    = mpc_new("alisp");

    mpca_lang(MPCA_LANG_DEFAULT,
            " number : /-?[0-9]+/ ;                             "
            " symbol : '+' | '-' | '*' | '/' ;                  "
            " sexpr  : '(' <expr>* ')' ;                        "
            " expr   : <number> | <symbol> | <sexpr> ;          "
            " alisp  : /^/ <expr>* /$/ ;                        ",
            Number, Symbol, Sexpr, Expr, Alisp);

    puts("Alisp Version 0.0.1");

    while (1) {
        char *input = readline("alisp> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Alisp, &r)) {
            lval *x = lval_eval(lval_read(r.output));
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

