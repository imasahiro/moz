#include "module.h"
#include "core/karray.h"

#ifdef __cplusplus
extern "C" {
#endif

DEF_ARRAY_S_T(mozaddr_t);
DEF_ARRAY_OP_NOPOINTER(mozaddr_t);

typedef struct mozlinker_t {
    mozaddr_t *address_head;
    mozaddr_t *address_tail;
    unsigned address_size;
    ARRAY(mozaddr_t) labels;
    ARRAY(mozaddr_t) targets;
} mozlinker_t;

static void mozlinker_init(mozlinker_t *linker, mozaddr_t *address_head, mozaddr_t *address_tail, unsigned address_size)
{
    linker->address_head = address_head;
    linker->address_tail = address_tail;
    linker->address_size = address_size;
    ARRAY_init(mozaddr_t, &linker->labels, 1);
    ARRAY_init(mozaddr_t, &linker->targets, 1);
}

static void mozlinker_dispose(mozlinker_t *linker)
{
    ARRAY_dispose(mozaddr_t, &linker->labels);
    ARRAY_dispose(mozaddr_t, &linker->targets);
}

static void mozlinker_add_label(mozlinker_t *linker, moz_buffer_writer_t *W, block_t *target, unsigned calleeId)
{
    IR_t *first = ARRAY_get(IR_ptr_t, &target->insts, 0);
    ARRAY_add(mozaddr_t, &linker->labels, moz_buffer_writer_length(W));
    ARRAY_add(mozaddr_t, &linker->targets, first->id);
    moz_buffer_writer_write32(W, calleeId);
}

static void mozlinker_dump_address(mozlinker_t *linker)
{
    unsigned i;
    for (i = 0; i < linker->address_size; i++) {
        mozaddr_t head = linker->address_head[i];
        mozaddr_t tail = linker->address_tail[i];
        fprintf(stderr, "%3d (head,tail)=(%4d,%4d)\n", i, head, tail);
    }
}

static void mozlinker_resolve(mozlinker_t *linker, uint8_t *code)
{
    unsigned i;
    assert(ARRAY_size(linker->labels) == ARRAY_size(linker->targets));
    mozlinker_dump_address(linker);
    for (i = 0; i < ARRAY_size(linker->labels); i++) {
        /*
         * bytecode format
         * ---+-------------+-----------------+-----+-----------------+---
         * ...|bytecode type|bytecode operands|label|bytecode operands|...
         * ---+-------------+-----------------+-----+-----------------+---
         *    ^                                ^^^^^                  ^
         *    |                                                       |
         *    +- head                                          tail --+
         */
        mozaddr_t labelOffset = *ARRAY_n(linker->labels, i);
        mozaddr_t targetId = *ARRAY_n(linker->targets, i);
        int callerId = *(int *)(code + labelOffset);
        fprintf(stderr, "labelOffset:%d, targetId:%d, callerId:%d\n",
                labelOffset, targetId, callerId);
        mozaddr_t *addr = (mozaddr_t *)(code + labelOffset);
        moz_inst_t *callee_addr_head = code + linker->address_head[targetId];
        moz_inst_t *caller_addr_tail = code + linker->address_tail[callerId];
        *addr = (mozaddr_t)(intptr_t) (callee_addr_head - caller_addr_tail);
    }
}

#ifdef __cplusplus
}
#endif
