#include "mozvm.h"
#include "loader.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include "nez_moz.h"
#include "compiler/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

static void usage(const char *arg)
{
    fprintf(stderr, "Usage: %s -i <input_file> -o <output_file>\n", arg);
}

int main(int argc, char *const argv[])
{
    long parsed;
    mozvm_loader_t L;
    moz_inst_t *inst;

    const char *input_file = NULL;
    const char *output_file = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "i:o:h")) != -1) {
        switch (opt) {
        case 'i':
            input_file = optarg;
            break;
        case 'o':
            output_file = optarg;
            break;
        case 'h':
        default: /* '?' */
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    if (output_file == NULL) {
        fprintf(stderr, "error: please specify output file name\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (input_file == NULL) {
        fprintf(stderr, "error: please specify input file\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (!mozvm_loader_load_input_file(&L, input_file)) {
        fprintf(stderr, "error: failed to load input_file='%s'\n", input_file);
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    inst = mozvm_loader_load_syntax(&L, syntax_bytecode,
            sizeof(syntax_bytecode), 1);
    assert(inst != NULL);

    NodeManager_init();

    moz_runtime_set_source(L.R, L.input, L.input + L.input_size);
    inst = moz_runtime_parse_init(L.R, L.input, inst);
    parsed = moz_runtime_parse(L.R, L.input, inst);
    if (parsed == 0) {
        Node *node = ast_get_parsed_node(L.R->ast);
        if (node) {
            moz_compiler_compile(output_file, L.R, node);
            NODE_GC_RELEASE(node);
        }
    }
    else {
        fprintf(stderr, "parse error\n");
    }

    moz_runtime_dispose(L.R);
    mozvm_loader_dispose(&L);
    NodeManager_dispose();
    return 0;
}

#ifdef __cplusplus
}
#endif
