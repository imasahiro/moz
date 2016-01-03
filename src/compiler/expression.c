#include "compiler.h"
#include "expression.h"
#include "dump.h"

#define MOZC_USE_AST_INLINING 1

DEF_ARRAY_OP_NOPOINTER(decl_ptr_t);
DEF_ARRAY_OP_NOPOINTER(expr_ptr_t);
DEF_ARRAY_OP_NOPOINTER(uint8_t);

static unsigned to_hex(uint8_t c)
{
    if(c >= '0' && c <= '9') return c - '0';
    else if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    else if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
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
    MOZ_RC_INIT(decl);
    decl->type = DECL;
    return decl;
}

decl_t *moz_decl_new(moz_compiler_t *C, const char *name, unsigned len)
{
    decl_t *decl = decl_new();
    decl->name.str = name;
    decl->name.len = len;
    ARRAY_add(decl_ptr_t, &C->decls, decl);
    return decl;
}

void moz_decl_mark_as_top_level(decl_t *decl)
{
    MOZ_RC_RETAIN(decl);
}

static int moz_decl_use_once(decl_t *decl)
{
    return MOZ_RC_COUNT(decl) == 2;
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
    MOZ_RC_INIT(e);
    e->type = type;
    return e;
}

#define EXPR_ALLOC(T)   _EXPR_ALLOC(sizeof(T##_t), T)
#define EXPR_ALLOC_T(T) ((T##_t *) _EXPR_ALLOC(sizeof(T##_t), T))

static expr_t *moz_expr_new_Invoke(moz_compiler_t *C, const char *str, unsigned len, decl_t *decl)
{
    Invoke_t *e = EXPR_ALLOC_T(Invoke);
    e->name.str = str;
    e->name.len = len;
    if (decl) {
        e->decl = decl;
        MOZ_RC_RETAIN(decl);
    }
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Any(moz_compiler_t *C)
{
    return EXPR_ALLOC(Any);
}

static expr_t *moz_expr_new_Byte(moz_compiler_t *C, uint8_t byte)
{
    Byte_t *e = EXPR_ALLOC_T(Byte);
    e->byte = byte;
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Str(moz_compiler_t *C, const char *str, unsigned len)
{
    unsigned i;
    Str_t *e = EXPR_ALLOC_T(Str);
    assert(len > 0);
    ARRAY_init(uint8_t, &e->list, len);
    for (i = 0; i < len; i++) {
        ARRAY_add(uint8_t, &e->list, str[i]);
    }
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Set(moz_compiler_t *C, unsigned *data, unsigned len)
{
    Set_t *e = EXPR_ALLOC_T(Set);
    unsigned i;
    bitset_init(&e->set);
    assert(len % 2 == 0);
    for (i = 0; i < len / 2; i++) {
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

static expr_t *moz_expr_new_And(moz_compiler_t *C, expr_t *expr)
{
    And_t *e = EXPR_ALLOC_T(And);
    MOZ_RC_INIT_FIELD(e->expr, expr);
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Choice(moz_compiler_t *C)
{
    return EXPR_ALLOC(Choice);
}

static expr_t *moz_expr_new_Empty(moz_compiler_t *C)
{
    Empty_t *e = EXPR_ALLOC_T(Empty);
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Fail(moz_compiler_t *C)
{
    return EXPR_ALLOC(Fail);
}

static expr_t *moz_expr_new_Not(moz_compiler_t *C, expr_t *expr)
{
    Not_t *e = EXPR_ALLOC_T(Not);
    MOZ_RC_INIT_FIELD(e->expr, expr);
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Option(moz_compiler_t *C, expr_t *expr)
{
    Option_t *e = EXPR_ALLOC_T(Option);
    MOZ_RC_INIT_FIELD(e->expr, expr);
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Sequence(moz_compiler_t *C)
{
    return EXPR_ALLOC(Sequence);
}

static expr_t *moz_expr_new_Repetition(moz_compiler_t *C)
{
    return EXPR_ALLOC(Repetition);
}

static expr_t *moz_expr_new_Tpush(moz_compiler_t *C)
{
    return EXPR_ALLOC(Tpush);
}

static expr_t *moz_expr_new_Tpop(moz_compiler_t *C, const char *str, unsigned len)
{
    Tpop_t *e = EXPR_ALLOC_T(Tpop);
    e->name.str = str;
    e->name.len = len;
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Tcapture(moz_compiler_t *C)
{
    return EXPR_ALLOC(Tcapture);
}

static expr_t *moz_expr_new_Tdetree(moz_compiler_t *C)
{
    assert(0 && "TODO");
    return EXPR_ALLOC(Tdetree);
}

static expr_t *moz_expr_new_Tlfold(moz_compiler_t *C)
{
    assert(0 && "TODO");
    return EXPR_ALLOC(Tlfold);
}

static expr_t *moz_expr_new_Tnew(moz_compiler_t *C)
{
    return EXPR_ALLOC(Tnew);
}

static expr_t *moz_expr_new_Treplace(moz_compiler_t *C)
{
    Treplace_t *e = EXPR_ALLOC_T(Treplace);
    assert(0 && "TODO");
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Ttag(moz_compiler_t *C, const char *str, unsigned len)
{
    Ttag_t *e = EXPR_ALLOC_T(Ttag);
    e->name.str = str;
    e->name.len = len;
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Xblock(moz_compiler_t *C, expr_t *expr)
{
    Xblock_t *e = EXPR_ALLOC_T(Xblock);
    MOZ_RC_INIT_FIELD(e->expr, expr);
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Xexists(moz_compiler_t *C, const char *str, unsigned len)
{
    Xexists_t *e = EXPR_ALLOC_T(Xexists);
    e->name.str = str;
    e->name.len = len;
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Xif(moz_compiler_t *C)
{
    Xif_t *e = EXPR_ALLOC_T(Xif);
    assert(0 && "TODO");
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Xis(moz_compiler_t *C, const char *str, unsigned len)
{
    Xis_t *e = EXPR_ALLOC_T(Xis);
    e->name.str = str;
    e->name.len = len;
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Xisa(moz_compiler_t *C, const char *str, unsigned len)
{
    Xisa_t *e = EXPR_ALLOC_T(Xisa);
    e->name.str = str;
    e->name.len = len;
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Xlocal(moz_compiler_t *C, const char *str, unsigned len, expr_t *expr)
{
    Xlocal_t *e = EXPR_ALLOC_T(Xlocal);
    e->name.str = str;
    e->name.len = len;
    MOZ_RC_INIT_FIELD(e->expr, expr);
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Xmatch(moz_compiler_t *C)
{
    Xmatch_t *e = EXPR_ALLOC_T(Xmatch);
    assert(0 && "TODO");
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Xon(moz_compiler_t *C)
{
    Xon_t *e = EXPR_ALLOC_T(Xon);
    assert(0 && "TODO");
    return (expr_t *)e;
}

static expr_t *moz_expr_new_Xsymbol(moz_compiler_t *C, const char *str, unsigned len, expr_t *expr)
{
    Xsymbol_t *e = EXPR_ALLOC_T(Xsymbol);
    e->name.str = str;
    e->name.len = len;
    MOZ_RC_INIT_FIELD(e->expr, expr);
    return (expr_t *)e;
}

static void _compile_List(moz_compiler_t *C, Node *node, ARRAY(expr_ptr_t) *list)
{
    unsigned i, len = Node_length(node);
    ARRAY_init(expr_ptr_t, list, len);
    for (i = 0; i < len; i++) {
        Node *child = Node_get(node, i);
        expr_t *expr = compile_expression(C, child);
        ARRAY_add(expr_ptr_t, list, expr);
        MOZ_RC_RETAIN(expr);
    }
}

static expr_t *compile_Invoke(moz_compiler_t *C, Node *node)
{
    decl_t **x, **end;
    decl_t *decl = NULL;

    FOR_EACH_ARRAY(C->decls, x, end) {
        if (node->len == (*x)->name.len &&
                strncmp(node->pos, (*x)->name.str, node->len) == 0) {
            decl = *x;
            break;
        }
    }
    return moz_expr_new_Invoke(C, node->pos, node->len, decl);
}

static expr_t *compile_Any(moz_compiler_t *C, Node *node)
{
    return moz_expr_new_Any(C);
}

static expr_t *compile_Str(moz_compiler_t *C, Node *node)
{
    return moz_expr_new_Str(C, node->pos, node->len);
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
    return moz_expr_new_Empty(C);
}

static int parseEscapedChar(const uint8_t *p, uint8_t *out)
{
    uint8_t c = *p++;
    if (c == '\\') {
        c = *p++;
        switch (c) {
        case '\\':
            *out = '\\';
            return 1;
        case 'b':
            *out = '\b';
            return 1;
        case 'f':
            *out = '\f';
            return 1;
        case 'v':
            *out = '\v';
            return 1;
        case 'n':
            *out = '\n';
            return 1;
        case 'r':
            *out = '\r';
            return 1;
        case 't':
            *out = '\t';
            return 1;
        case 'x':
            c = to_hex(*p++);
            c = c << 8 | to_hex(*p);
            *out = c;
            return 1;
        }
    }
    return 0;
}

static expr_t *compile_Byte(moz_compiler_t *C, Node *node)
{
    uint8_t byte;
    assert(Node_length(node) == 0);
    if (node->len == 0) {
        return compile_Empty(C, node);
    }

    byte = node->pos[0];
    if (node->len == 1 || parseEscapedChar((const uint8_t *)node->pos, &byte)) {
        return moz_expr_new_Byte(C, byte);
    }
    else {
        return compile_Str(C, node);
    }
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
    unsigned data[len * 2];
    assert(len > 0);
    memset(data, 0, sizeof(unsigned) * len * 2);
    for (i = 0; i < len; i++) {
        Node *child = Node_get(node, i);
        if (tag_equal(child, "Class")) {
            unsigned ch = parseClass((const uint8_t *)child->pos);
            data[i * 2 + 0] = data[i * 2 + 1] = ch;
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
    return moz_expr_new_Set(C, data, len * 2);
}

static expr_t *compile_And(moz_compiler_t *C, Node *node)
{
    assert(Node_length(node) == 1);
    return moz_expr_new_And(C, compile_expression(C, Node_get(node, 0)));
}

static expr_t *compile_Choice(moz_compiler_t *C, Node *node)
{
    expr_t *e = moz_expr_new_Choice(C);
    _compile_List(C, node, &((Choice_t *)e)->list);
    return e;
}

static expr_t *compile_Fail(moz_compiler_t *C, Node *node)
{
    assert(0 && "TODO");
    return moz_expr_new_Fail(C);
}

static expr_t *compile_Not(moz_compiler_t *C, Node *node)
{
    assert(Node_length(node) == 1);
    return moz_expr_new_Not(C, compile_expression(C, Node_get(node, 0)));
}

static expr_t *compile_Option(moz_compiler_t *C, Node *node)
{
    assert(Node_length(node) == 1);
    return moz_expr_new_Option(C, compile_expression(C, Node_get(node, 0)));
}

static expr_t *compile_Sequence(moz_compiler_t *C, Node *node)
{
    expr_t *e = moz_expr_new_Sequence(C);
    _compile_List(C, node, &((Sequence_t *)e)->list);
    return e;
}

static expr_t *compile_Repetition(moz_compiler_t *C, Node *node)
{
    expr_t *e = moz_expr_new_Repetition(C);
    assert(Node_length(node) == 1);
    _compile_List(C, node, &((Repetition_t *)e)->list);
    return e;
}

static expr_t *compile_Repetition1(moz_compiler_t *C, Node *node)
{
    expr_t **x, **end;
    Sequence_t *e = EXPR_ALLOC_T(Sequence);
    expr_t *rep = compile_Repetition(C, node);

    assert(rep->type == Repetition);
    FOR_EACH_ARRAY(((Repetition_t *)rep)->list, x, end) {
        ARRAY_add(expr_ptr_t, &e->list, *x);
        MOZ_RC_RETAIN(*x);
    }
    ARRAY_add(expr_ptr_t, &e->list, rep);
    MOZ_RC_RETAIN(rep);
    return (expr_t *)e;
}

static expr_t *compile_Tcapture(moz_compiler_t *C, Node *node)
{
    return moz_expr_new_Tcapture(C);
}

static expr_t *compile_Tdetree(moz_compiler_t *C, Node *node)
{
    return moz_expr_new_Tdetree(C);
}

static expr_t *compile_Tlfold(moz_compiler_t *C, Node *node)
{
    return moz_expr_new_Tlfold(C);
}

static expr_t *compile_Tlink(moz_compiler_t *C, Node *node)
{
    Node *child = NULL;
    const char *str = "";
    unsigned len = 0;
    Sequence_t *seq = (Sequence_t *)moz_expr_new_Sequence(C);

    if (Node_length(node) == 1) {
        child = Node_get(node, 0);
    }
    else {
        Node *label = Node_get(node, 0);
        str = label->pos;
        len = label->len;
        child = Node_get(node, 1);
    }
    {
        expr_t *push = moz_expr_new_Tpush(C);
        expr_t *body = compile_expression(C, child);
        expr_t *pop  = moz_expr_new_Tpop(C, str, len);
        MOZ_RC_RETAIN(push);
        MOZ_RC_RETAIN(body);
        MOZ_RC_RETAIN(pop);
        ARRAY_add(expr_ptr_t, &seq->list, push);
        ARRAY_add(expr_ptr_t, &seq->list, body);
        ARRAY_add(expr_ptr_t, &seq->list, pop);
    }
    return (expr_t *)seq;
}

static expr_t *compile_Tnew(moz_compiler_t *C, Node *node)
{
    Sequence_t *seq = EXPR_ALLOC_T(Sequence);
    expr_t *tnew = moz_expr_new_Tnew(C);
    expr_t *tcap = moz_expr_new_Tcapture(C);

    expr_t *body = compile_expression(C, Node_get(node, 0));
    ARRAY_add(expr_ptr_t, &seq->list, tnew);
    ARRAY_add(expr_ptr_t, &seq->list, body);
    ARRAY_add(expr_ptr_t, &seq->list, tcap);
    MOZ_RC_RETAIN(tnew);
    MOZ_RC_RETAIN(body);
    MOZ_RC_RETAIN(tcap);
    return (expr_t *)seq;
}

static expr_t *compile_Treplace(moz_compiler_t *C, Node *node)
{
    return moz_expr_new_Treplace(C);
}

static expr_t *compile_Ttag(moz_compiler_t *C, Node *node)
{
    return moz_expr_new_Ttag(C, node->pos, node->len);
}

static expr_t *compile_Xblock(moz_compiler_t *C, Node *node)
{
    return moz_expr_new_Xblock(C, compile_expression(C, Node_get(node, 0)));
}

static expr_t *compile_Xexists(moz_compiler_t *C, Node *node)
{
    Node *name = Node_get(node, 0);
    return moz_expr_new_Xexists(C, name->pos, name->len);
}

static expr_t *compile_Xif(moz_compiler_t *C, Node *node)
{
    return moz_expr_new_Xif(C);
}

static expr_t *compile_Xis(moz_compiler_t *C, Node *node)
{
    Node *name = Node_get(node, 0);
    return moz_expr_new_Xis(C, name->pos, name->len);
}

static expr_t *compile_Xisa(moz_compiler_t *C, Node *node)
{
    Node *name = Node_get(node, 0);
    return moz_expr_new_Xisa(C, name->pos, name->len);
}

static expr_t *compile_Xlocal(moz_compiler_t *C, Node *node)
{
    Node *name = Node_get(node, 0);
    expr_t *expr = compile_expression(C, Node_get(node, 1));
    return moz_expr_new_Xlocal(C, name->pos, name->len, expr);
}

static expr_t *compile_Xmatch(moz_compiler_t *C, Node *node)
{
    return moz_expr_new_Xmatch(C);
}

static expr_t *compile_Xon(moz_compiler_t *C, Node *node)
{
    return moz_expr_new_Xon(C);
}

static expr_t *compile_Xsymbol(moz_compiler_t *C, Node *node)
{
    Node *name = Node_get(node, 0);
    expr_t *expr = compile_expression(C, Node_get(node, 1));
    return moz_expr_new_Xsymbol(C, name->pos, name->len, expr);
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
    MOZ_RC_INIT_FIELD(decl->body, compile_expression(C, Node_get(node, 2)));
}

/* dump */
static const char *ast_name[] = {
#define DEFINE_NAME(NAME, DUMP, OPT, SWEEP) #NAME,
    FOR_EACH_BASE_AST(DEFINE_NAME)
#undef DEFINE_NAME
};

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
            decl->name.len, decl->name.str, MOZ_RC_COUNT(decl));
    moz_expr_dump(level + 1, decl->body);
    fprint_indent(level);
    fprintf(stdout, "}\n");
}

typedef void (*f_dump)(int level, expr_t *);

void moz_expr_dump(int level, expr_t *e)
{
    f_dump dump[] = {
#define F_DUMP_DECL(NAME, DUMP, OPT, SWEEP) moz_##DUMP##_dump,
        FOR_EACH_BASE_AST(F_DUMP_DECL)
#undef  F_DUMP_DECL
    };
    dump[e->type](level, e);
}

void moz_ast_dump(moz_compiler_t *C)
{
    decl_t **decl, **end;
    fprintf(stderr, "------------\n");
    FOR_EACH_ARRAY(C->decls, decl, end) {
        if (MOZ_RC_COUNT(*decl) > 0) {
            moz_decl_dump(0, *decl);
        }
    }
    fprintf(stderr, "------------\n");
}

/* optimize */
static int moz_expr_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e);

static int moz_Expr_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    /* do nothing */
    return 0;
}

static int moz_NameUnary_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    NameUnary_t *expr = (NameUnary_t *) e;
    return moz_expr_optimize(C, e, &expr->expr, expr->expr);
}

static int moz_Unary_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Unary_t *expr = (Unary_t *) e;
    return moz_expr_optimize(C, e, &expr->expr, expr->expr);
}

static int _moz_List_optimize(moz_compiler_t *C, expr_t *e)
{
    int modified = 0;
    List_t *expr = (List_t *) e;
    expr_t **x, **end;
    FOR_EACH_ARRAY(expr->list, x, end) {
        modified |= moz_expr_optimize(C, e, x, *x);
    }
    return modified;
}

static void moz_ast_do_inline(expr_t **ref, Invoke_t *expr)
{
    decl_t *decl = expr->decl;
    assert((expr_t *)expr == *ref);
    MOZ_RC_ASSIGN(*ref, decl->body, moz_expr_sweep);
}

static int moz_Invoke_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Invoke_t *expr = (Invoke_t *)e;
    if (expr->decl != NULL) {
        decl_t *decl = expr->decl;
        if (MOZC_USE_AST_INLINING &&
                (moz_decl_use_once(decl) || isPatternMatchOnly(decl->body))) {
            moz_ast_do_inline(ref, expr);
            return 1;
        }
        if (MOZC_USE_AST_INLINING) {
            // Inline decl body if decl only have single expression.
            assert(decl->body != NULL);
            switch (decl->body->type) {
            case Empty:
            case Any:
            case Byte:
            case Str:
            case Set:
            case Fail:
                moz_ast_do_inline(ref, expr);
                return 1;
            default:
                break;
            }
            }
    }
    return 0;
}

static int moz_Not_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Unary_t *expr = (Unary_t *) e;
    return moz_expr_optimize(C, e, &expr->expr, expr->expr);
}

static int moz_Option_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Unary_t *expr = (Unary_t *) e;
    return moz_expr_optimize(C, e, &expr->expr, expr->expr);
}

static bool useByteMapOptimization(Choice_t *e)
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
            return false;
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

static int moz_Choice_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Choice_t *expr = (Choice_t *) e;
    bitset_t bitset;
    expr_t **x, **end;
    int modified = _moz_List_optimize(C, e);

    if (ARRAY_size(expr->list) == 1) {
        expr_t *child = ARRAY_get(expr_ptr_t, &expr->list, 0);
        MOZ_RC_ASSIGN(*ref, child, moz_expr_sweep);
        ARRAY_dispose(expr_ptr_t, &expr->list);
        return 1;
    }
    if (useByteMapOptimization(expr)) {
        Set_t *set = EXPR_ALLOC_T(Set);
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
        MOZ_RC_ASSIGN(*ref, (expr_t *)set, moz_expr_sweep);
        return 1;
    }
    return modified;
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
        MOZ_RC_RETAIN(*x);
    }
    MOZ_RC_RELEASE((expr_t *)seq, moz_expr_sweep);
}

static expr_t *moz_Sequence_concatString(Str_t *s1, Str_t *s2)
{
    Str_t *s3 = EXPR_ALLOC_T(Str);
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

static expr_t *moz_Sequence_concatByte(Byte_t *b1, Byte_t *b2)
{
    Str_t *s3 = EXPR_ALLOC_T(Str);
    ARRAY_init(uint8_t, &s3->list, 2);
    ARRAY_add(uint8_t, &s3->list, b1->byte);
    ARRAY_add(uint8_t, &s3->list, b2->byte);
    return (expr_t *)s3;
}

static int _moz_Sequence_optimize(moz_compiler_t *C, Sequence_t *e)
{
    int modified = 0;
    int i = 0;
    for (i = 0; i < (int)ARRAY_size(e->list); i++) {
        expr_t *child = ARRAY_get(expr_ptr_t, &e->list, i);
        /* flatten */
        if (child->type == Sequence) {
            Sequence_t *seq = (Sequence_t *)child;
            moz_Sequence_do_flatten(e, i, seq);
            modified = 1;
        }

        /*
         * e = [A, 'X', 'Y'B]
         * -> e = [A, "XY", B]
         */
        if (child->type == Byte && i + 1 < (int)ARRAY_size(e->list)) {
            Byte_t *b1 = (Byte_t *) child;
            expr_t *child2 = ARRAY_get(expr_ptr_t, &e->list, i + 1);
            if (child2->type == Byte) {
                Byte_t *b2 = (Byte_t *) child2;
                expr_t *expr = moz_Sequence_concatByte(b1, b2);
                ARRAY_set(expr_ptr_t, &e->list, i, expr);
                MOZ_RC_RETAIN(expr);
                MOZ_RC_RELEASE(child, moz_expr_sweep);
                MOZ_RC_RELEASE(child2, moz_expr_sweep);
                ARRAY_remove(expr_ptr_t, &e->list, i + 1);
                modified = 1;
            }
        }

        /*
         * e = [A, "hello", ' ', "world", B]
         * -> e = [A, "hello ", "world", B]
         * -> e = [A, "hello world", B]
         */
        while (child->type == Str && i + 1 < (int)ARRAY_size(e->list)) {
            Str_t *s1 = (Str_t *) child;
            expr_t *child2 = ARRAY_get(expr_ptr_t, &e->list, i + 1);
            if (child2->type == Byte) {
                //TODO Need to allocate new Str node?
                Byte_t *byte = (Byte_t *) child2;
                ARRAY_add(uint8_t, &s1->list, byte->byte);
                MOZ_RC_RELEASE(child2, moz_expr_sweep);
                ARRAY_remove(expr_ptr_t, &e->list, i + 1);
                modified = 1;
            }
            else if (child2->type == Str) {
                Str_t *s2 = (Str_t *) child2;
                expr_t *expr = moz_Sequence_concatString(s1, s2);
                ARRAY_set(expr_ptr_t, &e->list, i, expr);
                ARRAY_dispose(uint8_t, &s1->list);
                ARRAY_dispose(uint8_t, &s2->list);
                ARRAY_remove(expr_ptr_t, &e->list, i + 1);
                MOZ_RC_RETAIN(expr);
                MOZ_RC_RELEASE(child, moz_expr_sweep);
                MOZ_RC_RELEASE(child2, moz_expr_sweep);
                modified = 1;
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
                Set_t *set = EXPR_ALLOC_T(Set);
                bitset_init(&set->set);
                bitset_set(&set->set, byte->byte);
                bitset_flip(&set->set);
                ARRAY_set(expr_ptr_t, &e->list, i, (expr_t *)set);
                ARRAY_remove(expr_ptr_t, &e->list, i + 1);
                MOZ_RC_RETAIN((expr_t *)set);
                MOZ_RC_RELEASE(child, moz_expr_sweep);
                MOZ_RC_RELEASE(child2, moz_expr_sweep);
                modified = 1;
            }
        }
    }
    return modified;
}

static int moz_Sequence_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    Sequence_t *expr = (Sequence_t *) e;
    int modified = 0;
    int i;

    expr_t **x, **end;
    FOR_EACH_ARRAY(expr->list, x, end) {
        modified |= moz_expr_optimize(C, e, x, *x);
    }

    modified |= _moz_Sequence_optimize(C, expr);

    for (i = ARRAY_size(expr->list) - 1; i >= 0; i--) {
        expr_t *child = ARRAY_get(expr_ptr_t, &expr->list, i);
        if (child->type == Empty) {
            ARRAY_remove(expr_ptr_t, &expr->list, i);
            MOZ_RC_RELEASE(child, moz_expr_sweep);
            modified = 1;
        }
    }

    if (ARRAY_size(expr->list) == 1) {
        MOZ_RC_ASSIGN(*ref, ARRAY_get(expr_ptr_t, &expr->list, 0), moz_expr_sweep);
        ARRAY_dispose(expr_ptr_t, &expr->list);
        return 1;
    }
    return modified;
}

static int moz_Repetition_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    int modified = 0;
    Repetition_t *expr = (Repetition_t *) e;
    expr_t **x, **end;
    FOR_EACH_ARRAY(expr->list, x, end) {
        modified |= moz_expr_optimize(C, e, x, *x);
    }
    return modified;
}

typedef int (*f_optimize)(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e);
static int moz_expr_optimize(moz_compiler_t *C, expr_t *parent, expr_t **ref, expr_t *e)
{
    f_optimize optimize[] = {
#define F_OPTIMIZE_DECL(NAME, DUMP, OPT, SWEEP) moz_##OPT##_optimize,
        FOR_EACH_BASE_AST(F_OPTIMIZE_DECL)
#undef  F_OPTIMIZE_DECL
    };
    return optimize[e->type](C, parent, ref, e);
}

static void moz_ast_mark_live_decl(moz_compiler_t *C)
{
    decl_t **decl, **end;
    FOR_EACH_ARRAY(C->decls, decl, end) {
        MOZ_RC_RETAIN(*decl);
    }
}

static void moz_ast_remove_unused_decl(moz_compiler_t *C)
{
    int i, removed = 0;
    decl_t **decl, **end;
    decl_t *decls[ARRAY_size(C->decls) + 1];
    for (i = 0; i < (int)ARRAY_size(C->decls); i++) {
        decls[i] = NULL;
    }
    FOR_EACH_ARRAY(C->decls, decl, end) {
        if (MOZ_RC_COUNT(*decl) == 1) {
            decls[removed++] = *decl;
        }
        MOZ_RC_RELEASE(*decl, moz_decl_sweep);
    }
    for (i = 0; i < removed; i++) {
        ARRAY_remove_element(decl_ptr_t, &C->decls, decls[i]);
    }
}

void moz_ast_optimize(moz_compiler_t *C)
{
    int modified = 1;
    while (modified) {
        decl_t **decl, **end;
        modified = 0;
        moz_ast_mark_live_decl(C);
        FOR_EACH_ARRAY(C->decls, decl, end) {
            modified |= moz_expr_optimize(C, NULL, &(*decl)->body, (*decl)->body);
        }
        moz_ast_remove_unused_decl(C);
    }
}

/* sweep */
void moz_decl_sweep(decl_t *decl)
{
    MOZ_RC_RELEASE(decl->body, moz_expr_sweep);
    decl->body = NULL;
    // memset(decl, 0, sizeof(*decl));
    VM_FREE(decl);
}

static void moz_Expr_sweep(expr_t *e)
{
    memset(e, 0, sizeof(*e));
    VM_FREE(e);
}

static void moz_Unary_sweep(Unary_t *e)
{
    MOZ_RC_RELEASE(e->expr, moz_expr_sweep);
    memset(e, 0, sizeof(*e));
    VM_FREE(e);
}

static void moz_NameUnary_sweep(NameUnary_t *e)
{
    MOZ_RC_RELEASE(e->expr, moz_expr_sweep);
    memset(e, 0, sizeof(*e));
    VM_FREE(e);
}

static void moz_Invoke_sweep(Invoke_t *e)
{
    MOZ_RC_RELEASE(e->decl, moz_decl_sweep);
    memset(e, 0, sizeof(*e));
    VM_FREE(e);
}

static void moz_List_sweep(List_t *e)
{
    expr_t **x, **end;
    FOR_EACH_ARRAY(e->list, x, end) {
        MOZ_RC_RELEASE(*x, moz_expr_sweep);
    }
    memset(e, 0, sizeof(*e));
    VM_FREE(e);
}

typedef void (*f_sweep)(expr_t *e);

void moz_expr_sweep(expr_t *e)
{
    f_sweep sweep[] = {
#define F_SWEEP_DECL(NAME, DUMP, OPT, SWEEP) (f_sweep) moz_##SWEEP##_sweep,
        FOR_EACH_BASE_AST(F_SWEEP_DECL)
#undef  F_SWEEP_DECL
    };
    sweep[e->type](e);
}

static void moz_ast_prepare(moz_compiler_t *C, Node *node)
{
    unsigned i, decl_size = Node_length(node);

    for (i = 0; i < decl_size; i++) {
        Node *child = Node_get(node, i);
        if (tag_equal(child, "Production")) {
            Node *name;
            assert(Node_length(child) == 3);
            name = Node_get(child, 1);
            assert(tag_equal(name, "Name") || tag_equal(name, "String"));
            moz_decl_new(C, name->pos, name->len);
        }
    }
    if (ARRAY_size(C->decls) > 0) {
        moz_decl_mark_as_top_level(ARRAY_get(decl_ptr_t, &C->decls, 0));
    }
}

void moz_node_to_ast(moz_compiler_t *C, Node *node)
{
    unsigned i, decl_index = 0, decl_size = Node_length(node);
    moz_ast_prepare(C, node);
    for (i = 0; i < decl_size; i++) {
        Node *child = Node_get(node, i);
        if (tag_equal(child, "Comment")) {
            compile_comment(C, child);
        }
        if (tag_equal(child, "Production")) {
            decl_t *decl = ARRAY_get(decl_ptr_t, &C->decls, decl_index++);
            compile_production(C, child, decl);
        }
        if (tag_equal(child, "Format")) {
            compile_format(C, child);
        }
    }
    moz_ast_optimize(C);
}

moz_expr_factory_t *moz_compiler_get_factory()
{
    static moz_expr_factory_t factory = {
        moz_expr_new_Invoke,
        moz_expr_new_Any,
        moz_expr_new_Byte,
        moz_expr_new_Str,
        moz_expr_new_Set,
        moz_expr_new_And,
        moz_expr_new_Choice,
        moz_expr_new_Empty,
        moz_expr_new_Fail,
        moz_expr_new_Not,
        moz_expr_new_Option,
        moz_expr_new_Sequence,
        moz_expr_new_Repetition,
        moz_expr_new_Tcapture,
        moz_expr_new_Tdetree,
        moz_expr_new_Tlfold,
        moz_expr_new_Tpush,
        moz_expr_new_Tpop,
        moz_expr_new_Tnew,
        moz_expr_new_Treplace,
        moz_expr_new_Ttag,
        moz_expr_new_Xblock,
        moz_expr_new_Xexists,
        moz_expr_new_Xif,
        moz_expr_new_Xis,
        moz_expr_new_Xisa,
        moz_expr_new_Xlocal,
        moz_expr_new_Xmatch,
        moz_expr_new_Xon,
        moz_expr_new_Xsymbol,
    };
    return &factory;
}
