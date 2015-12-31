#include "mozvm.h"
#include "compiler.h"
#include "expression.h"
#include "core/pstring.h"
#include <assert.h>
#define MOZC_USE_AST_INLINING 1
#include "ir.h"
#include "block.c"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned attribute_t;
// #define DECL_ATTRIBUTE_PUBLIC (1 << 0)

typedef pstring_t *pstring_ptr_t;

DEF_ARRAY_STRUCT0(pstring_ptr_t, unsigned);
DEF_ARRAY_T(pstring_ptr_t);
DEF_ARRAY_OP_NOPOINTER(pstring_ptr_t);

DEF_ARRAY_STRUCT0(bitset_t, unsigned);
DEF_ARRAY_T(bitset_t);
DEF_ARRAY_OP(bitset_t);

typedef struct moz_state_t {
    block_t *head;
    block_t *cur;
    block_t *next;
    block_t *fail;
} moz_state_t;

typedef struct compiler {
    moz_runtime_t *R;
    ARRAY(block_ptr_t) blocks;
    ARRAY(decl_ptr_t) decls;
    ARRAY(pstring_ptr_t) strs;
    ARRAY(bitset_t) sets;
} moz_compiler_t;

static STRING_t moz_compiler_get_string(moz_compiler_t *C, ARRAY(uint8_t) *str)
{
    pstring_t **x;
    unsigned i = 0;
    unsigned max = ARRAY_size(C->strs);
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
    return max;
}

static BITSET_t moz_compiler_get_set(moz_compiler_t *C, bitset_t *set)
{
    bitset_t tmp;
    bitset_t *x;
    unsigned i = 0;
    unsigned max = ARRAY_size(C->sets);
    FOR_EACH_ARRAY_(C->sets, x, i) {
        if (bitset_equal(set, x)) {
            return i;
        }
    }
    bitset_copy(&tmp, set);
    ARRAY_add(bitset_t, &C->sets, &tmp);
    return max;

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

#include "ast.c"

/* compile */

static uintptr_t _MAX_IR_ID = 0;

static const char *IR_TYPE_NAME[] = {
#define DEFINE_IR_NAME(NAME) #NAME,
    FOR_EACH_IR(DEFINE_IR_NAME)
#undef DEFINE_IR_NAME
};

static IR_t *_IR_ALLOC(size_t size, ir_type_t type, moz_state_t *S)
{
    IR_t *ir = (IR_t *)VM_CALLOC(1, size);
    ir->id = _MAX_IR_ID++;
    ir->type = type;
    ir->fail = S->fail;
    if (type != IJump) {
        block_link(S->cur, S->fail);
    }
    return ir;
}

#define IR_ALLOC(T, S)   _IR_ALLOC(sizeof(T##_t), T, S)
#define IR_ALLOC_T(T, S) ((T##_t *) IR_ALLOC(T, S))

static block_t *moz_compiler_create_named_block(moz_compiler_t *C, const char *name)
{
    block_t *BB = block_new(name);
    ARRAY_add(block_ptr_t, &C->blocks, BB);
    return BB;
}

static block_t *moz_compiler_create_block(moz_compiler_t *C)
{
    return moz_compiler_create_named_block(C, NULL);
}

static void moz_state_init(moz_compiler_t *C, moz_state_t *S)
{
    S->head = moz_compiler_create_named_block(C, "head");
    S->next = moz_compiler_create_named_block(C, "exit");
    S->fail = moz_compiler_create_named_block(C, "fail");
    S->cur = S->head;
}

static void moz_state_copy(moz_state_t *dst, moz_state_t *src)
{
    dst->head = src->head;
    dst->cur  = src->cur;
    dst->next = src->next;
    dst->fail = src->fail;
}

static void moz_compiler_set_label(moz_compiler_t *C, moz_state_t *S, block_t *BB)
{
    S->cur = BB;
}

// static void moz_compiler_set_fail(moz_compiler_t *C, block_t *BB, block_t *failBB)
// {
//     assert(BB->fail == NULL);
//     block_link(BB, failBB);
//     BB->fail = failBB;
// }

static void moz_compiler_add(moz_compiler_t *C, moz_state_t *S, IR_t *ir)
{
    IR_t *last = block_get_last(S->cur);
    assert(last == NULL || last->type != IJump);
    block_append(S->cur, ir);
}

static void moz_compiler_link(moz_compiler_t *C, moz_state_t *S, block_t *BB1, block_t *BB2)
{
    assert(BB1 != BB2); // In most case, this is bug.
    IJump_t *ir = IR_ALLOC_T(IJump, S);
    ir->v.target = BB2;
    moz_compiler_add(C, S, (IR_t *)ir);
    block_link(BB1, BB2);
    fprintf(stderr, "link BB%d -> BB%d\n", block_id(BB1), block_id(BB2));
}

typedef void (*f_to_ir)(moz_compiler_t *C, moz_state_t *S, expr_t *e);

static void moz_expr_to_ir(moz_compiler_t *C, moz_state_t *S, expr_t *e);

#define TODO(E) do { \
    moz_expr_dump(0, (expr_t *)(E)); \
    assert(0 && "TODO"); \
} while (0)

static void moz_Empty_to_ir(moz_compiler_t *C, moz_state_t *S, Empty_t *e)
{
    /* do nothing */
}

static void moz_Invoke_to_ir(moz_compiler_t *C, moz_state_t *S, Invoke_t *e)
{
    IInvoke_t *ir = IR_ALLOC_T(IInvoke, S);
    ir->v.decl = e->decl;
    moz_compiler_add(C, S, (IR_t *)ir);
}

static void moz_Any_to_ir(moz_compiler_t *C, moz_state_t *S, Any_t *e)
{
    IAny_t *ir = IR_ALLOC_T(IAny, S);
    moz_compiler_add(C, S, (IR_t *)ir);
}

static void moz_Byte_to_ir(moz_compiler_t *C, moz_state_t *S, Byte_t *e)
{
    IByte_t *ir = IR_ALLOC_T(IByte, S);
    ir->byte = e->byte;
    moz_compiler_add(C, S, (IR_t *)ir);
}

static void moz_Str_to_ir(moz_compiler_t *C, moz_state_t *S, Str_t *e)
{
    IStr_t *ir = IR_ALLOC_T(IStr, S);
    ir->strId = moz_compiler_get_string(C, &e->list);
    moz_compiler_add(C, S, (IR_t *)ir);
}

static void moz_Set_to_ir(moz_compiler_t *C, moz_state_t *S, Set_t *e)
{
    ISet_t *ir = IR_ALLOC_T(ISet, S);
    ir->setId = moz_compiler_get_set(C, &e->set);
    moz_compiler_add(C, S, (IR_t *)ir);
}

static void moz_Choice_to_ir(moz_compiler_t *C, moz_state_t *S, Choice_t *e)
{
    /**
     * Choice(E1, E2, E3)
     * L_head
     *  E1, NEXT, next1
     * L_next1
     *  E2, NEXT, next2
     * L_next2
     *  E3, NEXT, FAIL
     */
    unsigned i;
    block_t *blocks[ARRAY_size(e->list) + 1 + 1];
    block_t *next;
    moz_state_t state = {};
    expr_t **x;

    moz_state_copy(&state, S);
    for (i = 0; i < ARRAY_size(e->list); i++) {
        blocks[i] = moz_compiler_create_block(C);
    }
    blocks[i] = S->fail;
    next = moz_compiler_create_block(C);

    moz_compiler_link(C, &state, state.cur, blocks[0]);
    FOR_EACH_ARRAY_(e->list, x, i) {
        state.next = next;
        state.fail = blocks[i + 1];
        moz_compiler_set_label(C, &state, blocks[i]);
        moz_expr_to_ir(C, &state, *x);
        moz_compiler_link(C, &state, state.cur, state.next);
    }
    moz_compiler_set_label(C, S, state.next);
}

static void moz_Fail_to_ir(moz_compiler_t *C, moz_state_t *S, Fail_t *e)
{
    IJump_t *ir = IR_ALLOC_T(IJump, S);
    ir->v.target = S->fail;
    moz_compiler_add(C, S, (IR_t *)ir);
}

static void moz_And_to_ir(moz_compiler_t *C, moz_state_t *S, And_t *e)
{
    /**
     * And(E1)
     * L_head:
     *  PStore
     *  E1, next, fail
     * L_next:
     *  PLoad
     *  goto NEXT
     * L_fail:
     *  goto FAIL;
     */
    moz_state_t state;
    moz_state_copy(&state, S);
    block_t *head = moz_compiler_create_block(C);
    block_t *next = moz_compiler_create_block(C);
    block_t *fail = moz_compiler_create_block(C);

    moz_compiler_link(C, &state, state.cur, head);
    state.next = next;
    state.fail = fail;
    moz_compiler_set_label(C, &state, head);

    moz_compiler_add(C, &state, IR_ALLOC(IPStore, &state));
    moz_expr_to_ir(C, &state, e->expr);
    moz_compiler_link(C, &state, state.cur, state.next);

    moz_compiler_set_label(C, &state, state.next);
    state.fail = S->fail;
    moz_compiler_add(C, &state, IR_ALLOC(IPLoad, &state));
    state.fail = fail;

    moz_compiler_set_label(C, &state, state.fail);
    moz_compiler_link(C, &state, state.fail, S->fail);

    moz_compiler_set_label(C, S, state.next);
}

static void moz_Not_to_ir(moz_compiler_t *C, moz_state_t *S, Not_t *e)
{
    /**
     * Not(E1)
     * L_current:
     *  goto head;
     * L_head:
     *  PStore
     *  E1, next, fail
     * L_next:
     *  goto FAIL
     * L_fail:
     *  PLoad
     *  goto NEXT;
     */
    moz_state_t state;
    moz_state_copy(&state, S);
    block_t *head = moz_compiler_create_block(C);
    block_t *next = moz_compiler_create_block(C);
    block_t *fail = moz_compiler_create_block(C);

    moz_compiler_link(C, &state, state.cur, head);
    state.next = fail;
    state.fail = next;
    moz_compiler_set_label(C, &state, head);

    moz_compiler_add(C, &state, IR_ALLOC(IPStore, &state));
    moz_expr_to_ir(C, &state, e->expr);
    moz_compiler_link(C, &state, state.cur, state.next);

    moz_compiler_set_label(C, &state, state.next);
    moz_compiler_link(C, &state, state.next, S->fail);

    moz_compiler_set_label(C, &state, state.fail);
    {
        block_t *cur_fail = state.fail;
        state.fail = S->fail;
        moz_compiler_add(C, &state, IR_ALLOC(IPLoad, &state));
        state.fail = cur_fail;
    }
    moz_compiler_set_label(C, S, state.fail);
}

static void moz_Option_to_ir(moz_compiler_t *C, moz_state_t *S, Option_t *e)
{
    /**
     * Option(E1)
     * L_head:
     *  E1, next, fail
     * L_next:
     * L_fail:
     *  goto next;
     */
    moz_state_t state = {};
    moz_state_copy(&state, S);
    state.next = state.fail = moz_compiler_create_block(C);
    moz_expr_to_ir(C, &state, e->expr);
    moz_compiler_set_label(C, S, state.fail);
    moz_compiler_link(C, &state, state.fail, state.next);
}

static void moz_Sequence_to_ir(moz_compiler_t *C, moz_state_t *S, Sequence_t *e)
{
    /**
     * Sequence(E1, E2, E3)
     * parent_state = (NEXT, FAIL)
     * L_head:
     *  E1, next1, FAIL
     * L_next1:
     *  E2, next2, FAIL
     * L_next2:
     *  E3, next3, FAIL
     * L_next3:
     *  goto NEXT;
     * PARENT_FAIL:
     */

    unsigned i;
    expr_t **x;
    moz_state_t state = {};
    block_t *blocks[ARRAY_size(e->list) + 1];

    moz_state_copy(&state, S);
    state.next = moz_compiler_create_block(C);

    for (i = 0; i < ARRAY_size(e->list); i++) {
        blocks[i] = moz_compiler_create_block(C);
    }
    FOR_EACH_ARRAY_(e->list, x, i) {
        moz_compiler_link(C, &state, state.cur, blocks[i]);
        moz_compiler_set_label(C, &state, blocks[i]);
        moz_expr_to_ir(C, &state, *x);
    }
    moz_compiler_link(C, &state, state.cur, state.next);
    moz_compiler_set_label(C, S, state.next);
}

static void moz_Repetition_to_ir(moz_compiler_t *C, moz_state_t *S, Repetition_t *e)
{
    /**
     * Repetition(E1, E2, E3)
     * L_head
     *  E1, next1, L_fail
     * L_next1
     *  E2, next2, L_fail
     * L_next2
     *  E3, next3, L_fail
     * L_next3
     *  goto L_head
     * L_fail
     */

    unsigned i;
    block_t *blocks[ARRAY_size(e->list) + 1 + 1];
    moz_state_t state = {};
    expr_t **x;
    block_t *fail;

    moz_state_copy(&state, S);
    for (i = 0; i < ARRAY_size(e->list); i++) {
        blocks[i] = moz_compiler_create_block(C);
    }
    blocks[i] = S->cur;
    fail = moz_compiler_create_block(C);
    moz_compiler_link(C, &state, state.cur, blocks[0]);

    FOR_EACH_ARRAY_(e->list, x, i) {
        moz_compiler_set_label(C, &state, blocks[i]);
        state.fail = fail;
        moz_expr_to_ir(C, &state, *x);
        moz_compiler_link(C, &state, state.cur, blocks[i + 1]);
    }
    moz_compiler_set_label(C, &state, fail);
    moz_compiler_link(C, &state, state.cur, S->fail);
    moz_compiler_set_label(C, S, state.next);
}

static void moz_Tcapture_to_ir(moz_compiler_t *C, moz_state_t *S, Tcapture_t *e)
{
    TODO(e);
}

static void moz_Tdetree_to_ir(moz_compiler_t *C, moz_state_t *S, Tdetree_t *e)
{
    TODO(e);
}

static void moz_Tlfold_to_ir(moz_compiler_t *C, moz_state_t *S, Tlfold_t *e)
{
    TODO(e);
}

static void moz_Tlink_to_ir(moz_compiler_t *C, moz_state_t *S, Tlink_t *e)
{
    TODO(e);
}

static void moz_Tnew_to_ir(moz_compiler_t *C, moz_state_t *S, Tnew_t *e)
{
    TODO(e);
}

static void moz_Treplace_to_ir(moz_compiler_t *C, moz_state_t *S, Treplace_t *e)
{
    TODO(e);
}

static void moz_Ttag_to_ir(moz_compiler_t *C, moz_state_t *S, Ttag_t *e)
{
    ITTag_t *ir = IR_ALLOC_T(ITTag, S);
    ir->tagId = moz_compiler_get_tag(C, &e->name);
    moz_compiler_add(C, S, (IR_t *)ir);
}

static void moz_Xblock_to_ir(moz_compiler_t *C, moz_state_t *S, Xblock_t *e)
{
    TODO(e);
}

static void moz_Xexists_to_ir(moz_compiler_t *C, moz_state_t *S, Xexists_t *e)
{
    TODO(e);
}

static void moz_Xif_to_ir(moz_compiler_t *C, moz_state_t *S, Xif_t *e)
{
    TODO(e);
}

static void moz_Xis_to_ir(moz_compiler_t *C, moz_state_t *S, Xis_t *e)
{
    TODO(e);
}

static void moz_Xisa_to_ir(moz_compiler_t *C, moz_state_t *S, Xisa_t *e)
{
    TODO(e);
}

static void moz_Xon_to_ir(moz_compiler_t *C, moz_state_t *S, Xon_t *e)
{
    TODO(e);
}

static void moz_Xmatch_to_ir(moz_compiler_t *C, moz_state_t *S, Xmatch_t *e)
{
    TODO(e);
}

static void moz_Xlocal_to_ir(moz_compiler_t *C, moz_state_t *S, Xlocal_t *e)
{
    TODO(e);
}

static void moz_Xsymbol_to_ir(moz_compiler_t *C, moz_state_t *S, Xsymbol_t *e)
{
    TODO(e);
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
    /*
     * decl(E1)
     * L_head:
     *  E1
     * L_fail:
     * L_exit:
     */
    moz_state_t state, *S = &state;
    moz_state_init(C, S);
    moz_compiler_set_label(C, S, S->head);
    moz_expr_to_ir(C, S, decl->body);
    moz_compiler_set_label(C, S, S->next);
    moz_compiler_add(C, S, IR_ALLOC(IRet, S));

    moz_compiler_set_label(C, S, S->fail);
    moz_compiler_add(C, S, IR_ALLOC(IFail, S));
}

static void moz_ast_to_ir(moz_compiler_t *C)
{
    decl_t **decl, **e;
    FOR_EACH_ARRAY(C->decls, decl, e) {
        if ((*decl)->refc > 0) {
            moz_decl_to_ir(C, *decl);
        }
    }
}

static void moz_ir_optimize(moz_compiler_t *C)
{
}

static void moz_block_dump(block_t *BB)
{
    block_t **I;
    IR_t **x, **e;
    unsigned i;
    fprintf(stderr, "BB%d", block_id(BB));
    if (BB->name != NULL) {
        fprintf(stderr, "(%s)", BB->name);
    }

    fprintf(stderr, " pred=[");
    FOR_EACH_ARRAY_(BB->preds, I, i) {
        if (i > 0) {
            fprintf(stderr, ", ");
        }
        fprintf(stderr, "BB%d", block_id(*I));
    }
    fprintf(stderr, "]");

    fprintf(stderr, " succ=[");
    FOR_EACH_ARRAY_(BB->succs, I, i) {
        if (i > 0) {
            fprintf(stderr, ", ");
        }
        fprintf(stderr, "BB%d", block_id(*I));
    }
    fprintf(stderr, "]");

    fprintf(stderr, "\n");
    FOR_EACH_ARRAY(BB->insts, x, e) {
        IR_t *ir = *x;
        switch (ir->type) {
        case IJump:
            fprintf(stderr, "  %03d %s BB%d\n", ir->id, IR_TYPE_NAME[ir->type],
                    block_id(((IJump_t *)ir)->v.target));
            break;
        case ILabel:
        case IExit:
        case ITableJump:
        case IRet:
            fprintf(stderr, "  %03d %s\n", ir->id, IR_TYPE_NAME[ir->type]);
            break;
        case IPLoad:
        case IPStore:
        case IFail:
        case IInvoke:
        case IAny:
        case INAny:
        case IByte:
        case IStr:
        case ISet:
        case IUByte:
        case IUSet:
        case IRByte:
        case IRStr:
        case IRSet:
        case IRUByte:
        case IRUSet:
        case ILookup:
        case IMemo:
        case IMemoFail:
        case ITStart:
        case ITCommit:
        case ITAbort:
        case ITPush:
        case ITPop:
        case ITFoldL:
        case ITNew:
        case ITCapture:
        case ITTag:
        case ITReplace:
        case ITLookup:
        case ITMemo:
        case ISOpen:
        case ISClose:
        case ISMask:
        case ISDef:
        case ISIsDef:
        case ISExists:
        case ISMatch:
        case ISIs:
        case ISIsa:
            fprintf(stderr, "  %03d %s fail=BB%d\n", ir->id,
                    IR_TYPE_NAME[ir->type], block_id(ir->fail));
            break;
        default:
            assert(0 && "unreachable");
        }
    }
}

static void moz_ir_dump(moz_compiler_t *C)
{
    block_t **I, **E;
    FOR_EACH_ARRAY(C->blocks, I, E) {
        moz_block_dump(*I);
    }
}

void moz_compiler_compile(const char *output_file, moz_runtime_t *R, Node *node)
{
    unsigned i, decl_index = 0, decl_size = Node_length(node);
    moz_compiler_t C;
    C.R = R;
    ARRAY_init(decl_ptr_t, &C.decls, decl_size);
    ARRAY_init(pstring_ptr_t, &C.strs, 1);
    ARRAY_init(bitset_t, &C.sets, 1);
    ARRAY_init(block_ptr_t, &C.blocks, 1);

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
    moz_ir_dump(&C);
    ARRAY_dispose(block_ptr_t, &C.blocks);
    ARRAY_dispose(pstring_ptr_t, &C.strs);
    ARRAY_dispose(bitset_t, &C.sets);
    ARRAY_dispose(decl_ptr_t, &C.decls);
}

#ifdef __cplusplus
}
#endif
