#include <sys/time.h> // gettimeofday

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
    }

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


int main(int argc, char* const argv[])
{
    uint64_t start, end, latency;
    struct ParsingContext lctx, *ctx;
    ctx = ParsingContext_init(&lctx, CNEZ_FLAG_TABLE_SIZE, CNEZ_MEMO_SIZE, argv[1]);
    for (int i = 0; i < 1; i++) {
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
        if (CNEZ_ENABLE_AST_CONSTRUCTION) {
            Node *node = node = ast_get_parsed_node(ctx->ast);
            Node_print(node);
        }
        fprintf(stderr, "ErapsedTime: %llu msec\n", end - start);
        if(i == 0) {
            latency = end - start;
        }
        else if(latency > end - start) {
            latency = end - start;
        }
    }
    return 0;
}
