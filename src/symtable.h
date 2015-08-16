#include "token.h"

#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

void symtable_add_symbol_mask(char *tableName);
void symtable_add_symbol(char *tableName, token_t *captured);
int symtable_has_symbol(char *tableName);
int symtable_get_symbol(char *tableName, token_t *t);
int symtable_contains(char *tableName, token_t *t);
long symtable_savepoint();
void symtable_rollback(long saved);
#endif /* end of include guard */
