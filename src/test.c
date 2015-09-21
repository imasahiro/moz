#include "karray.h"
#include "pstring.h"
#include "input_source.h"

typedef struct Example {
    const char *hash;
    const char *text;
} Example;

DEF_ARRAY_STRUCT0(Example, unsigned);
DEF_ARRAY_T(Example);
DEF_ARRAY_OP(Example);

static int load_tests(ARRAY(Example) *examples, const char *test_file)
{
    size_t input_len = 0;
    Example e;
    const char *p;
    const char *input = load_file(test_file, &input_len, 0);
    const char *end   = input + input_len;
    char *next;
    // [format]
    // L1: hash
    // L2: text_0
    // L3: text_1
    // ...

    p = input;
    next = (char *)strchr(p, '\n');
    e.hash = pstring_alloc(p, next - p);
    p = next + 1;
    e.text = pstring_alloc(p, end - p);
    ARRAY_add(Example, examples, &e);
    return 1;
}

static mozvm_nterm_entry_t *get_production(mozvm_loader_t *L)
{
    return &L->R->nterm_entry[0];
}
#define PARSE_ERROR(file, hash) \
    fprintf(stderr, "parse error. %s %s\n", file, hash);

#define PASS(file, hash) \
    fprintf(stderr, "pass. %s %s\n", file, hash);

#define INVALID(file, hash, hash2) \
    fprintf(stderr, "invalid. %s %s %s\n", file, hash, hash2);

int mozvm_run_test(mozvm_loader_t *L, const char *test_file)
{
    Example *x, *e;
    ARRAY(Example) examples;
    ARRAY_init(Example, &examples, 1);

    // unsigned i;
    // for (i = 0; i < L->R->C.nterm_size; i++) {
    //     fprintf(stderr, "nterm%d %s\n", i, L->R->C.nterms[i]);
    // }

    if (load_tests(&examples, test_file)) {
        FOR_EACH_ARRAY(examples, x, e) {
            const char *begin = x->text;
            const char *end   = begin + pstring_length(begin);
            mozvm_nterm_entry_t *e = get_production(L);

            moz_runtime_set_source(L->R, begin, end);
            moz_runtime_parse_init(L->R, x->text, NULL);
            if (moz_runtime_parse(L->R, x->text, e->begin) == 1) {
                PARSE_ERROR(test_file, x->hash);
            }
            else {
                Node *node = ast_get_parsed_node(L->R->ast);
                if (node) {
                    unsigned char buf[33] = {};
                    // Node_print(node, L->R->C.tags);
                    Node_digest(node, L->R->C.tags, buf);
                    unsigned hlen = pstring_length(x->hash);
                    if (strncmp((char *)buf, x->hash, hlen) == 0) {
                        PASS(test_file, x->hash);
                    }
                    else {
                        INVALID(test_file, x->hash, buf);
                    }
                }
            }
            moz_runtime_reset1(L->R);
            NodeManager_reset();
            moz_runtime_reset2(L->R);
        }
    }

    FOR_EACH_ARRAY(examples, x, e) {
        // fprintf(stderr, "'%s' '%s' '%s'\n", x->hash, x->text);
        pstring_delete(x->hash);
        pstring_delete(x->text);
    }
    ARRAY_dispose(Example, &examples);
    return 0;
}
