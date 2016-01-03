#include "core/karray.h"
#include <assert.h>

DEF_ARRAY_T_OP_NOPOINTER(int);

void test_array_add()
{
    ARRAY(int) a;
    ARRAY_init(int, &a, 1);
    ARRAY_add(int, &a, 10);
    assert(ARRAY_size(a) == 1);
    assert(ARRAY_get(int, &a, 0) == 10);
    ARRAY_dispose(int, &a);
}

void test_array_each()
{
    int ary[] = {1, 2, 3, 4};
    ARRAY(int) a;
    ARRAY_init(int, &a, 1);
    ARRAY_add(int, &a, 1);
    ARRAY_add(int, &a, 2);
    ARRAY_add(int, &a, 3);
    ARRAY_add(int, &a, 4);
    int *x, *e;
    int idx = 0;
    int loop = 0;
    assert(*ARRAY_BEGIN(a) == ary[0]);
    assert(*ARRAY_last(a) == ary[3]);
    FOR_EACH_ARRAY(a, x, e) {
        assert(*x == ary[idx]);
        idx++;
        loop++;
    }
    assert(loop == 4);
    idx = 3;
    loop = 0;
    FOR_EACH_ARRAY_R(a, x, e) {
        assert(*x == ary[idx]);
        idx--;
        loop++;
    }
    assert(loop == 4);
}

int main(int argc, char const* argv[])
{
    test_array_add();
    test_array_each();
    return 0;
}
