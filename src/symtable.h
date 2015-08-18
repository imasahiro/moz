#include "token.h"

#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

struct symtable_t;
typedef struct symtable_t symtable_t;

void symtable_add_symbol_mask(symtable_t *tbl, char *tableName);
void symtable_add_symbol(symtable_t *tbl, char *tableName, token_t *captured);
int symtable_has_symbol(symtable_t *tbl, char *tableName);
int symtable_get_symbol(symtable_t *tbl, char *tableName, token_t *t);
int symtable_contains(symtable_t *tbl, char *tableName, token_t *t);
long symtable_savepoint(symtable_t *tbl);
void symtable_rollback(symtable_t *tbl, long saved);
#endif /* end of include guard */
