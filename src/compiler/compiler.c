#include "mozvm.h"
#include "compiler.h"
#include "expression.h"
#include "core/pstring.h"
#include <assert.h>
#define MOZC_USE_AST_INLINING 1
#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned attribute_t;
// #define DECL_ATTRIBUTE_PUBLIC (1 << 0)

typedef pstring_t *pstring_ptr_t;

DEF_ARRAY_STRUCT0(pstring_ptr_t, unsigned);
DEF_ARRAY_T(pstring_ptr_t);
DEF_ARRAY_OP_NOPOINTER(pstring_ptr_t);

typedef struct compiler {
    moz_runtime_t *R;
    ARRAY(decl_ptr_t) decls;
    ARRAY(pstring_ptr_t) strs;
} moz_compiler_t;

static STRING_t moz_compiler_get_string(moz_compiler_t *C, ARRAY(uint8_t) *str)
{
    pstring_t **x;
    unsigned i = 0;
    unsigned maxStrSize = ARRAY_size(C->strs);
    char *s = (char *)ARRAY_BEGIN(*str);
    FOR_EACH_ARRAY_(C->strs, x, i) {
        if ((*x)->len == ARRAY_size(*str)) {
            if (strncmp((*x)->str, s, (*x)->len) == 0) {
                return i;
            }
        }
    }
    const char *newStr = pstring_alloc(s, ARRAY_size(*str));
    ARRAY_add(pstring_ptr_t, &C->strs, pstring_get_raw(newStr));
    return maxStrSize;
}

static BITSET_t moz_compiler_get_set(moz_compiler_t *C, bitset_t *set)
{
    asm volatile("int3");
    return 0;
}

static TAG_t moz_compiler_get_tag(moz_compiler_t *C, name_t *name)
{
    asm volatile("int3");
    return 0;
}

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

static bool isByteOrSet(expr_t *e)
{
    return e->type == Byte || e->type == Set;
}

static bool isPatternMatchOnly(expr_t *e)
{
    expr_t **x, **end;
    switch (e->type) {
    case Empty:
    case Any:
    case Byte:
    case Str:
    case Set:
    case Fail:
        return true;
    case Invoke:
        return false; /* TODO */
    case And:
    case Not:
    case Option:
        return isPatternMatchOnly(((Unary_t *)e)->expr);
    case Choice:
    case Sequence:
    case Repetition:
    case Repetition1:
        FOR_EACH_ARRAY(((List_t *)e)->list, x, end) {
            if (!isPatternMatchOnly(*x)) {
                return false;
            }
        }
        return true;
    default:
        break;
    }
    return false;
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

static uintptr_t _MAX_IR_ID = 0;

static IR_t *_IR_ALLOC(size_t size, ir_type_t type)
{
    IR_t *ir = (IR_t *)VM_CALLOC(1, size);
    ir->id = _MAX_IR_ID++;
    return ir;
}

#define IR_ALLOC(T) ((T##_t *) _IR_ALLOC(sizeof(T##_t), T))

static void _compile_List(moz_compiler_t *C, Node *node, ARRAY(expr_ptr_t) *list)
{
    unsigned i, len = Node_length(node);
    ARRAY_init(expr_ptr_t, list, len);
    for (i = 0; i < len; i++) {
        Node *child = Node_get(node, i);
        expr_t *expr = compile_expression(C, child);
        ARRAY_add(expr_ptr_t, list, expr);
        expr->refc++;
    }
}

static expr_t *compile_Invoke(moz_compiler_t *C, Node *node)
{
    unsigned len;
    Invoke_t *e = EXPR_ALLOC(Invoke);
    decl_t **decl, **end;
    e->name.str = node->pos;
    e->name.len = len = node->len;
    FOR_EACH_ARRAY(C->decls, decl, end) {
        if (len == (*decl)->name.len) {
            if (strncmp(e->name.str, (*decl)->name.str, len) == 0) {
                e->decl = *decl;
                (*decl)->refc++;
            }
        }
    }
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
static expr_t *compile_InvokeOrStr(moz_compiler_t *C, Node *node)
{
    unsigned len = node->len;
    decl_t **decl, **end;
    FOR_EACH_ARRAY(C->decls, decl, end) {
        if (len == (*decl)->name.len) {
            if (strncmp(node->pos, (*decl)->name.str, len) == 0) {
                return compile_Invoke(C, node);
            }
        }
    }
    return compile_Str(C, node);
}
static expr_t *compile_Empty(moz_compiler_t *C, Node *node)
{
    Empty_t *e = EXPR_ALLOC(Empty);
    return (expr_t *)e;
}
static expr_t *compile_Byte(moz_compiler_t *C, Node *node)
{
    assert(Node_length(node) == 0);
    if (node->len == 0) {
        return compile_Empty(C, node);
    }
    else if (node->len == 1 || (node->len == 2 && node->pos[0] == '\\')) {
        Byte_t *byte = EXPR_ALLOC(Byte);
        byte->byte = (uint8_t)node->pos[0];
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
            return parseUnicode4(p);
        }
    }
    return (unsigned) c;
}

static expr_t *compile_Set(moz_compiler_t *C, Node *node)
{
    unsigned i, len = Node_length(node);
    Set_t *e = EXPR_ALLOC(Set);
    bitset_init(&e->set);
    unsigned data[len * 2];
    assert(len > 0);
    memset(data, 0, sizeof(unsigned) * len * 2);
    for (i = 0; i < len; i++) {
        Node *child = Node_get(node, i);
        if (tag_equal(child, "Class")) {
            unsigned ch = parseClass((const uint8_t *)child->pos);
            data[i * 2 + 0] = data[i * 2 + 1] = ch;
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
            data[i * 2 + 0] = ch1;
            data[i * 2 + 1] = ch2;
        }
        else {
            assert(0 && "unreachable");
        }
    }
    for (i = 0; i < len; i++) {
        unsigned ch1 = data[i * 2 + 0];
        unsigned ch2 = data[i * 2 + 1];
        if (ch1 > 256 || ch2 > 256) {
            assert(0 && "unicode char class is not supported");
        }
        for (; ch1 <= ch2; ch1++) {
            bitset_set(&e->set, ch1);
        }
    }
#if DUMP_SET
    char buf[1024] = {};
    dump_set(&e->set, buf);
    fprintf(stderr, "%s\n", buf);
#endif
    return (expr_t *)e;
}
static expr_t *compile_And(moz_compiler_t *C, Node *node)
{
    And_t *e = EXPR_ALLOC(And);
    assert(Node_length(node) == 1);
    e->expr = compile_expression(C, Node_get(node, 0));
    e->expr->refc++;
    return (expr_t *)e;
}
static expr_t *compile_Choice(moz_compiler_t *C, Node *node)
{
    Choice_t *e = EXPR_ALLOC(Choice);
    _compile_List(C, node, &e->list);
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
    e->expr->refc++;
    return (expr_t *)e;
}
static expr_t *compile_Option(moz_compiler_t *C, Node *node)
{
    Option_t *e = EXPR_ALLOC(Option);
    assert(Node_length(node) == 1);
    e->expr = compile_expression(C, Node_get(node, 0));
    e->expr->refc++;
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
    assert(Node_length(node) == 1);
    _compile_List(C, node, &e->list);
    return (expr_t *)e;
}
static expr_t *compile_Repetition1(moz_compiler_t *C, Node *node)
{
    Repetition1_t *e = EXPR_ALLOC(Repetition1);
    assert(Node_length(node) == 1);
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
    Node *child = NULL;
    if (Node_length(node) == 1) {
        e->name.str = NULL;
        e->name.len = 0;
        child = Node_get(node, 0);
    }
    else {
        Node *label = Node_get(node, 0);
        e->name.str = label->pos;
        e->name.len = label->len;
        child = Node_get(node, 1);
    }
    e->expr = compile_expression(C, child);
    return (expr_t *)e;
}
static expr_t *compile_Tnew(moz_compiler_t *C, Node *node)
{
    Tnew_t *e = EXPR_ALLOC(Tnew);
    e->expr = compile_expression(C, Node_get(node, 0));
    e->expr->refc++;
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
    if (tag_equal(node, "String")) {
        return compile_InvokeOrStr(C, node);
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
    if (tag_equal(node, "Link") || tag_equal(node, "label")) {
        return compile_Tlink(C, node);
    }
    if (tag_equal(node, "New")) {
        return compile_Tnew(C, node);
    }
    if (tag_equal(node, "Replace")) {
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
    return NULL;
}

static void compile_production(moz_compiler_t *C, Node *node, decl_t *decl)
{
    decl->body = compile_expression(C, Node_get(node, 2));
    decl->body->refc++;
}

/* dump */
static const char *ast_name[] = {
#define DEFINE_NAME(NAME, DUMP, OPT) #NAME,
    FOR_EACH_BASE_AST(DEFINE_NAME)
    FOR_EACH_EXTRA_AST(DEFINE_NAME)
#undef DFINE_NAME
};

static void moz_expr_dump(int level, expr_t *e);
static void fprint_indent(int level)
{
    while (level-- > 0) {
        fprintf(stdout, "  ");
    }
}

static void moz_Expr_dump(int level, expr_t *e)
{
    fprint_indent(level);
    fprintf(stdout, "%s\n", ast_name[e->type]);
}

static void moz_Name_dump(int level, expr_t *e)
{
    Name_t *expr = (Name_t *)e;
    fprint_indent(level);
    fprintf(stdout, "%s(%.*s)\n", ast_name[e->type],
            expr->name.len, expr->name.str);
}

static void moz_Unary_dump(int level, expr_t *e)
{
    Unary_t *expr = (Unary_t *)e;
    fprint_indent(level);
    fprintf(stdout, "%s[\n", ast_name[e->type]);
    moz_expr_dump(level + 1, expr->expr);
    fprint_indent(level);
    fprintf(stdout, "]\n");
}

static void moz_NameUnary_dump(int level, expr_t *e)
{
    NameUnary_t *expr = (NameUnary_t *)e;
    fprint_indent(level);
    fprintf(stdout, "%s(%.*s) [\n", ast_name[e->type],
            expr->name.len, expr->name.str);
    moz_expr_dump(level + 1, expr->expr);
    fprint_indent(level);
    fprintf(stdout, "]\n");
}

static void moz_List_dump(int level, expr_t *e)
{
    expr_t **x, **end;
    struct List_t *expr = (struct List_t *)e;
    fprint_indent(level);
    fprintf(stdout, "%s [\n", ast_name[e->type]);
    FOR_EACH_ARRAY(expr->list, x, end) {
        moz_expr_dump(level + 1, *x);
    }
    fprint_indent(level);
    fprintf(stdout, "]\n");
}

static void moz_Byte_dump(int level, expr_t *e)
{
    Byte_t *expr = (Byte_t *) e;
    fprint_indent(level);
    fprintf(stdout, "%s('%c')\n", ast_name[e->type], expr->byte);
}

static void moz_Str_dump(int level, expr_t *e)
{
    Str_t *expr = (Str_t *) e;
    fprint_indent(level);
    fprintf(stdout, "%s('%.*s')\n", ast_name[e->type],
            ARRAY_size(expr->list), ARRAY_BEGIN(expr->list));
}

static void moz_Set_dump(int level, expr_t *e)
{
    Set_t *expr = (Set_t *) e;
    char buf[1024];
    dump_set(&expr->set, buf);
    fprint_indent(level);
    fprintf(stdout, "%s(%s)\n", ast_name[e->type], buf);
}

static void moz_decl_dump(int level, decl_t *decl)
{
    fprint_indent(level);
    fprintf(stdout, "decl %.*s (refc=%ld) {\n",
            decl->name.len, decl->name.str,
            decl->refc);
    moz_expr_dump(level + 1, decl->body);
    fprint_indent(level);
    fprintf(stdout, "}\n");
}

typedef void (*f_dump)(int level, expr_t *);
static void moz_expr_dump(int level, expr_t *e)
{
    f_dump dump[] = {
#define F_DUMP_DECL(NAME, DUMP, OPT) moz_##DUMP##_dump,
        FOR_EACH_BASE_AST(F_DUMP_DECL)
        FOR_EACH_EXTRA_AST(F_DUMP_DECL)
#undef  F_DUMP_DECL
    };
    dump[e->type](level, e);
}

static void moz_ast_dump(moz_compiler_t *C)
{
    decl_t **decl, **end;
    fprintf(stderr, "------------\n");
    FOR_EACH_ARRAY(C->decls, decl, end) {
        if ((*decl)->refc > 0) {
            moz_decl_dump(0, *decl);
        }
    }
    fprintf(stderr, "------------\n");
}

/* optimize */
static void moz_expr_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e);

static void moz_Expr_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    /* do nothing */
}

static void moz_NameUnary_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    NameUnary_t *expr = (NameUnary_t *) e;
    moz_expr_optimize(C, e, &expr->expr, expr->expr);
}

static void moz_Unary_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Unary_t *expr = (Unary_t *) e;
    moz_expr_optimize(C, e, &expr->expr, expr->expr);
}

static void _moz_List_optimize(moz_compiler_t *C, expr_t *e)
{
    List_t *expr = (List_t *) e;
    expr_t **x, **end;
    FOR_EACH_ARRAY(expr->list, x, end) {
        moz_expr_optimize(C, e, x, *x);
    }
}

static void moz_ast_do_inline(expr_t **ref, Invoke_t *expr)
{
    decl_t *decl = expr->decl;
    *ref = decl->body;
    expr->base.refc--;
    decl->refc--;
    decl->body->refc++;
}

static void moz_Invoke_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Invoke_t *expr = (Invoke_t *)e;
    if (expr->decl != NULL) {
        decl_t *decl = expr->decl;
        if (MOZC_USE_AST_INLINING &&
                (decl->refc == 1 || isPatternMatchOnly(decl->body))) {
            moz_ast_do_inline(ref, expr);
            return;
        }
        assert(decl->body != NULL);
        switch (decl->body->type) {
        case Empty:
        case Any:
        case Byte:
        case Str:
        case Set:
        case Fail:
            moz_ast_do_inline(ref, expr);
            return;
        default:
            break;
        }
    }
}

static void moz_Not_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Unary_t *expr = (Unary_t *) e;
    moz_expr_optimize(C, e, &expr->expr, expr->expr);
}

static void moz_Option_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Unary_t *expr = (Unary_t *) e;
    moz_expr_optimize(C, e, &expr->expr, expr->expr);
}

static bool applyByteMapOptimization(Choice_t *e)
{
    expr_t **x, **end;
    FOR_EACH_ARRAY(e->list, x, end) {
        expr_type_t type = (*x)->type;
        switch (type) {
        case Byte:
        case Set:
            break;
        case Not:
            if (isByteOrSet(((Not_t *)(*x))->expr)) {
                break;
            }
            /* fallthrough */
        default:
            return false;
        }
    }
    return true;
}

static void put_byte(bitset_t *set, Byte_t *e)
{
    bitset_set(set, e->byte);
}

static void put_set(bitset_t *set, Set_t *e)
{
    bitset_or(set, &e->set);
}

static void moz_Choice_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Choice_t *expr = (Choice_t *) e;
    bitset_t bitset;
    expr_t **x, **end;
    _moz_List_optimize(C, e);

    if (ARRAY_size(expr->list) == 1) {
        *ref = ARRAY_get(expr_ptr_t, &expr->list, 0);
        ARRAY_dispose(expr_ptr_t, &expr->list);
        return;
    }
    if (applyByteMapOptimization(expr)) {
        Set_t *set = EXPR_ALLOC(Set);
        bitset_init(&set->set);
        FOR_EACH_ARRAY(expr->list, x, end) {
            expr_t *child = *x;
            expr_type_t type = child->type;
            bool invert = false;
            bitset_init(&bitset);
            if (type == Not) {
                Not_t *not = (Not_t *)(child);
                assert(0 && "not tested");
                child = not->expr;
                type = child->type;
                invert = true;
            }
            switch (type) {
            case Byte:
                put_byte(&set->set, (Byte_t *)(child));
                break;
            case Set:
                put_set(&set->set, (Set_t *)(child));
                break;
            default:
                break;
            }
            if (invert) {
                bitset_flip(&bitset);
            }
            bitset_or(&set->set, &bitset);
        }
        *ref = (expr_t *)set;
    }
}

static void moz_Sequence_do_flatten(Sequence_t *e, int offset, Sequence_t *seq)
{
    /*
     * expr   = [A, B, seq, C, D]
     * seq    = [X, Y, Z]
     * offset = 2
     * -> expr = [A, B, X, Y, Z, C, D]
     *    seq  = [X, Y, Z]
     */
    expr_t **x, **end;
    int i = 0;
    ARRAY_remove(expr_ptr_t, &e->list, offset);
    FOR_EACH_ARRAY(seq->list, x, end) {
        ARRAY_insert(expr_ptr_t, &e->list, offset + i++, *x);
    }
}

static expr_t *moz_Sequence_concatString(Str_t *s1, Str_t *s2)
{
    Str_t *s3 = EXPR_ALLOC(Str);
    unsigned i;
    ARRAY_init(uint8_t, &s3->list, ARRAY_size(s1->list) + ARRAY_size(s2->list));
    for (i = 0; i < ARRAY_size(s1->list); i++) {
        ARRAY_add(uint8_t, &s3->list, ARRAY_get(uint8_t, &s1->list, i));
    }
    for (i = 0; i < ARRAY_size(s2->list); i++) {
        ARRAY_add(uint8_t, &s3->list, ARRAY_get(uint8_t, &s2->list, i));
    }
    return (expr_t *)s3;
}

static void _moz_Sequence_optimize(moz_compiler_t *C, Sequence_t *e)
{
    int i = 0;
    for (i = 0; i < (int)ARRAY_size(e->list); i++) {
        expr_t *child = ARRAY_get(expr_ptr_t, &e->list, i);
        /* flatten */
        if (child->type == Sequence && isPatternMatchOnly(child)) {
            Sequence_t *seq = (Sequence_t *)child;
            moz_Sequence_do_flatten(e, i, seq);
        }

        /*
         * e = [A, "hello", ' ', "world", B]
         * -> e = [A, "hello ", Str3, B]
         * -> e = [A, "hello world", B]
         */
        while (child->type == Str && i + 1 < (int)ARRAY_size(e->list)) {
            Str_t *s1 = (Str_t *) child;
            expr_t *child2 = ARRAY_get(expr_ptr_t, &e->list, i + 1);
            if (child2->type == Byte) {
                Byte_t *byte = (Byte_t *) child2;
                ARRAY_add(uint8_t, &s1->list, byte->byte);
            }
            else if (child2->type == Str) {
                Str_t *s2 = (Str_t *) child2;
                child = moz_Sequence_concatString(s1, s2);
                ARRAY_set(expr_ptr_t, &e->list, i, child);
                ARRAY_dispose(uint8_t, &s1->list);
                ARRAY_dispose(uint8_t, &s2->list);
                ARRAY_remove(expr_ptr_t, &e->list, i + 1);
            }
            else {
                break;
            }
        }
        /*
         * e = [A, Not(Byte('4'), Any, C]
         * -> e = [A, Set[^4], C]
         */
        if (child->type == Not && i + 1 < (int)ARRAY_size(e->list)) {
            Not_t *not = (Not_t *)(child);
            expr_t *child2 = ARRAY_get(expr_ptr_t, &e->list, i + 1);
            if (not->expr->type == Byte && child2->type == Any) {
                Byte_t *byte = (Byte_t *)not->expr;
                Set_t *set = EXPR_ALLOC(Set);
                bitset_init(&set->set);
                bitset_set(&set->set, byte->byte);
                bitset_flip(&set->set);
                ARRAY_set(expr_ptr_t, &e->list, i, (expr_t *)set);
                ARRAY_remove(expr_ptr_t, &e->list, i + 1);

            }
        }
    }
}

static void moz_Sequence_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Sequence_t *expr = (Sequence_t *) e;
    int i;

    expr_t **x, **end;
    FOR_EACH_ARRAY(expr->list, x, end) {
        moz_expr_optimize(C, e, x, *x);
    }

    _moz_Sequence_optimize(C, expr);

    for (i = ARRAY_size(expr->list) - 1; i >= 0; i--) {
        expr_t *child = ARRAY_get(expr_ptr_t, &expr->list, i);
        if (child->type == Empty) {
            ARRAY_remove(expr_ptr_t, &expr->list, i);
        }
    }

    if (ARRAY_size(expr->list) == 1) {
        *ref = ARRAY_get(expr_ptr_t, &expr->list, 0);
        ARRAY_dispose(expr_ptr_t, &expr->list);
    }
}

static void moz_Repetition_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Repetition_t *expr = (Repetition_t *) e;
    expr_t **x, **end;
    FOR_EACH_ARRAY(expr->list, x, end) {
        moz_expr_optimize(C, e, x, *x);
    }
}

static void moz_Repetition1_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Repetition1_t *expr = (Repetition1_t *) e;
    expr_t **x, **end;
    FOR_EACH_ARRAY(expr->list, x, end) {
        moz_expr_optimize(C, e, x, *x);
    }
}

typedef void (*f_optimize)(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e);
static void moz_expr_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    f_optimize optimize[] = {
#define F_OPTIMIZE_DECL(NAME, DUMP, OPT) moz_##OPT##_optimize,
        FOR_EACH_BASE_AST(F_OPTIMIZE_DECL)
        FOR_EACH_EXTRA_AST(F_OPTIMIZE_DECL)
#undef  F_OPTIMIZE_DECL
    };
    optimize[e->type](C, parent, ref, e);
}

static void moz_ast_optimize(moz_compiler_t *C)
{
    decl_t **decl, **end;
    FOR_EACH_ARRAY(C->decls, decl, end) {
        if ((*decl)->refc > 0) {
            moz_expr_optimize(C, NULL, &(*decl)->body, (*decl)->body);
        }
    }
    FOR_EACH_ARRAY(C->decls, decl, end) {
        if ((*decl)->refc > 0) {
            moz_expr_optimize(C, NULL, &(*decl)->body, (*decl)->body);
        }
    }
}

/* compile */

static void moz_ast_prepare(moz_compiler_t *C, Node *node)
{
    unsigned i, decl_size = Node_length(node);

    for (i = 0; i < decl_size; i++) {
        Node *child = Node_get(node, i);
        if (tag_equal(child, "Production")) {
            decl_t *decl = decl_new();
            assert(Node_length(child) == 3);
            decl_set_name(decl, Node_get(child, 1));
            // fprintf(stderr, "%.*s\n", decl->name.len, decl->name.str);
            // Node_print(child, C->R->C.tags);
            ARRAY_add(decl_ptr_t, &C->decls, decl);
        }
    }
    if (ARRAY_size(C->decls) > 0) {
        ARRAY_get(decl_ptr_t, &C->decls, 0)->refc++;
    }
}

typedef struct moz_state_t {
    ILabel_t *next;
    ILabel_t *fail;
    ILabel_t *cont;
} moz_state_t;

typedef void (*f_to_ir)(moz_compiler_t *C, moz_state_t *S, expr_t *e);

static void moz_expr_to_ir(moz_compiler_t *C, moz_state_t *S, expr_t *e);

static void moz_Empty_to_ir(moz_compiler_t *C, moz_state_t *S, Empty_t *e)
{
    /* do nothing */
}

static void moz_Invoke_to_ir(moz_compiler_t *C, moz_state_t *S, Invoke_t *e)
{
    IInvoke_t *ir = IR_ALLOC(IInvoke);
    ir->v.decl = e->decl;
}

static void moz_Any_to_ir(moz_compiler_t *C, moz_state_t *S, Any_t *e)
{
    IAny_t *ir = IR_ALLOC(IAny);
    (void) ir;
}

static void moz_Byte_to_ir(moz_compiler_t *C, moz_state_t *S, Byte_t *e)
{
    IByte_t *ir = IR_ALLOC(IByte);
    ir->byte = e->byte;
}

static void moz_Str_to_ir(moz_compiler_t *C, moz_state_t *S, Str_t *e)
{
    IStr_t *ir = IR_ALLOC(IStr);
    ir->strId = moz_compiler_get_string(C, &e->list);
}

static void moz_Set_to_ir(moz_compiler_t *C, moz_state_t *S, Set_t *e)
{
    ISet_t *ir = IR_ALLOC(ISet);
    ir->setId = moz_compiler_get_set(C, &e->set);
}

static void moz_And_to_ir(moz_compiler_t *C, moz_state_t *S, And_t *e)
{
    assert(0 && "TODO");
}

static void moz_Choice_to_ir(moz_compiler_t *C, moz_state_t *S, Choice_t *e)
{
    assert(0 && "TODO");
}

static void moz_Fail_to_ir(moz_compiler_t *C, moz_state_t *S, Fail_t *e)
{
    assert(0 && "TODO");
}

static void moz_Not_to_ir(moz_compiler_t *C, moz_state_t *S, Not_t *e)
{
    assert(0 && "TODO");
}

static void moz_Option_to_ir(moz_compiler_t *C, moz_state_t *S, Option_t *e)
{
    assert(0 && "TODO");
}

static void moz_Sequence_to_ir(moz_compiler_t *C, moz_state_t *S, Sequence_t *e)
{
    /**
     * Sequence(E1, E2, E3)
     * parent_state = (NEXT, FAIL, CONT)
     * L_head:
     *  E1, next1, FAIL
     * L_next1:
     *  E2, next2, FAIL
     * L_next2:
     *  E3, next3, FAIL
     * L_next3:
     * L_fail:
     */
    moz_state_t state = {};
    state.next = IR_ALLOC(ILabel);
    state.fail = IR_ALLOC(ILabel);
    state.cont = state.next;
    moz_expr_dump(0, (expr_t *)e);
    assert(0 && "TODO");
}

static void moz_Repetition_to_ir(moz_compiler_t *C, moz_state_t *S, Repetition_t *e)
{
    moz_state_t state = {};
    expr_t **x, **end;
    FOR_EACH_ARRAY(e->list, x, end) {
        moz_expr_to_ir(C, &state, *x);
    }
    assert(0 && "TODO");
}

static void moz_Repetition1_to_ir(moz_compiler_t *C, moz_state_t *S, Repetition1_t *e)
{
    moz_state_t state = {};
    expr_t **x, **end;
    FOR_EACH_ARRAY(e->list, x, end) {
        moz_expr_to_ir(C, &state, *x);
    }
}

static void moz_Tcapture_to_ir(moz_compiler_t *C, moz_state_t *S, Tcapture_t *e)
{
    assert(0 && "TODO");
}

static void moz_Tdetree_to_ir(moz_compiler_t *C, moz_state_t *S, Tdetree_t *e)
{
    assert(0 && "TODO");
}

static void moz_Tlfold_to_ir(moz_compiler_t *C, moz_state_t *S, Tlfold_t *e)
{
    assert(0 && "TODO");
}

static void moz_Tlink_to_ir(moz_compiler_t *C, moz_state_t *S, Tlink_t *e)
{
    assert(0 && "TODO");
}

static void moz_Tnew_to_ir(moz_compiler_t *C, moz_state_t *S, Tnew_t *e)
{
    assert(0 && "TODO");
}

static void moz_Treplace_to_ir(moz_compiler_t *C, moz_state_t *S, Treplace_t *e)
{
    assert(0 && "TODO");
}

static void moz_Ttag_to_ir(moz_compiler_t *C, moz_state_t *S, Ttag_t *e)
{
    ITTag_t *ir = IR_ALLOC(ITTag);
    ir->tagId = moz_compiler_get_tag(C, &e->name);
}

static void moz_Xblock_to_ir(moz_compiler_t *C, moz_state_t *S, Xblock_t *e)
{
    assert(0 && "TODO");
}

static void moz_Xexists_to_ir(moz_compiler_t *C, moz_state_t *S, Xexists_t *e)
{
    assert(0 && "TODO");
}

static void moz_Xif_to_ir(moz_compiler_t *C, moz_state_t *S, Xif_t *e)
{
    assert(0 && "TODO");
}

static void moz_Xis_to_ir(moz_compiler_t *C, moz_state_t *S, Xis_t *e)
{
    assert(0 && "TODO");
}

static void moz_Xisa_to_ir(moz_compiler_t *C, moz_state_t *S, Xisa_t *e)
{
    assert(0 && "TODO");
}

static void moz_Xon_to_ir(moz_compiler_t *C, moz_state_t *S, Xon_t *e)
{
    assert(0 && "TODO");
}

static void moz_Xmatch_to_ir(moz_compiler_t *C, moz_state_t *S, Xmatch_t *e)
{
    assert(0 && "TODO");
}

static void moz_Xlocal_to_ir(moz_compiler_t *C, moz_state_t *S, Xlocal_t *e)
{
    assert(0 && "TODO");
}

static void moz_Xsymbol_to_ir(moz_compiler_t *C, moz_state_t *S, Xsymbol_t *e)
{
    assert(0 && "TODO");
}

static void moz_expr_to_ir(moz_compiler_t *C, moz_state_t *S, expr_t *e)
{
    f_to_ir translate[] = {
#define F_IR_DECL(NAME, DUMP, OPT) (f_to_ir) moz_##NAME##_to_ir,
        FOR_EACH_BASE_AST(F_IR_DECL)
#undef  F_IR_DECL
    };
    translate[e->type](C, S, e);
}

static void moz_decl_to_ir(moz_compiler_t *C, decl_t *decl)
{
    moz_state_t state = {};
    /*
     * Repetition1 {
     *   Byte('A')
     * }
     * L_head: Byte 'A'
     * L_cont: L
     * L_fail: L
     */
    ILabel_t *head = IR_ALLOC(ILabel);
    state.next = IR_ALLOC(ILabel);
    state.fail = IR_ALLOC(ILabel);
    state.cont = state.next;
    IR_next(head, state.next);
    moz_expr_to_ir(C, &state, decl->body);
}

static void moz_ast_to_ir(moz_compiler_t *C)
{
    decl_t **decl, **end;
    FOR_EACH_ARRAY(C->decls, decl, end) {
        if ((*decl)->refc > 0) {
            moz_decl_to_ir(C, *decl);
        }
    }
}

static void moz_ir_optimize(moz_compiler_t *C)
{
}

static void moz_ir_flatten(moz_compiler_t *C)
{
}

void moz_compiler_compile(const char *output_file, moz_runtime_t *R, Node *node)
{
    unsigned i, decl_index = 0, decl_size = Node_length(node);
    moz_compiler_t C;
    C.R = R;
    ARRAY_init(decl_ptr_t, &C.decls, decl_size);
    ARRAY_init(pstring_ptr_t, &C.strs, 1);

    moz_ast_prepare(&C, node);
    for (i = 0; i < decl_size; i++) {
        Node *child = Node_get(node, i);
        if (tag_equal(child, "Comment")) {
            compile_comment(&C, child);
        }
        if (tag_equal(child, "Production")) {
            decl_t *decl = ARRAY_get(decl_ptr_t, &C.decls, decl_index++);
            compile_production(&C, child, decl);
        }
        if (tag_equal(child, "Format")) {
            compile_format(&C, child);
        }
    }
    moz_ast_optimize(&C);
    moz_ast_dump(&C);
    moz_ast_to_ir(&C);
    moz_ir_optimize(&C);
    moz_ir_flatten(&C);
}

#ifdef __cplusplus
}
#endif
