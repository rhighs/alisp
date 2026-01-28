#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <editline/readline.h>

#include "mpc.h"
#include "types.h"
#include "lval.h"

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
    mpc_parser_t *Qexpr    = mpc_new("qexpr");
    mpc_parser_t *Expr     = mpc_new("expr");
    mpc_parser_t *Sexpr    = mpc_new("sexpr");
    mpc_parser_t *Alisp    = mpc_new("alisp");

    mpca_lang(MPCA_LANG_DEFAULT,
            " number : /-?[0-9]+/ ;                                             "
            " symbol : \"list\" | \"head\" | \"tail\" | \"cons\" | \"init\"     "
            "        | \"join\" | \"eval\" | \"len\"  | '+' | '-' | '*' | '/' ; "
            " sexpr  : '(' <expr>* ')' ;                                        "
            " qexpr  : '{' <expr>* '}' ;                                        "
            " expr   : <number> | <symbol> | <sexpr> | <qexpr>;                 "
            " alisp  : /^/ <expr>* /$/ ;                                        ",
            Number, Symbol, Sexpr, Qexpr, Expr, Alisp);

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

    mpc_cleanup(6, Number, Symbol, Qexpr, Sexpr, Expr, Alisp);

    return 0;
}

