#include <stdio.h>
#include <assert.h>
#include "core/bitset.h"

void test_bitset()
{
    bitset_t set;
    bitset_init(&set);
    bitset_set(&set, 2);
    bitset_set(&set, 5);
    assert(bitset_get(&set, 0) == 0);
    assert(bitset_get(&set, 1) == 0);
    assert(bitset_get(&set, 2) == 1);
    assert(bitset_get(&set, 3) == 0);
    assert(bitset_get(&set, 4) == 0);
    assert(bitset_get(&set, 5) == 1);
    assert(bitset_get(&set, 6) == 0);
}

int main(int argc, char const* argv[])
{
    test_bitset();
    return 0;
}
