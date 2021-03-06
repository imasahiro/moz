#include "block.h"

static unsigned max_block_id = 0;

static void block_set_type(block_t *bb, enum block_type type)
{
    bb->type |= type;
}

static int block_is(block_t *bb, enum block_type type)
{
    return (bb->type & type) == type;
}

static block_t *block_new(enum block_type type)
{
    block_t *bb = (block_t *)VM_CALLOC(1, sizeof(*bb));
    bb->id = max_block_id++;
    block_set_type(bb, type);
    ARRAY_init(IR_ptr_t, &bb->insts, 0);
    ARRAY_init(block_ptr_t, &bb->preds, 0);
    ARRAY_init(block_ptr_t, &bb->succs, 0);
    return bb;
}

static int block_id(block_t *bb)
{
    return bb->id;
}

static unsigned block_size(block_t *bb)
{
    return ARRAY_size(bb->insts);
}

// static void block_delete(block_t *bb)
// {
//     ARRAY_dispose(IR_ptr_t, &bb->insts);
//     ARRAY_dispose(block_ptr_t, &bb->preds);
//     ARRAY_dispose(block_ptr_t, &bb->succs);
//     VM_FREE(bb);
// }

static void block_link(block_t *pred, block_t *succ)
{
    ARRAY_add_once(block_ptr_t, &pred->succs, succ);
    ARRAY_add_once(block_ptr_t, &succ->preds, pred);
}

static void block_unlink(block_t *pred, block_t *succ)
{
    ARRAY_remove_element(block_ptr_t, &pred->succs, succ);
    ARRAY_remove_element(block_ptr_t, &succ->preds, pred);
}

static void block_append(block_t *bb, IR_t *inst)
{
    ARRAY_add(IR_ptr_t, &bb->insts, inst);
    inst->parent = bb;
}

static void block_remove(block_t *bb, IR_t *inst)
{
    ARRAY_remove_element(IR_ptr_t, &bb->insts, inst);
}

static void block_insert_before(block_t *bb, IR_t *target, IR_t *inst)
{
    int idx;
    assert(target != NULL && inst != NULL);
    idx = ARRAY_index(IR_ptr_t, &bb->insts, target);
    assert(idx >= 0);
    ARRAY_insert(IR_ptr_t, &bb->insts, idx, inst);
}

static IR_t *block_get(block_t *bb, int i)
{
    return ARRAY_get(IR_ptr_t, &bb->insts, i);
}

static IR_t *block_get_last(block_t *bb)
{
    unsigned size = block_size(bb);
    if (size == 0) {
        return NULL;
    }
    return block_get(bb, size - 1);
}
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
