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
#define MEMO_ENTRY_FAILED UINTPTR_MAX
struct memo;
typedef struct memo memo_t;

memo_t *memo_init(unsigned w, unsigned n);
void memo_dispose(memo_t *memo);
void memo_print_stats();

int memo_set(memo_t *memo, mozpos_t pos, uint32_t memoId, Node n, unsigned consumed, int state);
int memo_fail(memo_t *memo, mozpos_t pos, uint32_t memoId);
MemoEntry_t *memo_get(memo_t *memo, mozpos_t pos, uint32_t memoId, uint8_t state);

#endif /* end of include guard */
