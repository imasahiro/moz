#include "memo.h"
#include <stdio.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

// typedef struct MemoEntry {
//     uint8_t failed;
//     uint32_t consumed;
//     void *result;
// } MemoEntry_t;

typedef struct memo {
} memo_t;

typedef struct memo_stat {
    unsigned memo_stored;
    unsigned memo_used;
    unsigned memo_invalidated;
    unsigned memo_hit;
    unsigned memo_failhit;
    unsigned memo_miss;
} memo_stat_t;

static memo_stat_t stat = {};

memo_t *memo_init(unsigned init_size)
{
    assert(0 && "not implemented");
    return NULL;
}

void memo_dispose(memo_t *memo)
{
    assert(0 && "not implemented");
}

MemoEntry_t *memo_get(uint32_t memoId, uint8_t state)
{
    assert(0 && "not implemented");
    return NULL;
}

void memo_set(char *pos, uint32_t memoId, int failed, void *result, unsigned consumed, int state)
{
    assert(0 && "not implemented");
}

void memo_hit()
{
    stat.memo_hit++;
}

void memo_failhit()
{
    stat.memo_failhit++;
}

void memo_miss()
{
    stat.memo_miss++;
}

#ifdef DEBUG
int main(int argc, char const* argv[])
{
    memo_t *memo = memo_init(12);
    memo_dispose(memo);
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif
