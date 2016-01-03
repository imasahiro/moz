#include "mozvm.h"
#include "compiler.h"
#include "expression.h"
#include "core/pstring.h"
#include <assert.h>
#include "ir.h"
#include "block.c"
#include "worklist.h"
#include "dump.h"

#ifdef __cplusplus
extern "C" {
#endif

DEF_ARRAY_OP_NOPOINTER(pstring_ptr_t);
DEF_ARRAY_OP(bitset_t);

DEF_ARRAY_OP_NOPOINTER(decl_ptr_t);
DEF_ARRAY_OP_NOPOINTER(expr_ptr_t);

#define OPTIMIZE /*optimize annotation*/

typedef struct moz_state_t {
    struct block_t *head;
    struct block_t *cur;
    struct block_t *next;
    struct block_t *fail;
} moz_state_t;

static unsigned moz_compiler_add_string_id(ARRAY(pstring_ptr_t) *ary, name_t *name)
{
    pstring_t **x;
    unsigned i = 0;
    unsigned max = ARRAY_size(*ary);
    FOR_EACH_ARRAY_(*ary, x, i) {
        if ((*x)->len == name->len) {
            if (strncmp((*x)->str, name->str, name->len) == 0) {
                return i;
            }
        }
    }
    const char *newStr = pstring_alloc(name->str, name->len);
    ARRAY_add(pstring_ptr_t, ary, pstring_get_raw(newStr));
    return max;
}
static void moz_ir_dump(moz_compiler_t *C);

static STRING_t moz_compiler_add_string(moz_compiler_t *C, ARRAY(uint8_t) *str)
{
    name_t name;
    name.str = (char *)ARRAY_BEGIN(*str);
    name.len = ARRAY_size(*str);
    return moz_compiler_add_string_id(&C->strs, &name);
}

static TAG_t moz_compiler_add_tag(moz_compiler_t *C, name_t *name)
{
    return moz_compiler_add_string_id(&C->tags, name);
}

static BITSET_t moz_compiler_add_set(moz_compiler_t *C, bitset_t *set)
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
    if (S->fail && !block_is(S->fail, BLOCK_HANDLER)) {
        block_set_type(S->fail, BLOCK_HANDLER);
    }
    return ir;
}

#define IR_ALLOC(T, S)   _IR_ALLOC(sizeof(T##_t), T, S)
#define IR_ALLOC_T(T, S) ((T##_t *) IR_ALLOC(T, S))

static block_t *moz_compiler_create_named_block(moz_compiler_t *C, enum block_type type)
{
    block_t *BB = block_new(type);
    ARRAY_add(block_ptr_t, &C->blocks, BB);
    return BB;
}

static block_t *moz_compiler_create_block(moz_compiler_t *C)
{
    return moz_compiler_create_named_block(C, BLOCK_DEFAULT);
}

static void moz_state_init(moz_compiler_t *C, moz_state_t *S)
{
    S->head = moz_compiler_create_named_block(C, BLOCK_ENTRY);
    S->next = moz_compiler_create_named_block(C, BLOCK_EXIT);
    S->fail = moz_compiler_create_named_block(C, BLOCK_FAIL);
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
    // fprintf(stderr, "set label BB%d -> BB%d\n", block_id(S->cur), block_id(BB));
    S->cur = BB;
}

static void moz_compiler_add(moz_compiler_t *C, moz_state_t *S, IR_t *ir)
{
    IR_t *last = block_get_last(S->cur);
    assert(last == NULL || last->type != IJump);
    block_append(S->cur, ir);
    (void)last;
}

static void moz_compiler_link(moz_compiler_t *C, moz_state_t *S, block_t *BB1, block_t *BB2)
{
    assert(BB1 != BB2); // In most case, this is bug.
    IJump_t *ir = IR_ALLOC_T(IJump, S);
    ir->v.target = BB2;
    moz_compiler_add(C, S, (IR_t *)ir);
    block_link(BB1, BB2);
    // fprintf(stderr, "link BB%d -> BB%d\n", block_id(BB1), block_id(BB2));
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
    ir->strId = moz_compiler_add_string(C, &e->list);
    moz_compiler_add(C, S, (IR_t *)ir);
}

static void moz_Set_to_ir(moz_compiler_t *C, moz_state_t *S, Set_t *e)
{
    ISet_t *ir = IR_ALLOC_T(ISet, S);
    ir->setId = moz_compiler_add_set(C, &e->set);
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

OPTIMIZE static void moz_Not_Any_to_NAny(moz_compiler_t *C, moz_state_t *S)
{
    INAny_t *ir = IR_ALLOC_T(INAny, S);
    moz_compiler_add(C, S, (IR_t *)ir);
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

    if (e->expr->type == Any) {
        moz_Not_Any_to_NAny(C, S);
        return;
    }
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

OPTIMIZE static void moz_Option_to_OSet(moz_compiler_t *C, moz_state_t *S, Set_t *e)
{
    IOSet_t *ir = IR_ALLOC_T(IOSet, S);
    ir->setId = moz_compiler_add_set(C, &e->set);
    moz_compiler_add(C, S, (IR_t *)ir);
}

OPTIMIZE static void moz_Option_to_OStr(moz_compiler_t *C, moz_state_t *S, Str_t *e)
{
    IOStr_t *ir = IR_ALLOC_T(IOStr, S);
    ir->strId = moz_compiler_add_string(C, &e->list);
    moz_compiler_add(C, S, (IR_t *)ir);
}

OPTIMIZE static void moz_Option_to_OByte(moz_compiler_t *C, moz_state_t *S, Byte_t *e)
{
    IOByte_t *ir = IR_ALLOC_T(IOByte, S);
    ir->byte = e->byte;
    moz_compiler_add(C, S, (IR_t *)ir);
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
    switch (e->expr->type) {
    case Set:
        moz_Option_to_OSet(C, S, (Set_t *)e->expr);
        return;
    case Byte:
        moz_Option_to_OByte(C, S, (Byte_t *)e->expr);
        return;
    case Str:
        moz_Option_to_OStr(C, S, (Str_t *)e->expr);
        return;
    default:
        break;
    }
    moz_state_copy(&state, S);
    state.next = state.fail = moz_compiler_create_block(C);
    moz_expr_to_ir(C, &state, e->expr);
    moz_compiler_link(C, &state, state.cur, state.fail);
    moz_compiler_set_label(C, S, state.fail);
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

OPTIMIZE static void moz_Repetition_to_RSet(moz_compiler_t *C, moz_state_t *S, Set_t *e)
{
    IRSet_t *ir = IR_ALLOC_T(IRSet, S);
    ir->setId = moz_compiler_add_set(C, &e->set);
    moz_compiler_add(C, S, (IR_t *)ir);
}

OPTIMIZE static void moz_Repetition_to_RStr(moz_compiler_t *C, moz_state_t *S, Str_t *e)
{
    IRStr_t *ir = IR_ALLOC_T(IRStr, S);
    ir->strId = moz_compiler_add_string(C, &e->list);
    moz_compiler_add(C, S, (IR_t *)ir);
}

OPTIMIZE static void moz_Repetition_to_RByte(moz_compiler_t *C, moz_state_t *S, Byte_t *e)
{
    IRByte_t *ir = IR_ALLOC_T(IRByte, S);
    ir->byte = e->byte;
    moz_compiler_add(C, S, (IR_t *)ir);
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
    if (ARRAY_size(e->list) == 1) {
        expr_t *expr = ARRAY_get(expr_ptr_t, &e->list, 0);
        switch (expr->type) {
        case Set:
            moz_Repetition_to_RSet(C, S, (Set_t *)expr);
            return;
        case Byte:
            moz_Repetition_to_RByte(C, S, (Byte_t *)expr);
            return;
        case Str:
            moz_Repetition_to_RStr(C, S, (Str_t *)expr);
            return;
        default:
            break;
        }
    }

    moz_state_copy(&state, S);
    for (i = 0; i < ARRAY_size(e->list); i++) {
        blocks[i] = moz_compiler_create_block(C);
    }
    blocks[i] = S->cur;
    fail = state.fail = moz_compiler_create_block(C);
    moz_compiler_link(C, &state, state.cur, blocks[0]);

    FOR_EACH_ARRAY_(e->list, x, i) {
        moz_compiler_set_label(C, &state, blocks[i]);
        state.next = blocks[i + 1];
        state.fail = fail;
        moz_expr_to_ir(C, &state, *x);
        moz_compiler_link(C, &state, state.cur, blocks[i + 1]);
    }
    moz_compiler_set_label(C, S, fail);
}

static void moz_Tcapture_to_ir(moz_compiler_t *C, moz_state_t *S, Tcapture_t *e)
{
    ITCapture_t *ir = IR_ALLOC_T(ITCapture, S);
    moz_compiler_add(C, S, (IR_t *)ir);
}

static void moz_Tdetree_to_ir(moz_compiler_t *C, moz_state_t *S, Tdetree_t *e)
{
    TODO(e);
}

static void moz_Tlfold_to_ir(moz_compiler_t *C, moz_state_t *S, Tlfold_t *e)
{
    TODO(e);
}

static void moz_Tpush_to_ir(moz_compiler_t *C, moz_state_t *S, Tpush_t *e)
{
    ITPush_t *ir = IR_ALLOC_T(ITPush, S);
    moz_compiler_add(C, S, (IR_t *)ir);
}

static void moz_Tpop_to_ir(moz_compiler_t *C, moz_state_t *S, Tpop_t *e)
{
    ITPop_t *ir = IR_ALLOC_T(ITPop, S);
    ir->tagId = moz_compiler_add_tag(C, &e->name);
    moz_compiler_add(C, S, (IR_t *)ir);
}

static void moz_Tnew_to_ir(moz_compiler_t *C, moz_state_t *S, Tnew_t *e)
{
    ITNew_t *ir = IR_ALLOC_T(ITNew, S);
    moz_compiler_add(C, S, (IR_t *)ir);
}

static void moz_Treplace_to_ir(moz_compiler_t *C, moz_state_t *S, Treplace_t *e)
{
    TODO(e);
}

static void moz_Ttag_to_ir(moz_compiler_t *C, moz_state_t *S, Ttag_t *e)
{
    ITTag_t *ir = IR_ALLOC_T(ITTag, S);
    ir->tagId = moz_compiler_add_tag(C, &e->name);
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
#define F_IR_DECL(NAME, DUMP, OPT, SWEEP) (f_to_ir) moz_##NAME##_to_ir,
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

    moz_compiler_link(C, S, S->cur, S->next);
    moz_compiler_set_label(C, S, S->next);
    moz_compiler_add(C, S, IR_ALLOC(IRet, S));

    moz_compiler_set_label(C, S, S->fail);
    moz_compiler_add(C, S, IR_ALLOC(IFail, S));
    decl->inst = S->head;
}

static void moz_ast_to_ir(moz_compiler_t *C)
{
    decl_t **decl, **e;
    FOR_EACH_ARRAY(C->decls, decl, e) {
        if (MOZ_RC_COUNT(*decl) > 0) {
            moz_decl_to_ir(C, *decl);
        }
    }
}

typedef moz_compiler_t *moz_compiler_ptr_t;
DEF_WORK_LIST_TYPES(block_ptr_t, moz_compiler_ptr_t);
DEF_WORK_LIST_OP(block_ptr_t, moz_compiler_ptr_t);

static IR_t *block_get_terminator(block_t *bb)
{
    IR_t *ir;
    if ((ir = block_get_last(bb)) != NULL) {
        switch (ir->type) {
        case IExit:
        case IJump:
        case ITableJump:
        case IRet:
        case IFail:
            return ir;
        default:
            break;
        }
    }
    return NULL;
}

static void remove_from_parent(moz_compiler_t *C, IR_t *inst, int do_free)
{
    block_t *BB = inst->parent;
    block_remove(BB, inst);
    if (do_free) {
        memset(inst, 0, sizeof(*inst));
        VM_FREE(inst);
    }
}

static int simplify_cfg(WORK_LIST(block_ptr_t, moz_compiler_ptr_t) *W, block_t *bb)
{
    moz_compiler_t *C = W->context;
    IR_t *inst;
    block_ptr_t *I, *E;

    if (ARRAY_size(bb->insts) == 0) {
        // Skip this block because this block seems already removed.
        return 0;
    }
    if (block_is(bb, BLOCK_ENTRY) || block_is(bb, BLOCK_FAIL) ||
            block_is(bb, BLOCK_HANDLER)) {
        // Skip special block such as entry block, fail block, handler block.
        return 0;
    }
    FOR_EACH_ARRAY(bb->preds, I, E) {
        // This block seems loop. Skip this block.
        if (*I == bb) {
            return 0;
        }
    }
    // fprintf(stderr, "cfg BB%d\n", bb->id);

    inst = block_get_terminator(bb);

    if (ARRAY_size(bb->succs) == 1) {
        /* Remove indirect jump
         * [Before]         |[After]
         * pred: INST1      | pred: INST1
         *       ...        |       ...
         *       IJump BB   |       INST2
         * BB  : INST2      |       IJump succ
         *       IJump succ | succ: INST3
         * succ: INST3      |      ...
         *      ...         |
         */

        block_t *succ = ARRAY_get(block_ptr_t, &bb->succs, 0);
        if (inst->type == IJump) {
            IR_t **x, **e;
            int preds_size = ARRAY_size(bb->preds);
            int modified = 0;

            FOR_EACH_ARRAY_R(bb->preds, I, E) {
                block_t *pred = *I;
                IR_t *term;

                if (pred == bb) {
                    continue;
                }
                term = block_get_terminator(pred);
                if (term == NULL || term->type != IJump) {
                    continue;
                }
                FOR_EACH_ARRAY(bb->insts, x, e) {
                    if (*x != inst) {
                        block_insert_before(pred, term, *x);
                    }
                }
                ((IJump_t *)term)->v.target = succ;
                block_set_type(pred, bb->type);
                block_unlink(pred, bb);
                block_link(pred, succ);
                WORK_LIST_push(block_ptr_t, moz_compiler_ptr_t, W, pred);
                modified++;
            }
            if (modified == preds_size) {
                block_unlink(bb, succ);
                remove_from_parent(C, inst, 1);
                ARRAY_size(bb->insts) = 0;
                ARRAY_remove_element(block_ptr_t, &C->blocks, bb);
            }
            if (modified > 0) {
                WORK_LIST_push(block_ptr_t, moz_compiler_ptr_t, W, succ);
                return 1;
            }
        }
    }

    if (inst && inst->type == IRet) {
        /* Merge exit block to predecessor block
         * [Before]         |[After]
         * pred: INST1      | pred: INST1
         *       ...        |       ...
         *       IJump BB   |       IRet
         * BB  : IRet       |
         */
        int preds_size = ARRAY_size(bb->preds);
        int modified = 0;

        assert(ARRAY_size(bb->succs) == 0 && block_is(bb, BLOCK_EXIT));
        FOR_EACH_ARRAY_R(bb->preds, I, E) {
            block_t *pred = *I;
            IR_t **x, **e;
            IR_t *term = block_get_terminator(pred);
            if (term == NULL || term->type != IJump) {
                continue;
            }
            FOR_EACH_ARRAY(bb->insts, x, e) {
                block_insert_before(pred, term, *x);
            }
            remove_from_parent(C, term, 1);
            block_set_type(pred, bb->type);
            block_unlink(pred, bb);
            WORK_LIST_push(block_ptr_t, moz_compiler_ptr_t, W, pred);
        }
        if (modified == preds_size) {
            ARRAY_remove_element(block_ptr_t, &C->blocks, bb);
        }
        if (modified > 0) {
            return 1;
        }
    }
    return 0;
}

static int remove_indirect_jump_handler(WORK_LIST(block_ptr_t, moz_compiler_ptr_t) *W, block_t *bb)
{
    moz_compiler_t *C = W->context;

    assert(block_is(bb, BLOCK_HANDLER));
    if (ARRAY_size(bb->succs) == 1) {
        IR_t *inst = block_get_terminator(bb);
        block_t *succ = ARRAY_get(block_ptr_t, &bb->succs, 0);
        block_ptr_t *I, *E;

        /* Remove a handler that only have an indirect jump.
         * [Before]             | [After]
         *       INST1, fail=BB |       INST1, fail=succ
         *       ...            |       ...
         * BB  : IJump succ     |       IJump succ
         * succ: INST3          | succ: INST3
         *       ...            |      ...
         */
        if (ARRAY_size(bb->insts) != 1 || inst->type != IJump ||
                ARRAY_size(succ->preds) != 1) {
            return 0;
        }
        block_unlink(bb, succ);
        remove_from_parent(C, inst, 1);
        ARRAY_size(bb->insts) = 0;
        ARRAY_remove_element(block_ptr_t, &C->blocks, bb);
        FOR_EACH_ARRAY(C->blocks, I, E) {
            IR_t **x, **e;
            FOR_EACH_ARRAY((*I)->insts, x, e) {
                if ((*x)->fail == bb) {
                    (*x)->fail = succ;
                }
            }
        }
        WORK_LIST_push(block_ptr_t, moz_compiler_ptr_t, W, succ);
        return 1;
    }
    return 0;
}

static void add_all_blocks(WORK_LIST(block_ptr_t, moz_compiler_ptr_t) *W, moz_compiler_t *C)
{
    block_t **I, **E;
    FOR_EACH_ARRAY(C->blocks, I, E) {
        WORK_LIST_push(block_ptr_t, moz_compiler_ptr_t, W, *I);
    }
}

static void add_handler_block(WORK_LIST(block_ptr_t, moz_compiler_ptr_t) *W, moz_compiler_t *C)
{
    block_t **I, **E;
    FOR_EACH_ARRAY(C->blocks, I, E) {
        if (block_is(*I, BLOCK_HANDLER)) {
            WORK_LIST_push(block_ptr_t, moz_compiler_ptr_t, W, *I);
        }
    }
}


static void moz_ir_optimize(moz_compiler_t *C)
{
    int modified = 1;
    block_t **I, **E;
    FOR_EACH_ARRAY(C->blocks, I, E) {
        assert(block_get_terminator(*I) != NULL);
    }
    while (modified) {
        modified = 0;
        modified += WORK_LIST_apply(block_ptr_t, moz_compiler_ptr_t, C, add_all_blocks, simplify_cfg);
        modified += WORK_LIST_apply(block_ptr_t, moz_compiler_ptr_t, C, add_handler_block, remove_indirect_jump_handler);
    }
}

static void moz_inst_header_dump(IR_t *ir, int fail_block, int line_feed)
{
    fprintf(stderr, "  %03d %s", ir->id, IR_TYPE_NAME[ir->type]);
    if (fail_block) {
        fprintf(stderr, " fail=BB%d", block_id(ir->fail));
    }
    if (line_feed) {
        fprintf(stderr, "\n");
    }
}

static void moz_inst_dump(moz_compiler_t *C, IR_t *ir)
{
    char buf[128] = {};
    switch (ir->type) {
    case IJump:
        moz_inst_header_dump(ir, 0, 0);
        fprintf(stderr, " BB%d\n", block_id(((IJump_t *)ir)->v.target));
        break;
    case ITableJump:
    case ILabel:
    case IExit:
    case IRet:
    case IPLoad:
    case IPStore:
        moz_inst_header_dump(ir, 0, 1);
        break;
    case IFail:
        moz_inst_header_dump(ir, 1, 1);
        break;
    case IInvoke:
        moz_inst_header_dump(ir, 1, 0);
        fprintf(stderr, " decl=BB%d(%.*s)\n",
                block_id(((IInvoke_t *)ir)->v.decl->inst),
                ((IInvoke_t *)ir)->v.decl->name.len,
                ((IInvoke_t *)ir)->v.decl->name.str);
        break;
    case IByte:
    case IRByte:
    case IOByte:
        moz_inst_header_dump(ir, ir->type == IByte, 0);
        write_char(buf, ((IByte_t *)ir)->byte);
        fprintf(stderr, " byte='%s'\n", buf);
        break;

    case IStr:
    case IRStr:
    case IOStr:
        moz_inst_header_dump(ir, ir->type == IStr, 0);
        fprintf(stderr, " str='%s'\n",
                ARRAY_get(pstring_ptr_t, &C->strs, ((IStr_t *)ir)->strId)->str);
        break;
    case ISet:
    case IRSet:
    case IOSet:
        moz_inst_header_dump(ir, ir->type == ISet, 0);
        dump_set(ARRAY_get(bitset_t, &C->sets, ((ISet_t *)ir)->setId), buf);
        fprintf(stderr, " set=%s\n", buf);
        break;
    case ITTag:
    case ITPop:
        moz_inst_header_dump(ir, 0, 0);
        fprintf(stderr, " tag='%s'\n",
                ARRAY_get(pstring_ptr_t, &C->tags, ((ITTag_t *)ir)->tagId)->str);
        break;
    case IUSet:
    case IUByte:
    case IRUByte:
    case IRUSet:
    case IOUByte:
    case IOUSet:

    case IAny:
    case INAny:
    case ILookup:
    case IMemo:
    case IMemoFail:
    case ITStart:
    case ITCommit:
    case ITAbort:
    case ITPush:
    case ITFoldL:
    case ITNew:
    case ITCapture:
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
        moz_inst_header_dump(ir, 0, 1);
        break;
    default:
        assert(0 && "unreachable");
    }
}

static void moz_block_dump(moz_compiler_t *C, block_t *BB)
{
    block_t **I;
    IR_t **x, **e;
    unsigned i;
    fprintf(stderr, "BB%d", block_id(BB));
    if (block_is(BB, BLOCK_ENTRY)) {
        fprintf(stderr, "|entry");
    }
    if (block_is(BB, BLOCK_EXIT)) {
        fprintf(stderr, "|exit");
    }
    if (block_is(BB, BLOCK_FAIL)) {
        fprintf(stderr, "|fail");
    }
    if (block_is(BB, BLOCK_HANDLER)) {
        fprintf(stderr, "|handler");
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
        moz_inst_dump(C, *x);
    }
}

static void moz_ir_dump(moz_compiler_t *C)
{
    block_t **I, **E;
    fprintf(stderr, "=================\n");
    FOR_EACH_ARRAY(C->blocks, I, E) {
        moz_block_dump(C, *I);
    }
    fprintf(stderr, "=================\n");
}

moz_compiler_t *moz_compiler_init(moz_compiler_t *C, moz_runtime_t *R)
{
    C->R = R;
    ARRAY_init(decl_ptr_t, &C->decls, 1);
    ARRAY_init(pstring_ptr_t, &C->strs, 1);
    ARRAY_init(pstring_ptr_t, &C->tags, 1);
    ARRAY_init(bitset_t, &C->sets, 1);
    ARRAY_init(block_ptr_t, &C->blocks, 1);
    return C;
}

void moz_compiler_dispose(moz_compiler_t *C)
{
    ARRAY_dispose(block_ptr_t, &C->blocks);
    ARRAY_dispose(pstring_ptr_t, &C->strs);
    ARRAY_dispose(pstring_ptr_t, &C->tags);
    ARRAY_dispose(bitset_t, &C->sets);
    ARRAY_dispose(decl_ptr_t, &C->decls);
}

void moz_compiler_compile(moz_runtime_t *R, Node *node)
{
    moz_compiler_t C;
    moz_compiler_init(&C, R);
    moz_node_to_ast(&C, node);
    moz_ast_dump(&C);
    moz_ast_to_ir(&C);
    moz_ir_optimize(&C);
    moz_ir_dump(&C);
    moz_compiler_dispose(&C);
}

#ifdef __cplusplus
}
#endif
