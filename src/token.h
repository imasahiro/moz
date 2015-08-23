#ifndef TOKEN_H
#define TOKEN_H

typedef struct moz_token {
    const char *s;
    unsigned len;
} token_t;

static inline void token_init(token_t *t, const char *s, const char *e)
{
    t->s = s;
    t->len = e - s;
}

static inline void token_copy(token_t *t, token_t *t2)
{
    t->s   = t2->s;
    t->len = t2->len;
}

static inline unsigned token_length(token_t *t)
{
    return t->len;
}

static inline int token_equal(token_t *t1, token_t *t2)
{
    const char *s1, *s2;
    const char *e1, *e2;
    if (t1->len != t2->len) {
        return 0;
    }
    s1 = t1->s;
    s2 = t2->s;
    e1 = s1 + t1->len;
    e2 = s2 + t2->len;
    while (s1 < e1) {
        if (*s1++ != *s2++) {
            return 0;
        }
    }
    return 1;
}

static inline int token_equal_string(token_t *t, const char *s)
{
    const char *s1 = t->s;
    const char *e1 = s1 + t->len;
    while (s1 < e1) {
        if (*s1++ != *s++) {
            return 0;
        }
    }
    return 1;
}

#endif /* end of include guard */
