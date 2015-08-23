#include "memo.h"
#include <stdio.h>
#include <assert.h>

int main(int argc, char const* argv[])
{
    char *str = "hello world";
    memo_t *memo;
    MemoEntry_t *e;
    NodeManager_init();
    memo = memo_init(MOZ_MEMO_DEFAULT_WINDOW_SIZE, 4);
    memo_set(memo, str, 0, NULL, 0, 0);
    e = memo_get(memo, str, 0, 0);
    assert(e->result == NULL);
    memo_dispose(memo);
    NodeManager_dispose();
    return 0;
}
