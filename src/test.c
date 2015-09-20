#include "karray.h"
#include "pstring.h"
#include "input_source.h"

typedef struct Example {
    const char *hash;
    const char *name;
    const char *text;
} Example;

DEF_ARRAY_STRUCT0(Example, unsigned);
DEF_ARRAY_T(Example);
DEF_ARRAY_OP(Example);

static int load_tests(ARRAY(Example) *examples, const char *test_file)
{
    size_t input_len = 0;
    Example e = {};
    const char *p;
    const char *input = load_file(test_file, &input_len, 0);
    const char *end   = input + input_len;

    if (strncmp(input, "##neztest", strlen("##neztest")) != 0) {
        VM_FREE((char *)input);
        return 0;
    }

    input += strlen("##neztest\n");

    p = input;
    while (p < end) {
        char *next;
        size_t len;

        // [format]
        // L1: ##neztest
        // L1: ~hash0
        // L2: &name0
        // L3: >text_size
        // L4: text0_0
        // L5: text0_1
        // L6: ~hash1
        // L7: ...

        switch (*p) {
        case '~':
            p++;
            next = strchr(p, '\n');
            e.hash = pstring_alloc(p, next - p);
            p = next + 1;
            break;
        case '&':
            p++;
            next = strchr(p, '\n');
            e.name = pstring_alloc(p, next - p);
            p = next + 1;
            break;
        case '>':
            p++;
            len = strtol(p, &next, 10);
            p = next + 1/*\n*/;
            e.text = pstring_alloc(p, len);
            p += len + 1/*\n*/;
            ARRAY_add(Example, examples, &e);
            memset(&e, 0, sizeof(Example));
            break;
        default:
            return 0;
        }
    }
    return 1;
}

static mozvm_nterm_entry_t *find_production(mozvm_loader_t *L, const char *name)
{
    unsigned i;
    for (i = 0; i < L->R->C.nterm_size; i++) {
        if (pstring_equal(name, L->R->C.nterms[i])) {
            return &L->R->nterm_entry[i];
        }
    }
    return NULL;
}

int mozvm_run_test(mozvm_loader_t *L, const char *test_file)
{
    Example *x, *e;
    ARRAY(Example) examples;
    ARRAY_init(Example, &examples, 0);

    unsigned i;
    for (i = 0; i < L->R->C.nterm_size; i++) {
        fprintf(stderr, "nterm%d %s\n", i, L->R->C.nterms[i]);
    }

    if (load_tests(&examples, test_file)) {
        FOR_EACH_ARRAY(examples, x, e) {
            mozvm_nterm_entry_t *e;
            const char *begin = x->text;
            const char *end   = begin + pstring_length(begin);
            e = find_production(L, x->name);
            if (e == NULL) {
                fprintf(stderr, "no_nterm. '%s' '%s'\n", x->hash, x->name);
                continue;
            }
            moz_runtime_set_source(L->R, begin, end);
            moz_runtime_parse_init(L->R, x->text, NULL);
            if (moz_runtime_parse(L->R, x->text, e->begin) == 0) {
                Node *node = ast_get_parsed_node(L->R->ast);
                if (node) {
                    unsigned char buf[33] = {};
                    Node_print(node, L->R->C.tags);
                    Node_digest(node, L->R->C.tags, buf);
                    unsigned hlen = pstring_length(x->hash);
                    if (strncmp((char *)buf, x->hash, hlen) == 0) {
                        fprintf(stderr, "pass. '%s' '%s'\n", x->hash, x->name);
                    }
                    else {
                        fprintf(stderr, "invalid. '%s' '%s'\n", x->hash, buf);
                    }
                }
                moz_runtime_reset1(L->R);
                NodeManager_reset();
                moz_runtime_reset2(L->R);
            }
            else {
                fprintf(stderr, "parse error. '%s' '%s'\n", x->hash, x->name);
            }
        }
    }

    FOR_EACH_ARRAY(examples, x, e) {
        // fprintf(stderr, "'%s' '%s' '%s'\n", x->hash, x->name, x->text);
        pstring_delete(x->hash);
        pstring_delete(x->name);
        pstring_delete(x->text);
    }
    ARRAY_dispose(Example, &examples);
    return 0;
}
