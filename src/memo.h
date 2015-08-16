#ifndef MEMO_H
#define MEMO_H

typedef struct MemoEntry {
    uint8_t failed;
    uint32_t consumed;
    void *result;
} MemoEntry_t;

MemoEntry_t *memo_get(uint32_t memoId, uint8_t state);
void memo_hit();
void memo_failhit();
void memo_miss();
void memo_set(char *pos, uint32_t memoId, int failed, void *result, unsigned consumed, int state);

#endif /* end of include guard */
