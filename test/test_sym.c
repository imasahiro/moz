#include "symtable.h"
#include <assert.h>

int main(int argc, char const* argv[])
{
    token_t tmp = {};
    symtable_t *tbl = symtable_init();
    assert(symtable_has_symbol(tbl, "NULL") == 0);
    assert(symtable_get_symbol(tbl, "NULL", &tmp) == 0);
    symtable_dispose(tbl);
    return 0;
}
