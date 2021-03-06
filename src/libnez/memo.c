#include <stdlib.h>
#include <assert.h>
#ifdef MOZVM_PROFILE
#include <stdio.h>
#endif

#include "memo.h"
#include "core/karray.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOZVM_MEMO_PROFILE_EACH(F) \
    F(MEMO_GET)     \
    F(MEMO_MISS)    \
    F(MEMO_HIT)     \
    F(MEMO_HITFAIL) \
    F(MEMO_SET)     \
    F(MEMO_FAIL)

MOZVM_MEMO_PROFILE_EACH(MOZVM_PROFILE_DECL);

DEF_ARRAY_STRUCT0(MemoEntry_t, unsigned);
DEF_ARRAY_T(MemoEntry_t);
DEF_ARRAY_OP(MemoEntry_t);

struct memo {
    ARRAY(MemoEntry_t) ary;
    unsigned shift;
    unsigned mask;
};

#if defined(MOZVM_MEMO_TYPE_ELASTIC)
static void memo_elastic_init(memo_t *m, unsigned w, unsigned n)
{
    unsigned i;
    unsigned len = w * (1 << LOG2(n));
    ARRAY_init(MemoEntry_t, &m->ary, len);
    MemoEntry_t e;;
    e.hash = 0;
    e.result = NULL;
    e.consumed = 0;
    e.state = 0;

    for (i = 0; i < len; i++) {
        ARRAY_add(MemoEntry_t, &m->ary, &e);
    }
    m->mask  = len - 1;
    m->shift = LOG2(n) + 1;
}

static int memo_elastic_set(memo_t *m, mozpos_t pos, uint32_t memoId, Node *result, unsigned consumed, int state)
{
    uintptr_t hash = (((uintptr_t)pos << m->shift) | memoId);
    unsigned idx = hash & m->mask;
    MemoEntry_t *e = ARRAY_get(MemoEntry_t, &m->ary, idx);
    MOZVM_PROFILE_INC(MEMO_SET);
    if (e->result && e->failed != MEMO_ENTRY_FAILED) {
        NODE_GC_RELEASE(e->result);
    }
    e->hash = hash;
    e->consumed = consumed;
    e->state    = state;
    e->result   = result;
    // ARRAY_set(MemoEntry_t, &m->ary, idx, e);
    return 1;
}

static int memo_elastic_fail(memo_t *m, mozpos_t pos, unsigned memoId)
{
    uintptr_t hash = (((uintptr_t)pos << m->shift) | memoId);
    unsigned idx = hash & m->mask;
    MemoEntry_t *old = ARRAY_get(MemoEntry_t, &m->ary, idx);
    MOZVM_PROFILE_INC(MEMO_FAIL);
    if (old->result && old->failed != MEMO_ENTRY_FAILED) {
        NODE_GC_RELEASE(old->result);
    }
    old->hash = hash;
    old->failed = MEMO_ENTRY_FAILED;
    return 0;
}

static MemoEntry_t *memo_elastic_get(memo_t *m, mozpos_t pos, unsigned memoId, unsigned state)
{
    uintptr_t hash = (((uintptr_t)pos << m->shift) | memoId);
    unsigned idx = hash & m->mask;
    MemoEntry_t *e = ARRAY_get(MemoEntry_t, &m->ary, idx);
    MOZVM_PROFILE_INC(MEMO_GET);
    if (e->hash == hash) {
        if (e->state == state) {
            if (e->failed == MEMO_ENTRY_FAILED) {
                MOZVM_PROFILE_INC(MEMO_HITFAIL);
            }
            else {
                MOZVM_PROFILE_INC(MEMO_HIT);
            }
            return e;
        }
    }
    MOZVM_PROFILE_INC(MEMO_MISS);
    return NULL;
}

static void memo_elastic_dispose(memo_t *m)
{
    unsigned i;
    for (i = 0; i < ARRAY_size(m->ary); i++) {
        MemoEntry_t *e = ARRAY_get(MemoEntry_t, &m->ary, i);
        if (e->result && e->failed != MEMO_ENTRY_FAILED) {
            NODE_GC_RELEASE(e->result);
        }
    }
    memset(m->ary.list, 0, sizeof(MemoEntry_t) * ARRAY_size(m->ary));
    ARRAY_dispose(MemoEntry_t, &m->ary);
}

#elif defined(MOZVM_MEMO_TYPE_HASH)
#else  /* MOZVM_MEMO_TYPE_NULL */
static void memo_null_init(memo_t *memo, unsigned w, unsigned n)
{
    /* do nothing */
}

static int memo_null_set(memo_t *m, mozpos_t pos, unsigned memoId, MemoEntry_t *e)
{
    return 0;
}

static int memo_null_fail(memo_t *m, mozpos_t pos, unsigned memoId)
{
    return 0;
}

static MemoEntry_t *memo_null_get(memo_t *m, mozpos_t pos, unsigned memoId, unsigned state)
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

MemoEntry_t *memo_get(memo_t *m, mozpos_t pos, uint32_t memoId, uint8_t state)
{
#if defined(MOZVM_MEMO_TYPE_ELASTIC)
    return memo_elastic_get(m, pos, memoId, state);
#elif defined(MOZVM_MEMO_TYPE_HASH)
#else  /* MOZVM_MEMO_TYPE_NULL */
    return memo_null_get(m, pos, memoId, state);
#endif
}

int memo_fail(memo_t *m, mozpos_t pos, uint32_t memoId)
{
#if defined(MOZVM_MEMO_TYPE_ELASTIC)
    return memo_elastic_fail(m, pos, memoId);
#elif defined(MOZVM_MEMO_TYPE_HASH)
#else  /* MOZVM_MEMO_TYPE_NULL */
    return memo_null_fail(m, pos, memoId);
#endif
}

int memo_set(memo_t *m, mozpos_t pos, uint32_t memoId, Node *result, unsigned consumed, int state)
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

void memo_print_stats()
{
    MOZVM_MEMO_PROFILE_EACH(MOZVM_PROFILE_SHOW);
}

#ifdef MOZVM_MEMORY_USE_MSGC
void memo_trace(void *p, NodeVisitor *visitor)
{
    memo_t *m = (memo_t *)p;
#if defined(MOZVM_MEMO_TYPE_ELASTIC)
    MemoEntry_t *x, *e;
    FOR_EACH_ARRAY(m->ary, x, e) {
        if (x->result && x->failed != MEMO_ENTRY_FAILED) {
            visitor->fn_visit(visitor, x->result);
        }
    }
#endif
}
#endif

#ifdef __cplusplus
}
#endif
