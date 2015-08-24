#include <stdint.h>
#include "mozvm_config.h"
#include "node.h"

#ifndef MEMO_H
#define MEMO_H

typedef struct MemoEntry {
    uintptr_t hash;
    union {
        Node result;
        uintptr_t failed;
    };
    unsigned consumed;
    unsigned state;
} MemoEntry_t;

struct memo;
typedef struct memo memo_t;

// void memo_hit();
// void memo_failhit();
// void memo_miss();

void memo_dispose(memo_t *memo);
memo_t *memo_init(unsigned w, unsigned n);
int memo_set(memo_t *memo, mozpos_t pos, uint32_t memoId, Node n, unsigned consumed, int state);
int memo_fail(memo_t *memo, mozpos_t pos, uint32_t memoId);
MemoEntry_t *memo_get(memo_t *memo, mozpos_t pos, uint32_t memoId, uint8_t state);

#endif /* end of include guard */
