#include "compiler.h"
#include "expression.h"
#include "core/pstring.h"
#include <assert.h>

typedef unsigned attribute_t;
// #define DECL_ATTRIBUTE_PUBLIC (1 << 0)

typedef struct compiler {
    moz_runtime_t *R;
    ARRAY(decl_ptr_t) decls;
} moz_compiler_t;

static inline int tag_equal(Node *node, const char *tag)
{
    unsigned len;
    if (node->tag == NULL) {
        return tag == NULL;
    }
    if (tag == NULL) {
        return 0;
    }
    len = pstring_length(node->tag);
    return len == strlen(tag) && strncmp(node->tag, tag, len) == 0;
}

/* decl */
static decl_t *decl_new()
{
    decl_t *decl = VM_CALLOC(1, sizeof(*decl));
    return decl;
}

static void decl_set_name(decl_t *decl, Node *node)
{
    assert(tag_equal(node, "Name") || tag_equal(node, "String"));
    decl->name.str = node->pos;
    decl->name.len = node->len;
}
static void name_set(name_t *name, Node *node)
{
    assert(tag_equal(node, "Name"));
    name->str = node->pos;
    name->len = node->len;
}

/* compiler */
static void compile_comment(moz_compiler_t *C, Node *node)
{
    // Node_print(node, C->R->C.tags);
}

static void compile_format(moz_compiler_t *C, Node *node)
{
    // Node_print(node, C->R->C.tags);
}

static expr_t *compile_expression(moz_compiler_t *C, Node *node);

static inline expr_t *_EXPR_ALLOC(size_t size, expr_type_t type)
{
    expr_t *e = (expr_t *)VM_CALLOC(1, size);
    e->refc = 0;
    e->type = type;
    return e;
}

#define EXPR_ALLOC(T) ((T##_t *) _EXPR_ALLOC(sizeof(T##_t), T))

static void _compile_List(moz_compiler_t *C, Node *node, ARRAY(expr_ptr_t) *list)
{
    unsigned i, len = Node_length(node);
    ARRAY_init(expr_ptr_t, list, len);
    for (i = 0; i < len; i++) {
        Node *child = Node_get(node, i);
        expr_t *expr = compile_expression(C, child);
        ARRAY_add(expr_ptr_t, list, expr);
    }
}

static expr_t *compile_Invoke(moz_compiler_t *C, Node *node)
{
    Invoke_t *e = EXPR_ALLOC(Invoke);
    e->v.name.str = node->pos;
    e->v.name.len = node->len;
    return (expr_t *)e;
}
static expr_t *compile_Any(moz_compiler_t *C, Node *node)
{
    Any_t *e = EXPR_ALLOC(Any);
    return (expr_t *)e;
}
static expr_t *compile_Str(moz_compiler_t *C, Node *node)
{
    unsigned i, len = node->len;
    Str_t *e = EXPR_ALLOC(Str);
    assert(node->len > 0);
    ARRAY_init(uint8_t, &e->list, len);
    for (i = 0; i < len; i++) {
        ARRAY_add(uint8_t, &e->list, node->pos[i]);
    }
    return (expr_t *)e;
}
static expr_t *compile_Byte(moz_compiler_t *C, Node *node)
{
    assert(Node_length(node) == 0);
    if (node->len == 1) {
        Byte_t *byte = EXPR_ALLOC(Byte);
        byte->byte = node->pos[0];
        return &byte->base;
    }
    else {
        return compile_Str(C, node);
    }
}

static unsigned to_hex(uint8_t c)
{
    if(c >= '0' && c <= '9') return c - '0';
    else if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    else if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static unsigned parseUnicode4(const uint8_t *p)
{
    unsigned data = 0;
    data  = to_hex(*p++) * 4096;
    data += to_hex(*p++) *  256;
    data += to_hex(*p++) *   16;
    data += to_hex(*p++) *    1;
    return data;
}

static unsigned parseClass(const uint8_t *p)
{
    uint8_t c = *p++;
    unsigned data = 0;
    if (c == '\\') {
        c = *p++;
        switch (c) {
        case '-':
            return (unsigned) '-';
        case ']':
            return (unsigned) ']';
        case '\\':
            return (unsigned) '\\';
        case 'b':
            return (unsigned) '\b';
        case 'f':
            return (unsigned) '\f';
        case 'v':
            return (unsigned) '\v';
        case 'n':
            return (unsigned) '\n';
        case 'r':
            return (unsigned) '\r';
        case 't':
            return (unsigned) '\t';
        case 'x':
            c = to_hex(*p++);
            c = c << 8 | to_hex(*p);
            return (unsigned) c;
        case 'u':
            data = parseUnicode4(p);
            assert(0 && "TODO");
            return data;
            break;
        }
    }
    return (unsigned) c;
}

static char *write_char(char *p, unsigned char ch)
{
    switch (ch) {
    case '\\':
        *p++ = '\\';
        break;
    case '\b':
        *p++ = '\\';
        *p++ = 'b';
        break;
    case '\f':
        *p++ = '\\';
        *p++ = 'f';
        break;
    case '\n':
        *p++ = '\\';
        *p++ = 'n';
        break;
    case '\r':
        *p++ = '\\';
        *p++ = 'r';
        break;
    case '\t':
        *p++ = '\\';
        *p++ = 't';
        break;
    default:
        if (32 <= ch && ch <= 126) {
            *p++ = ch;
        }
        else {
            *p++ = '\\';
            *p++ = "0123456789abcdef"[ch / 16];
            *p++ = "0123456789abcdef"[ch % 16];
        }
    }
    return p;
}

static void dump_set(bitset_t *set, char *buf)
{
    unsigned i, j;
    *buf++ = '[';
    for (i = 0; i < 256; i++) {
        if (bitset_get(set, i)) {
            buf = write_char(buf, i);
            for (j = i + 1; j < 256; j++) {
                if (!bitset_get(set, j)) {
                    if (j == i + 1) {}
                    else {
                        *buf++ = '-';
                        buf = write_char(buf, j - 1);
                        i = j - 1;
                    }
                    break;
                }
            }
            if (j == 256) {
                *buf++ = '-';
                buf = write_char(buf, j - 1);
                break;
            }
        }
    }
    *buf++ = ']';
    *buf++ = '\0';
}

static expr_t *compile_Set(moz_compiler_t *C, Node *node)
{
    unsigned i, len = Node_length(node);
    Set_t *e = EXPR_ALLOC(Set);
    bitset_init(&e->set);
    for (i = 0; i < len; i++) {
        Node *child = Node_get(node, i);
        if (tag_equal(child, "Class")) {
            unsigned ch = parseClass((const uint8_t *)child->pos);
            bitset_set(&e->set, ch);
        }
        else if (tag_equal(child, "List")) {
            assert(Node_length(child) == 2);
            Node *left = Node_get(child, 0);
            Node *right = Node_get(child, 1);
            unsigned ch1 = parseClass((const uint8_t *)left->pos);
            unsigned ch2 = parseClass((const uint8_t *)right->pos);
            if (ch1 > ch2) {
                unsigned tmp = ch1;
                ch1 = ch2;
                ch2 = tmp;
            }
            for (; ch1 <= ch2; ch1++) {
                bitset_set(&e->set, ch1);
            }
        }
        else {
            assert(0 && "unreachable");
        }
    }
    char buf[1024] = {};
    dump_set(&e->set, buf);
    fprintf(stderr, "%s\n", buf);
    return (expr_t *)e;
}
static expr_t *compile_And(moz_compiler_t *C, Node *node)
{
    And_t *e = EXPR_ALLOC(And);
    assert(Node_length(node) == 1);
    e->expr = compile_expression(C, Node_get(node, 0));
    return (expr_t *)e;
}
static expr_t *compile_Choice(moz_compiler_t *C, Node *node)
{
    Choice_t *e = EXPR_ALLOC(Choice);
    _compile_List(C, node, &e->list);
    return (expr_t *)e;
}
static expr_t *compile_Empty(moz_compiler_t *C, Node *node)
{
    Empty_t *e = EXPR_ALLOC(Empty);
    assert(0 && "TODO");
    return (expr_t *)e;
}
static expr_t *compile_Fail(moz_compiler_t *C, Node *node)
{
    Fail_t *e = EXPR_ALLOC(Fail);
    assert(0 && "TODO");
    return (expr_t *)e;
}
static expr_t *compile_Not(moz_compiler_t *C, Node *node)
{
    Not_t *e = EXPR_ALLOC(Not);
    assert(Node_length(node) == 1);
    e->expr = compile_expression(C, Node_get(node, 0));
    return (expr_t *)e;
}
static expr_t *compile_Option(moz_compiler_t *C, Node *node)
{
    Option_t *e = EXPR_ALLOC(Option);
    assert(Node_length(node) == 1);
    e->expr = compile_expression(C, Node_get(node, 0));
    return (expr_t *)e;
}
static expr_t *compile_Sequence(moz_compiler_t *C, Node *node)
{
    Sequence_t *e = EXPR_ALLOC(Sequence);
    _compile_List(C, node, &e->list);
    return (expr_t *)e;
}
static expr_t *compile_Repetition(moz_compiler_t *C, Node *node)
{
    Repetition_t *e = EXPR_ALLOC(Repetition);
    _compile_List(C, node, &e->list);
    return (expr_t *)e;
}
static expr_t *compile_Repetition1(moz_compiler_t *C, Node *node)
{
    Repetition1_t *e = EXPR_ALLOC(Repetition1);
    _compile_List(C, node, &e->list);
    return (expr_t *)e;
}
static expr_t *compile_Tcapture(moz_compiler_t *C, Node *node)
{
    Tcapture_t *e = EXPR_ALLOC(Tcapture);
    assert(0 && "TODO");
    return (expr_t *)e;
}
static expr_t *compile_Tdetree(moz_compiler_t *C, Node *node)
{
    Tdetree_t *e = EXPR_ALLOC(Tdetree);
    assert(0 && "TODO");
    return (expr_t *)e;
}
static expr_t *compile_Tlfold(moz_compiler_t *C, Node *node)
{
    Tlfold_t *e = EXPR_ALLOC(Tlfold);
    assert(0 && "TODO");
    return (expr_t *)e;
}
static expr_t *compile_Tlink(moz_compiler_t *C, Node *node)
{
    Tlink_t *e = EXPR_ALLOC(Tlink);
    assert(0 && "TODO");
    return (expr_t *)e;
}
static expr_t *compile_Tnew(moz_compiler_t *C, Node *node)
{
    Tnew_t *e = EXPR_ALLOC(Tnew);
    e->expr = compile_expression(C, Node_get(node, 0));
    return (expr_t *)e;
}
static expr_t *compile_Treplace(moz_compiler_t *C, Node *node)
{
    Treplace_t *e = EXPR_ALLOC(Treplace);
    assert(0 && "TODO");
    return (expr_t *)e;
}
static expr_t *compile_Ttag(moz_compiler_t *C, Node *node)
{
    Ttag_t *e = EXPR_ALLOC(Ttag);
    e->name.str = node->pos;
    e->name.len = node->len;
    return (expr_t *)e;
}
static expr_t *compile_Xblock(moz_compiler_t *C, Node *node)
{
    Xblock_t *e = EXPR_ALLOC(Xblock);
    e->expr = compile_expression(C, Node_get(node, 0));
    return (expr_t *)e;
}
static expr_t *compile_Xexists(moz_compiler_t *C, Node *node)
{
    Xexists_t *e = EXPR_ALLOC(Xexists);
    name_set(&e->name, Node_get(node, 0));
    return (expr_t *)e;
}
static expr_t *compile_Xif(moz_compiler_t *C, Node *node)
{
    Xif_t *e = EXPR_ALLOC(Xif);
    assert(0 && "TODO");
    return (expr_t *)e;
}
static expr_t *compile_Xis(moz_compiler_t *C, Node *node)
{
    Xis_t *e = EXPR_ALLOC(Xis);
    name_set(&e->name, Node_get(node, 0));
    return (expr_t *)e;
}
static expr_t *compile_Xisa(moz_compiler_t *C, Node *node)
{
    Xisa_t *e = EXPR_ALLOC(Xisa);
    name_set(&e->name, Node_get(node, 0));
    return (expr_t *)e;
}
static expr_t *compile_Xlocal(moz_compiler_t *C, Node *node)
{
    Xlocal_t *e = EXPR_ALLOC(Xlocal);
    name_set(&e->name, Node_get(node, 0));
    e->expr = compile_expression(C, Node_get(node, 1));
    return (expr_t *)e;
}
static expr_t *compile_Xmatch(moz_compiler_t *C, Node *node)
{
    Xmatch_t *e = EXPR_ALLOC(Xmatch);
    assert(0 && "TODO");
    return (expr_t *)e;
}
static expr_t *compile_Xon(moz_compiler_t *C, Node *node)
{
    Xon_t *e = EXPR_ALLOC(Xon);
    assert(0 && "TODO");
    return (expr_t *)e;
}
static expr_t *compile_Xsymbol(moz_compiler_t *C, Node *node)
{
    Xsymbol_t *e = EXPR_ALLOC(Xsymbol);
    Node *name = Node_get(node, 0);
    Node *expr = Node_get(node, 1);
    name_set(&e->name, name);
    e->expr = compile_expression(C, expr);
    return (expr_t *)e;
}

static expr_t *compile_expression(moz_compiler_t *C, Node *node)
{
    if (tag_equal(node, "NonTerminal")) {
        return compile_Invoke(C, node);
    }
    if (tag_equal(node, "Any")) {
        return compile_Any(C, node);
    }
    if (tag_equal(node, "Character")) {
        return compile_Byte(C, node);
    }
    if (tag_equal(node, "Str")) {
        return compile_Str(C, node);
    }
    if (tag_equal(node, "Class")) {
        return compile_Set(C, node);
    }
    if (tag_equal(node, "And")) {
        return compile_And(C, node);
    }
    if (tag_equal(node, "Choice")) {
        return compile_Choice(C, node);
    }
    if (tag_equal(node, "Empty")) {
        return compile_Empty(C, node);
    }
    if (tag_equal(node, "Fail")) {
        return compile_Fail(C, node);
    }
    if (tag_equal(node, "Not")) {
        return compile_Not(C, node);
    }
    if (tag_equal(node, "Option")) {
        return compile_Option(C, node);
    }
    if (tag_equal(node, "Sequence")) {
        return compile_Sequence(C, node);
    }
    if (tag_equal(node, "Repetition")) {
        return compile_Repetition(C, node);
    }
    if (tag_equal(node, "Repetition1")) {
        return compile_Repetition1(C, node);
    }
    if (tag_equal(node, "Tcapture")) {
        return compile_Tcapture(C, node);
    }
    if (tag_equal(node, "Tdetree")) {
        return compile_Tdetree(C, node);
    }
    if (tag_equal(node, "Tlfold")) {
        return compile_Tlfold(C, node);
    }
    if (tag_equal(node, "Link")) {
        return compile_Tlink(C, node);
    }
    if (tag_equal(node, "New")) {
        return compile_Tnew(C, node);
    }
    if (tag_equal(node, "Treplace")) {
        return compile_Treplace(C, node);
    }
    if (tag_equal(node, "Tagging")) {
        return compile_Ttag(C, node);
    }
    if (tag_equal(node, "Block")) {
        return compile_Xblock(C, node);
    }
    if (tag_equal(node, "Exists")) {
        return compile_Xexists(C, node);
    }
    if (tag_equal(node, "If")) {
        return compile_Xif(C, node);
    }
    if (tag_equal(node, "Is")) {
        return compile_Xis(C, node);
    }
    if (tag_equal(node, "Isa")) {
        return compile_Xisa(C, node);
    }
    if (tag_equal(node, "Local")) {
        return compile_Xlocal(C, node);
    }
    if (tag_equal(node, "Xmatch")) {
        return compile_Xmatch(C, node);
    }
    if (tag_equal(node, "Xon")) {
        return compile_Xon(C, node);
    }
    if (tag_equal(node, "Def")) {
        return compile_Xsymbol(C, node);
    }
    if (tag_equal(node, "Sym")) {
        return compile_Xsymbol(C, node);
    }
    Node_print(node, C->R->C.tags);
    assert(0 && "unreachable");
}

static void compile_production(moz_compiler_t *C, Node *node)
{
    decl_t *decl = decl_new();
    assert(Node_length(node) == 3);
    decl_set_name(decl, Node_get(node, 1));
    fprintf(stderr, "%.*s\n", decl->name.len, decl->name.str);
    Node_print(node, C->R->C.tags);
    decl->body = compile_expression(C, Node_get(node, 2));
    ARRAY_add(decl_ptr_t, &C->decls, decl);
}

void moz_compiler_compile(const char *output_file, moz_runtime_t *R, Node *node)
{
    unsigned i, decl_size = Node_length(node);
    moz_compiler_t C;
    C.R = R;
    ARRAY_init(decl_ptr_t, &C.decls, decl_size);

    for (i = 0; i < decl_size; i++) {
        Node *decl = Node_get(node, i);
        if (tag_equal(decl, "Comment")) {
            compile_comment(&C, decl);
        }
        if (tag_equal(decl, "Production")) {
            compile_production(&C, decl);
        }
        if (tag_equal(decl, "Format")) {
            compile_format(&C, decl);
        }
        // compile_decl(decl);
        // Node_print(decl, R->C.tags);
    }
}
