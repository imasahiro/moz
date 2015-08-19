#include <stdint.h>

#ifndef MEMO_H
#define MEMO_H

typedef struct MemoEntry {
    uintptr_t hash;
    uint8_t failed;
    unsigned consumed;
    unsigned state;
    void *result;
} MemoEntry_t;

enum memo_type {
    MEMO_TYPE_NULL,
    MEMO_TYPE_HASH,
    MEMO_TYPE_ELASTIC,
};


struct memo;
typedef struct memo memo_t;

void memo_hit();
void memo_failhit();
void memo_miss();

void memo_dispose(memo_t *memo);
memo_t *memo_init(unsigned w, unsigned n, enum memo_type type);
int memo_set(memo_t *memo, char *pos, uint32_t memoId, int failed, void *result, unsigned consumed, int state);
MemoEntry_t *memo_get(memo_t *memo, char *pos, uint32_t memoId, uint8_t state);

#endif /* end of include guard */
