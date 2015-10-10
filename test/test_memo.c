#include "libnez/memo.h"
#include <stdio.h>
#include <assert.h>

int main(int argc, char const* argv[])
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    char *str = "hello world";
#endif
    memo_t *memo;
    MemoEntry_t *e;
    NodeManager_init();
    memo = memo_init(MOZ_MEMO_DEFAULT_WINDOW_SIZE, 4);
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    memo_set(memo, str, 0, NULL, 0, 0);
    e = memo_get(memo, str, 0, 0);
#else
    memo_set(memo, 0, 0, NULL, 0, 0);
    e = memo_get(memo, 0, 0, 0);
#endif
    assert(e->result == NULL);
    memo_dispose(memo);
    NodeManager_dispose();
    return 0;
}
