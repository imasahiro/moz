#include "memo.h"
#include <stdio.h>
#include <assert.h>

int main(int argc, char const* argv[])
{
    char *str = "hello world";
    memo_t *memo;
    NodeManager_init();
    memo = memo_init(3, 4, MEMO_TYPE_NULL);
    assert(memo_set(memo, str, 0, NULL, 0, 0) == 0);
    assert(memo_get(memo, str, 0, 0) == 0);
    memo_dispose(memo);
    NodeManager_dispose();
    return 0;
}
