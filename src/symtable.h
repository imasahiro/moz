#include "token.h"
#include "karray.h"

#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

typedef struct symtable_entry_t {
    unsigned state;
    unsigned hash;
    const char *tag;
    token_t sym;
} entry_t;

DEF_ARRAY_STRUCT0(entry_t, unsigned);
DEF_ARRAY_T(entry_t);

struct symtable_t {
    unsigned state;
    ARRAY(entry_t) table;
};

typedef struct symtable_t symtable_t;

symtable_t *symtable_init();
void symtable_dispose(symtable_t *tbl);

void symtable_add_symbol_mask(symtable_t *tbl, const char *tableName);
void symtable_add_symbol(symtable_t *tbl, const char *tableName, token_t *captured);
int symtable_has_symbol(symtable_t *tbl, const char *tableName);
int symtable_get_symbol(symtable_t *tbl, const char *tableName, token_t *t);
int symtable_contains(symtable_t *tbl, const char *tableName, token_t *t);

static inline long symtable_savepoint(symtable_t *tbl)
{
    return ARRAY_size(tbl->table);
}

void symtable_rollback(symtable_t *tbl, long saved);
#endif /* end of include guard */
