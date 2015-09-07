#include "bitset.h"

#ifndef MOZVM_JMPTBL_T
#define MOZVM_JMPTBL_T

#ifdef __cplusplus
extern "C" {
#endif

typedef struct jump_table1_t {
    bitset_t b[1];
    int jumps[2];
} jump_table1_t;

typedef struct jump_table2_t {
    bitset_t b[2];
    int jumps[4];
} jump_table2_t;

typedef struct jump_table3_t {
    bitset_t b[3];
    int jumps[8];
} jump_table3_t;

static inline void jump_table1_init(jump_table1_t *tbl, int targets[2], int *jumps)
{
    unsigned i = 0;
    bitset_init(&tbl->b[0]);
    tbl->jumps[0] = targets[0];
    tbl->jumps[1] = targets[1];
    for (i = 0; i < 256; i++) {
        if (tbl->jumps[1] == jumps[i]) {
            bitset_set(&tbl->b[0], i);
        }
    }
}

static inline void jump_table2_init(jump_table2_t *tbl, int targets[4], int *jumps)
{
    unsigned i;
    bitset_init(&tbl->b[0]);
    bitset_init(&tbl->b[1]);
    for (i = 0; i < 4; i++) {
        tbl->jumps[i] = targets[i];
    }
    for (i = 0; i < 256; i++) {
        if (tbl->jumps[3] == jumps[i]) {
            bitset_set(&tbl->b[1], i);
            bitset_set(&tbl->b[0], i);
        }
        if (tbl->jumps[2] == jumps[i]) {
            bitset_set(&tbl->b[1], i);
        }
        if (tbl->jumps[1] == jumps[i]) {
            bitset_set(&tbl->b[0], i);
        }
    }
}

static inline void jump_table3_init(jump_table3_t *tbl, int targets[8], int *jumps)
{
    unsigned i;
    bitset_init(&tbl->b[0]);
    bitset_init(&tbl->b[1]);
    bitset_init(&tbl->b[2]);
    for (i = 0; i < 8; i++) {
        tbl->jumps[i] = targets[i];
    }
    for (i = 0; i < 256; i++) {
        if (tbl->jumps[7] == jumps[i]) {
            bitset_set(&tbl->b[2], i);
            bitset_set(&tbl->b[1], i);
            bitset_set(&tbl->b[0], i);
        }
        if (tbl->jumps[6] == jumps[i]) {
            bitset_set(&tbl->b[2], i);
            bitset_set(&tbl->b[1], i);
        }
        if (tbl->jumps[5] == jumps[i]) {
            bitset_set(&tbl->b[2], i);
            bitset_set(&tbl->b[0], i);
        }
        if (tbl->jumps[4] == jumps[i]) {
            bitset_set(&tbl->b[2], i);
        }
        if (tbl->jumps[3] == jumps[i]) {
            bitset_set(&tbl->b[1], i);
            bitset_set(&tbl->b[0], i);
        }
        if (tbl->jumps[2] == jumps[i]) {
            bitset_set(&tbl->b[1], i);
        }
        if (tbl->jumps[1] == jumps[i]) {
            bitset_set(&tbl->b[0], i);
        }
    }
}

static inline int jump_table1_jump(jump_table1_t *tbl, unsigned char ch)
{
    unsigned idx0 = bitset_get(&tbl->b[0], ch);
    return tbl->jumps[idx0];
}

static inline int jump_table2_jump(jump_table2_t *tbl, unsigned char ch)
{
    unsigned idx0 = bitset_get(&tbl->b[0], ch);
    unsigned idx1 = bitset_get(&tbl->b[1], ch);
    unsigned idx = idx1 << 1 | idx0;
    return tbl->jumps[idx];
}

static inline int jump_table3_jump(jump_table3_t *tbl, unsigned char ch)
{
    unsigned idx0 = bitset_get(&tbl->b[0], ch);
    unsigned idx1 = bitset_get(&tbl->b[1], ch);
    unsigned idx2 = bitset_get(&tbl->b[2], ch);
    unsigned idx = idx2 << 2 | idx1 << 1 | idx0;
    return tbl->jumps[idx];
}

#if 0
int main(int argc, char const* argv[])
{
    jump_table2_t tbl2;
    unsigned i;
    int target[4] = { 3, 278, 280, -1};
    int jumps[256] = {
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        278 ,280 ,280 ,280 ,280 ,280 ,280 ,280 ,280 ,280 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,
        3   ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3
    };

    jump_table2_init(&tbl2, target, jumps);
    char buf[1024];
    dump_set(&tbl2.b[0], buf);
    fprintf(stderr, "%s\n", buf);
    dump_set(&tbl2.b[1], buf);
    fprintf(stderr, "%s\n", buf);

    for (i = 0; i < 256; i++) {
        int jmp = jump_table2_jump(&tbl2, i);
        if (jmp != jumps[i]) {
            fprintf(stderr, "%d %d %d\n", i, jumps[i], jmp);
        }
    }
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
