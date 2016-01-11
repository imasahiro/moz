#include "mozvm.h"
#include "loader.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include "nez_moz.h"
#include "core/input_source.h"
#include "compiler/compiler.h"
#include "compiler/module.h"

#ifdef __cplusplus
extern "C" {
#endif

static void usage(const char *arg)
{
    fprintf(stderr, "Usage: %s -p <peg_file> -i <input_file>\n", arg);
}

static int parse(moz_module_t *M, const char *input_file)
{
    size_t input_size = 0;
    char *input = (char *)load_file(input_file, &input_size, 32);
    int result;
    M->dump(M);
    result = M->parse(M, input, input_size);
    M->dispose(M);
    return result;
}

struct parse_result {
    const char *error;
    int parsed;
};

static int run(const char *peg_file, const char *input_file, struct parse_result *result)
{
    mozvm_loader_t L;
    moz_inst_t *inst;
    Node *node;

    if (input_file == NULL) {
        result->error = "input_file is null. Please specify input file name";
        result->parsed = 0;
        return 1;
    }
    if (peg_file == NULL) {
        result->error = "peg_file is null. Please specify input peg name";
        result->parsed = 0;
        return 1;
    }
    if (!mozvm_loader_load_input_file(&L, peg_file)) {
        result->error = "Failed to load peg file";
        result->parsed = 0;
        return 1;
    }
    inst = mozvm_loader_load_syntax(&L, syntax_bytecode,
            sizeof(syntax_bytecode), 1);
    assert(inst != NULL);

    NodeManager_init();

    moz_runtime_set_source(L.R, L.input, L.input + L.input_size);
    inst = moz_runtime_parse_init(L.R, L.input, inst);
    if (moz_runtime_parse(L.R, L.input, inst) != 0) {
        result->error = "Failed to load peg file";
        result->parsed = 0;
        goto L_finally;
    }
    node = ast_get_parsed_node(L.R->ast);
    if (node == NULL) {
        result->error = "Failed to parse peg file";
        result->parsed = 0;
        goto L_finally;
    }
    moz_module_t *M = moz_compiler_compile(L.R, node);
    NODE_GC_RELEASE(node);
    if (parse(M, input_file) == 0) {
        result->error = "Failed to parse input file";
        result->parsed = 0;
        goto L_finally;
    }

    result->parsed = 1;
L_finally:
    moz_runtime_dispose(L.R);
    mozvm_loader_dispose(&L);
    NodeManager_dispose();
    return result->parsed == 1;
}

int main(int argc, char *const argv[])
{
    const char *peg_file = NULL;
    const char *input_file = NULL;
    struct parse_result result = {};
    int opt;

    while ((opt = getopt(argc, argv, "p:i:h")) != -1) {
        switch (opt) {
        case 'p':
            peg_file = optarg;
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
    if (run(peg_file, input_file, &result) == 0) {
        fprintf(stderr, "Error: %s\n", result.error);
    }
}

#ifdef __cplusplus
}
#endif
