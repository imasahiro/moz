#include <stdint.h>
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

enum memo_type {
    MEMO_TYPE_NULL,
    MEMO_TYPE_HASH,
    MEMO_TYPE_ELASTIC,
};


struct memo;
typedef struct memo memo_t;

// void memo_hit();
// void memo_failhit();
// void memo_miss();

void memo_dispose(memo_t *memo);
memo_t *memo_init(unsigned w, unsigned n, enum memo_type type);
int memo_set(memo_t *memo, char *pos, uint32_t memoId, Node n, unsigned consumed, int state);
int memo_fail(memo_t *memo, char *pos, uint32_t memoId);
MemoEntry_t *memo_get(memo_t *memo, char *pos, uint32_t memoId, uint8_t state);

#endif /* end of include guard */
