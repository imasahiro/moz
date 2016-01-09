#include "compiler.h"

#ifndef IR_BLOCK_H
#define IR_BLOCK_H

typedef struct block_t block_t;
typedef struct IR *IR_ptr_t;

DEF_ARRAY_STRUCT0(IR_ptr_t, unsigned);
DEF_ARRAY_T(IR_ptr_t);
DEF_ARRAY_OP_NOPOINTER(IR_ptr_t);

DEF_ARRAY_OP_NOPOINTER(block_ptr_t);

enum block_type {
    BLOCK_DEFAULT = 1 << 0,
    BLOCK_ENTRY   = 1 << 1,
    BLOCK_EXIT    = 1 << 2,
    BLOCK_FAIL    = 1 << 3,
    BLOCK_HANDLER = 1 << 4,
    BLOCK_LOOP_HEAD = 1 << 5,
    BLOCK_DELETED = -1
};

struct block_t {
    unsigned id;
    enum block_type type;
    ARRAY(IR_ptr_t) insts;
    ARRAY(block_ptr_t) preds;
    ARRAY(block_ptr_t) succs;
};

#endif /* end of include guard */
