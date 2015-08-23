#include "memo.h"
#include "karray.h"
#include <stdlib.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

DEF_ARRAY_STRUCT0(MemoEntry_t, unsigned);
DEF_ARRAY_T(MemoEntry_t);
DEF_ARRAY_OP(MemoEntry_t);

struct memo {
    ARRAY(MemoEntry_t) ary;
    unsigned shift;
};

#if defined(MOZVM_MEMO_TYPE_ELASTIC)
static void memo_elastic_init(memo_t *m, unsigned w, unsigned n)
{
    unsigned i;
    unsigned len = w * n + 1;
    ARRAY_init(MemoEntry_t, &m->ary, len);
    MemoEntry_t e = {};
    e.hash = UINTPTR_MAX;
    for (i = 0; i < len; i++) {
        ARRAY_add(MemoEntry_t, &m->ary, &e);
    }
    m->shift = LOG2(n) + 1;
}

static int memo_elastic_set(memo_t *m, const char *pos, uint32_t memoId, Node result, unsigned consumed, int state)
{
    uintptr_t hash = (((uintptr_t)pos << m->shift) | memoId);
    unsigned idx = hash % ARRAY_size(m->ary);
    MemoEntry_t *old = ARRAY_get(MemoEntry_t, &m->ary, idx);
    if (old->failed != UINTPTR_MAX && old->result) {
        NODE_GC_RELEASE(old->result);
    }
    old->hash = hash;
    old->consumed = consumed;
    old->state    = state;
    old->result   = result;
    // ARRAY_set(MemoEntry_t, &m->ary, idx, e);
    return 1;
}

static int memo_elastic_fail(memo_t *m, const char *pos, unsigned id)
{
    uintptr_t hash = (((uintptr_t)pos << m->shift) | id);
    unsigned idx = hash % ARRAY_size(m->ary);
    MemoEntry_t *old = ARRAY_get(MemoEntry_t, &m->ary, idx);
    if (old->failed != UINTPTR_MAX && old->result) {
        NODE_GC_RELEASE(old->result);
    }
    old->failed = UINTPTR_MAX;
    return 0;
}

static MemoEntry_t *memo_elastic_get(memo_t *m, const char *pos, unsigned id, unsigned state)
{
    uintptr_t hash = (((uintptr_t)pos << m->shift) | id);
    unsigned idx = hash % ARRAY_size(m->ary);
    MemoEntry_t *e = ARRAY_get(MemoEntry_t, &m->ary, idx);
    if (e->hash == hash) {
        if (e->state == state) {
            return e;
        }
    }
    return NULL;
}

static void memo_elastic_dispose(memo_t *m)
{
    unsigned i;
    for (i = 0; i < ARRAY_size(m->ary); i++) {
        MemoEntry_t *e = ARRAY_get(MemoEntry_t, &m->ary, i);
        if (e->failed != UINTPTR_MAX && e->result) {
            NODE_GC_RELEASE(e->result);
        }
    }
    ARRAY_dispose(MemoEntry_t, &m->ary);
}

#elif defined(MOZVM_MEMO_TYPE_HASH)
#else  /* MOZVM_MEMO_TYPE_NULL */
static void memo_null_init(memo_t *memo, unsigned w, unsigned n)
{
    /* do nothing */
}

static int memo_null_set(memo_t *m, const char *pos, unsigned id, MemoEntry_t *e)
{
    return 0;
}

static int memo_null_fail(memo_t *m, const char *pos, unsigned id)
{
    return 0;
}

static MemoEntry_t *memo_null_get(memo_t *m, const char *pos, unsigned id, unsigned state)
{
    return NULL;
}

static void memo_null_dispose(memo_t *memo)
{
    /* do nothing */
}
#endif

memo_t *memo_init(unsigned w, unsigned n)
{
    memo_t *memo = (memo_t *)VM_MALLOC(sizeof(*memo));
#if defined(MOZVM_MEMO_TYPE_ELASTIC)
    memo_elastic_init(memo, w, n);
#elif defined(MOZVM_MEMO_TYPE_HASH)
#else  /* MOZVM_MEMO_TYPE_NULL */
    memo_null_init(memo, w, n);
#endif
    return memo;
}

void memo_dispose(memo_t *memo)
{
#if defined(MOZVM_MEMO_TYPE_ELASTIC)
    memo_elastic_dispose(memo);
#elif defined(MOZVM_MEMO_TYPE_HASH)
#else  /* MOZVM_MEMO_TYPE_NULL */
    memo_null_dispose(memo);
#endif
    VM_FREE(memo);
}

MemoEntry_t *memo_get(memo_t *m, const char *pos, uint32_t memoId, uint8_t state)
{
#if defined(MOZVM_MEMO_TYPE_ELASTIC)
    return memo_elastic_get(m, pos, memoId, state);
#elif defined(MOZVM_MEMO_TYPE_HASH)
#else  /* MOZVM_MEMO_TYPE_NULL */
    return memo_null_get(m, pos, memoId, state);
#endif
}

int memo_fail(memo_t *m, const char *pos, uint32_t memoId)
{
#if defined(MOZVM_MEMO_TYPE_ELASTIC)
    return memo_elastic_fail(m, pos, memoId);
#elif defined(MOZVM_MEMO_TYPE_HASH)
#else  /* MOZVM_MEMO_TYPE_NULL */
    return memo_null_fail(m, pos, memoId);
#endif
}

int memo_set(memo_t *m, const char *pos, uint32_t memoId, Node result, unsigned consumed, int state)
{
    if (result) {
        NODE_GC_RETAIN(result);
    }
#if defined(MOZVM_MEMO_TYPE_ELASTIC)
    return memo_elastic_set(m, pos, memoId, result, consumed, state);
#elif defined(MOZVM_MEMO_TYPE_HASH)
    MemoEntry_t e;
    e.consumed = consumed;
    e.state    = state;
    e.result   = result;
#else  /* MOZVM_MEMO_TYPE_NULL */
    return memo_null_set(m, pos, memoId, &e);
#endif
}

// typedef struct memo_stat {
//     unsigned memo_stored;
//     unsigned memo_used;
//     unsigned memo_invalidated;
//     unsigned memo_hit;
//     unsigned memo_failhit;
//     unsigned memo_miss;
// } memo_stat_t;
//
// static memo_stat_t stat = {};
//
// void memo_hit()
// {
//     stat.memo_hit++;
// }
//
// void memo_failhit()
// {
//     stat.memo_failhit++;
// }
//
// void memo_miss()
// {
//     stat.memo_miss++;
// }

#ifdef __cplusplus
}
#endif
