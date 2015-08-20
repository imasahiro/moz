#include "mozvm.h"
#include "ast.h"
#include "loader.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>

static void usage(const char *arg)
{
    fprintf(stderr, "Usage: %s -p <bytecode_file> -i <input_file>\n", arg);
}

int main(int argc, char *const argv[])
{
    int parsed;
    mozvm_loader_t L = {};
    moz_inst_t *inst;

    const char *syntax_file = NULL;
    const char *input_file = NULL;
    unsigned tmp, loop = 1;
    unsigned print_stats = 0;
    int opt;

    while ((opt = getopt(argc, argv, "sn:p:i:h")) != -1) {
        switch (opt) {
        case 'n':
            tmp = atoi(optarg);
            loop = tmp > loop ? tmp : loop;
            syntax_file = optarg;
            break;
        case 's':
            print_stats = 1;
            break;
        case 'p':
            syntax_file = optarg;
            break;
        case 'i':
            input_file = optarg;
            break;
        case 'h':
        default: /* '?' */
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    if (syntax_file == NULL) {
        fprintf(stderr, "error: please specify bytecode file\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (input_file == NULL) {
        fprintf(stderr, "error: please specify input file\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (!mozvm_loader_load_input(&L, input_file)) {
        fprintf(stderr, "error: failed to load input_file='%s'\n", input_file);
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    inst = mozvm_loader_load_file(&L, syntax_file);
    assert(inst != NULL);
    while (loop-- > 0) {
        Node node;
        moz_runtime_reset(L.R);
        parsed = moz_runtime_parse(L.R, L.input, L.input + L.input_size, inst);
        node = ast_get_parsed_node(L.R->ast);
        Node_print(node);
    }
    moz_runtime_dispose(L.R);
    mozvm_loader_dispose(&L);
    return 0;
}
