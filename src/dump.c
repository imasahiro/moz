#include <unistd.h>
#include <getopt.h>
#define LOADER_DEBUG 1
#include "loader.c"

static void usage(const char *arg)
{
    fprintf(stderr, "Usage: %s -p <bytecode_file>\n", arg);
}

int main(int argc, char *const argv[])
{
    mozvm_loader_t L = {};

    const char *syntax_file = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "p:h")) != -1) {
        switch (opt) {
        case 'p':
            syntax_file = optarg;
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
    NodeManager_init();
    mozvm_loader_load_file(&L, syntax_file, 1);
    mozvm_loader_dump(&L, 1);
    assert(inst != NULL);
    moz_runtime_dispose(L.R);
    mozvm_loader_dispose(&L);
    NodeManager_dispose();
    return 0;
}
