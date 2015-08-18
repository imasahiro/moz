#include "symtable.h"
#include <stdlib.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

struct symtable_t {
};

symtable_t *symtable_init()
{
    assert(0 && "not implemented");
    return NULL;
}

void symtable_dispose(symtable_t *sym)
{
    assert(0 && "not implemented");
}

void symtable_add_symbol_mask(symtable_t *tbl, char *tableName)
{
    assert(0 && "not implemented");
}

void symtable_add_symbol(symtable_t *tbl, char *tableName, token_t *captured)
{
    assert(0 && "not implemented");
}

int symtable_has_symbol(symtable_t *tbl, char *tableName)
{
    assert(0 && "not implemented");
    return 0;
}

int symtable_get_symbol(symtable_t *tbl, char *tableName, token_t *t)
{
    assert(0 && "not implemented");
    return 0;
}

int symtable_contains(symtable_t *tbl, char *tableName, token_t *t)
{
    assert(0 && "not implemented");
    return 0;
}

long symtable_savepoint(symtable_t *tbl)
{
    assert(0 && "not implemented");
    return -1;
}

void symtable_rollback(symtable_t *tbl, long saved)
{
    assert(0 && "not implemented");
}

#ifdef DEBUG
int main(int argc, char const* argv[])
{
    symtable_t *sym = symtable_init();
    symtable_dispose(sym);
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif
