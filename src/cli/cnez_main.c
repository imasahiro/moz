#include <sys/time.h> // gettimeofday
#include <unistd.h>
#include <getopt.h>

static uint64_t timer()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static char *load_file(const char *path, size_t *size)
{
    size_t len;
    size_t readed;
    char *data;
    FILE *fp = fopen(path, "rb");
    assert(fp != 0);

    fseek(fp, 0, SEEK_END);
    len = (size_t) ftell(fp);
    fseek(fp, 0, SEEK_SET);
    data = (char *) VM_CALLOC(1, len + 1);
    readed = fread(data, 1, len, fp);
    assert(len == readed);
    fclose(fp);
    *size = len;
    return data;
    (void)readed;
}


static ParsingContext ParsingContext_init(ParsingContext ctx, unsigned flag_size, unsigned memo_size, const char *input)
{
    size_t input_size = 0;
    ctx->input = ctx->cur = load_file(input, &input_size);
    ctx->memo  = memo_init(MOZ_MEMO_DEFAULT_WINDOW_SIZE, memo_size);
    ctx->ast   = AstMachine_init(128, ctx->input);
    ctx->table = symtable_init();
    ctx->input_size = input_size;
    ctx->flags_size = flag_size;
    if (ctx->flags_size) {
        ctx->flags = (int *) malloc(sizeof(int) * flag_size);
        memset(ctx->flags, 0, sizeof(int) * flag_size);
    }

    NodeManager_init();
    return ctx;
}

static void ParsingContext_dispose(ParsingContext ctx)
{
    AstMachine_dispose(ctx->ast);
    symtable_dispose(ctx->table);
    free(ctx->input);
    if (ctx->flags_size) {
        free(ctx->flags);
    }
}

static void ParsingContext_reset(ParsingContext ctx, unsigned flag_size, unsigned memo_size)
{
    AstMachine_dispose(ctx->ast);
    symtable_dispose(ctx->table);
    memo_dispose(ctx->memo);
    NodeManager_reset();
    ctx->memo  = memo_init(MOZ_MEMO_DEFAULT_WINDOW_SIZE, memo_size);
    ctx->ast   = AstMachine_init(128, ctx->input);
    ctx->table = symtable_init();
    memset(ctx->flags, 0, sizeof(int) * flag_size);
}

static void usage(const char *arg)
{
    fprintf(stderr, "Usage: %s [-s] [-n <loop_count>] [-q] -i <input_file>\n", arg);
}

int main(int argc, char* const argv[])
{
    uint64_t start, end, latency;
    struct ParsingContext lctx, *ctx;

    const char *input_file = NULL;
    unsigned i, tmp, loop = 1;
    unsigned print_stats = 0;
    unsigned quiet_mode = 0;
    unsigned show_digest = 0;
    int opt;

    while ((opt = getopt(argc, argv,
#ifdef MOZVM_ENABLE_NODE_DIGEST
                    "d"
#endif
                    "qsn:i:h")) != -1) {
        switch (opt) {
        case 'n':
            tmp = atoi(optarg);
            loop = tmp > loop ? tmp : loop;
            break;
#ifdef MOZVM_ENABLE_NODE_DIGEST
        case 'd':
            show_digest = 1;
            break;
#endif
        case 'q':
            quiet_mode = 1;
            break;
        case 's':
            print_stats = 1;
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

    if (input_file == NULL) {
        fprintf(stderr, "error: please specify input file\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    ctx = ParsingContext_init(&lctx, CNEZ_FLAG_TABLE_SIZE, CNEZ_MEMO_SIZE, input_file);
    for (i = 0; i < loop; i++) {
        ctx->cur = ctx->input;
        start = timer();
        if(pFile(ctx)) {
            fprintf(stderr, "parse error\n");
            break;
        }
        else if((ctx->cur - ctx->input) != ctx->input_size) {
            fprintf(stderr, "unconsume\n");
            break;
        }
        end = timer();
#ifdef CNEZ_ENABLE_AST_CONSTRUCTION
            Node *node = node = ast_get_parsed_node(ctx->ast);
            if (node) {
                if (!quiet_mode) {
                    Node_print(node, global_tag_list);
                }
#ifdef MOZVM_ENABLE_NODE_DIGEST
                if (show_digest) {
                    unsigned char buf[32] = {};
                    Node_digest(node, global_tag_list, buf);
                    fprintf(stderr, "%.*s\n", 32, buf);
                }
#endif
            }
            NODE_GC_RELEASE(node);
#endif
        ParsingContext_reset(ctx, CNEZ_FLAG_TABLE_SIZE, CNEZ_MEMO_SIZE);
        fprintf(stderr, "ErapsedTime: %llu msec\n", end - start);
        if(i == 0) {
            latency = end - start;
        }
        else if(latency > end - start) {
            latency = end - start;
        }
    }
    ParsingContext_dispose(ctx);
    return 0;
}
