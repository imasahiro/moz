#include "memo.h"
#include "karray.h"
#include <stdlib.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct memo_api_t {
    void (*_init)(memo_t *, unsigned w, unsigned n);
    int  (*_set)(memo_t *, char *pos, unsigned id, MemoEntry_t *);
    MemoEntry_t *(*_get)(memo_t *, char *pos, unsigned id, unsigned state);
    void (*_dispose)(memo_t *);
} memo_api_t;

DEF_ARRAY_STRUCT0(MemoEntry_t, unsigned);
DEF_ARRAY_T(MemoEntry_t);
DEF_ARRAY_OP(MemoEntry_t);

struct memo {
    const memo_api_t *api;
    // union entry {
    //     kmap_t map;
    //     ARRAY(MemoEntry_t) ary;
    // } e;
    struct elastic_map {
        ARRAY(MemoEntry_t) ary;
        unsigned shift;
    } e;
};

typedef struct memo_stat {
    unsigned memo_stored;
    unsigned memo_used;
    unsigned memo_invalidated;
    unsigned memo_hit;
    unsigned memo_failhit;
    unsigned memo_miss;
} memo_stat_t;

static memo_stat_t stat = {};

static void memo_null_init(memo_t *memo, unsigned w, unsigned n)
{
    /* do nothing */
}

static int memo_null_set(memo_t *m, char *pos, unsigned id, MemoEntry_t *e)
{
    return 0;
}

static MemoEntry_t *memo_null_get(memo_t *m, char *pos, unsigned id, unsigned state)
{
    return NULL;
}

static void memo_null_dispose(memo_t *memo)
{
    /* do nothing */
}

static const memo_api_t MEMO_API_NULL = {
    memo_null_init,
    memo_null_set,
    memo_null_get,
    memo_null_dispose
};

static void memo_elastic_init(memo_t *m, unsigned w, unsigned n)
{
    unsigned i;
    unsigned len = w * n + 1;
    ARRAY_init(MemoEntry_t, &m->e.ary, len);
    MemoEntry_t e = {};
    e.hash = UINTPTR_MAX;
    for (i = 0; i < len; i++) {
        ARRAY_add(MemoEntry_t, &m->e.ary, &e);
    }
    m->e.shift = LOG2(n) + 1;
}

static int memo_elastic_set(memo_t *m, char *pos, unsigned id, MemoEntry_t *e)
{
    uintptr_t hash = (((uintptr_t)pos << m->e.shift) | id);
    unsigned idx = hash % ARRAY_size(m->e.ary);
    ARRAY_set(MemoEntry_t, &m->e.ary, idx, e);
    return 1;
}

static MemoEntry_t *memo_elastic_get(memo_t *m, char *pos, unsigned id, unsigned state)
{
    uintptr_t hash = (((uintptr_t)pos << m->e.shift) | id);
    unsigned idx = hash % ARRAY_size(m->e.ary);
    MemoEntry_t *e = ARRAY_get(MemoEntry_t, &m->e.ary, idx);
    if (e->hash == hash) {
        if (e->state == state) {
            return e;
        }
    }
    return NULL;
}

static void memo_elastic_dispose(memo_t *m)
{
    ARRAY_dispose(MemoEntry_t, &m->e.ary);
}

static const memo_api_t MEMO_API_ELASTIC = {
    memo_elastic_init,
    memo_elastic_set,
    memo_elastic_get,
    memo_elastic_dispose
};

memo_t *memo_init(unsigned w, unsigned n, enum memo_type type)
{
    memo_t *memo = (memo_t *)malloc(sizeof(*memo));
    switch (type) {
    case MEMO_TYPE_HASH:
        assert(0);
        // memo->api = &MEMO_API_HASH;
        break;
    case MEMO_TYPE_ELASTIC:
        memo->api = &MEMO_API_ELASTIC;
        break;
    case MEMO_TYPE_NULL:
    default:
        memo->api = &MEMO_API_NULL;
        break;
    }
    memo->api->_init(memo, w, n);
    return memo;
}

void memo_dispose(memo_t *memo)
{
    memo->api->_dispose(memo);
    free(memo);
}

MemoEntry_t *memo_get(memo_t *m, char *pos, uint32_t memoId, uint8_t state)
{
    return m->api->_get(m, pos, memoId, state);
}

int memo_set(memo_t *m, char *pos, uint32_t memoId, int failed, void *result, unsigned consumed, int state)
{
    MemoEntry_t e;
    e.failed   = failed;
    e.consumed = consumed;
    e.state    = state;
    e.result   = result;
    return m->api->_set(m, pos, memoId, &e);
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

#ifdef __cplusplus
}
#endif
