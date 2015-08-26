#include "mozvm.h"
#include "ast.h"
#include "loader.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>

static void usage(const char *arg)
{
    fprintf(stderr, "Usage: %s -p <bytecode_file> -i <input_file>\n", arg);
}

static struct timeval g_timer;
static void reset_timer()
{
    gettimeofday(&g_timer, NULL);
}

static inline void _show_timer(const char *s, size_t bufsz)
{
    struct timeval endtime;
    gettimeofday(&endtime, NULL);
    double sec = (endtime.tv_sec - g_timer.tv_sec)
        + (double)(endtime.tv_usec - g_timer.tv_usec) / 1000 / 1000;
    fprintf(stderr, "%20s: %3.4f ms  ", s, sec * 1000.0);
    // printf("%20s: %f MB\n", s, ((double)bufsz)/1024/1024);
    fprintf(stderr, "%f Mbps\n", ((double)bufsz)*8/sec/1000/1000);
}

#if 0
static void show_timer(const char *s)
{
    struct timeval endtime;
    gettimeofday(&endtime, NULL);
    double sec = (endtime.tv_sec - g_timer.tv_sec)
        + (double)(endtime.tv_usec - g_timer.tv_usec) / 1000 / 1000;
    printf("%20s: %f sec\n", s, sec);
}
#endif

int main(int argc, char *const argv[])
{
    long parsed;
    mozvm_loader_t L = {};
    moz_inst_t *inst;

    const char *syntax_file = NULL;
    const char *input_file = NULL;
    unsigned tmp, loop = 1;
    unsigned print_stats = 0;
    unsigned quiet_mode = 0;
    int opt;

    while ((opt = getopt(argc, argv, "qsn:p:i:h")) != -1) {
        switch (opt) {
        case 'n':
            tmp = atoi(optarg);
            loop = tmp > loop ? tmp : loop;
            break;
        case 'q':
            quiet_mode = 1;
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
#if defined(MOZVM_PROFILE) && defined(MOZVM_MEMORY_PROFILE)
        mozvm_mm_snapshot(MOZVM_MM_PROF_EVENT_INPUT_LOAD);
#endif
    NodeManager_init();
    inst = mozvm_loader_load_file(&L, syntax_file);
    assert(inst != NULL);

    while (loop-- > 0) {
        Node node = NULL;
        reset_timer();
        moz_runtime_set_source(L.R, L.input, L.input + L.input_size);
#if defined(MOZVM_PROFILE) && defined(MOZVM_MEMORY_PROFILE)
        mozvm_mm_snapshot(MOZVM_MM_PROF_EVENT_PARSE_START);
#endif
        parsed = moz_runtime_parse(L.R, L.input, inst);
        if (parsed != 0) {
            fprintf(stderr, "parse error\n");
            break;
        }
        node = ast_get_parsed_node(L.R->ast);
#if defined(MOZVM_PROFILE) && defined(MOZVM_MEMORY_PROFILE)
        mozvm_mm_snapshot(MOZVM_MM_PROF_EVENT_PARSE_END);
#endif
        if (node) {
            if (!quiet_mode) {
#ifdef NODE_USE_NODE_PRINT
                Node_print(node);
#endif
            }
            NODE_GC_RELEASE(node);
        }
        if (print_stats) {
            _show_timer(syntax_file, L.input_size);
        }
#if defined(MOZVM_PROFILE) && defined(MOZVM_MEMORY_PROFILE)
        mozvm_mm_snapshot(MOZVM_MM_PROF_EVENT_GC_EXECUTED);
#endif
        moz_runtime_reset(L.R);
        NodeManager_reset();
    }
    if (print_stats) {
#if defined(MOZVM_PROFILE) && defined(MOZVM_MEMORY_PROFILE)
        mozvm_mm_print_stats();
#endif
        moz_loader_print_stats(&L);
        NodeManager_print_stats();
        memo_print_stats();
        moz_runtime_print_stats(L.R);
    }
    moz_runtime_dispose(L.R);
    mozvm_loader_dispose(&L);
    NodeManager_dispose();
    return 0;
}
