#include "symtable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct symtable_t {
} symtable_t;

void symtable_add_symbol_mask(char *tableName)
{
}

void symtable_add_symbol(char *tableName, token_t *captured)
{
}

int symtable_has_symbol(char *tableName)
{
    return 0;
}

int symtable_get_symbol(char *tableName, token_t *t)
{
    return 0;
}

int symtable_contains(char *tableName, token_t *t)
{
    return 0;
}

long symtable_savepoint()
{
    return -1;
}

void symtable_rollback(long saved)
{
}

#ifdef DEBUG
int main(int argc, char const* argv[])
{
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif
