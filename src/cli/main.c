#include "mozvm.h"
#include "loader.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>

#include "test.c"

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
    fprintf(stderr, "%f MB ",   ((double)bufsz)/1024/1024);
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
    mozvm_loader_t L;
    moz_inst_t *head, *inst;

    const char *syntax_file = NULL;
    const char *input_file = NULL;
    unsigned tmp, loop = 1;
    unsigned print_stats = 0;
    unsigned quiet_mode = 0;
#ifdef MOZVM_ENABLE_NODE_DIGEST
    unsigned show_digest = 0;
    unsigned test_mode = 0;
#endif
    int opt;

    while ((opt = getopt(argc, argv,
#ifdef MOZVM_ENABLE_NODE_DIGEST
                    "d"
#ifdef MOZVM_ENABLE_NEZTEST
                    "t"
#endif
#endif
                    "qsn:p:i:h")) != -1) {
        switch (opt) {
        case 'n':
            tmp = atoi(optarg);
            loop = tmp > loop ? tmp : loop;
            break;
#ifdef MOZVM_ENABLE_NODE_DIGEST
        case 'd':
            show_digest = 1;
            break;
        case 't':
            test_mode = 1;
            break;
#endif
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
    if (test_mode == 0 && input_file == NULL) {
        fprintf(stderr, "error: please specify input file\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (test_mode == 0 && !mozvm_loader_load_input_file(&L, input_file)) {
        fprintf(stderr, "error: failed to load input_file='%s'\n", input_file);
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
#if defined(MOZVM_PROFILE) && defined(MOZVM_MEMORY_PROFILE)
        mozvm_mm_snapshot(MOZVM_MM_PROF_EVENT_INPUT_LOAD);
#endif
    head = inst = mozvm_loader_load_syntax_file(&L, syntax_file, 1);
    assert(inst != NULL);

    if (test_mode) {
        mozvm_run_test(&L, input_file);
        goto L_exit;
    }

    NodeManager_init();
    while (loop-- > 0) {
        Node *node = NULL;
        reset_timer();
        inst = head;
        moz_runtime_set_source(L.R, L.input, L.input + L.input_size);
#if defined(MOZVM_PROFILE) && defined(MOZVM_MEMORY_PROFILE)
        mozvm_mm_snapshot(MOZVM_MM_PROF_EVENT_PARSE_START);
#endif
        inst = moz_runtime_parse_init(L.R, L.input, inst);
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
                Node_print(node, L.R->C.tags);
#endif
            }
#ifdef MOZVM_ENABLE_NODE_DIGEST
            if (show_digest) {
                unsigned char buf[32] = {};
                Node_digest(node, L.R->C.tags, buf);
                fprintf(stderr, "%.*s\n", 32, buf);
            }
#endif
            NODE_GC_RELEASE(node);
        }
        if (print_stats) {
            _show_timer(input_file, L.input_size);
        }
#if defined(MOZVM_PROFILE) && defined(MOZVM_MEMORY_PROFILE)
        mozvm_mm_snapshot(MOZVM_MM_PROF_EVENT_GC_EXECUTED);
#endif
        moz_runtime_reset1(L.R);
        NodeManager_reset();
        moz_runtime_reset2(L.R);
    }
    if (print_stats) {
#if defined(MOZVM_PROFILE) && defined(MOZVM_MEMORY_PROFILE)
        mozvm_mm_print_stats();
#endif
        moz_loader_print_stats(&L);
        NodeManager_print_stats();
        memo_print_stats();
        symtable_print_stats();
        moz_vm_print_stats(L.R);
    }
L_exit:
    moz_runtime_dispose(L.R);
    mozvm_loader_dispose(&L);
    NodeManager_dispose();
    return 0;
}
