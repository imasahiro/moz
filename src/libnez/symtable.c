#include "symtable.h"
#define KHASH_USE_FNV1A 1
#include "khash.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOZVM_SYMTBL_PROFILE_EACH(F) \
    F(SYMTBL_MASK) \
    F(SYMTBL_ADD) \
    F(SYMTBL_HAS) \
    F(SYMTBL_GET) \
    F(SYMTBL_CONTAIN)

#ifdef MOZVM_PROFILE
static uint64_t max_symtbl_size = 0;
#endif

MOZVM_SYMTBL_PROFILE_EACH(MOZVM_PROFILE_DECL);

DEF_ARRAY_OP(entry_t);

symtable_t *symtable_init()
{
    symtable_t *tbl = (symtable_t *)VM_MALLOC(sizeof(*tbl));
    ARRAY_init(entry_t, &tbl->table, 4);
    tbl->state = 0;
    return tbl;
}

void symtable_dispose(symtable_t *tbl)
{
    ARRAY_dispose(entry_t, &tbl->table);
    VM_FREE(tbl);
}

void symtable_print_stats()
{
#ifdef MOZVM_PROFILE
    fprintf(stderr, "%-10s %llu\n", "MAX_SYMTBL_SIZE", max_symtbl_size);
#endif
    MOZVM_SYMTBL_PROFILE_EACH(MOZVM_PROFILE_SHOW);
}

static void symtable_push(symtable_t *tbl, const char *tag, unsigned hash, token_t *t)
{
    entry_t entry;
    entry.state = tbl->state++;
    entry.hash = hash;
    entry.tag = tag;
    if (t) {
        token_copy(&entry.sym, t);
    }
    ARRAY_add(entry_t, &tbl->table, &entry);
#ifdef MOZVM_PROFILE
    if (max_symtbl_size < ARRAY_size(tbl->table)) {
        max_symtbl_size = ARRAY_size(tbl->table);
    }
#endif
}

void symtable_add_symbol_mask(symtable_t *tbl, const char *tableName)
{
    MOZVM_PROFILE_INC(SYMTBL_MASK);
    symtable_push(tbl, tableName, 0, NULL);
}

void symtable_add_symbol(symtable_t *tbl, const char *tableName, token_t *captured)
{
    unsigned hash = fnv1a(captured->s, captured->len);
    MOZVM_PROFILE_INC(SYMTBL_ADD);
    symtable_push(tbl, tableName, hash, captured);
}

int symtable_has_symbol(symtable_t *tbl, const char *tableName)
{
    entry_t *cur, *head;
    MOZVM_PROFILE_INC(SYMTBL_HAS);

    if (ARRAY_size(tbl->table) == 0) {
        return 0;
    }
    cur = ARRAY_last(tbl->table);
    head = ARRAY_get(entry_t, &tbl->table, 0);
    for (; cur >= head; --cur) {
        if (cur->tag == tableName) {
            return cur->sym.s != NULL;
        }
    }
    return 0;
}

int symtable_get_symbol(symtable_t *tbl, const char *tableName, token_t *t)
{
    entry_t *cur, *head;
    MOZVM_PROFILE_INC(SYMTBL_GET);

    if (ARRAY_size(tbl->table) == 0) {
        return 0;
    }
    cur = ARRAY_last(tbl->table);
    head = ARRAY_get(entry_t, &tbl->table, 0);
    for (; cur >= head; --cur) {
        if (cur->tag == tableName) {
            if (cur->sym.s != NULL) {
                token_copy(t, &cur->sym);
                return 1;
            }
        }
    }
    return 0;
}

int symtable_contains(symtable_t *tbl, const char *tableName, token_t *t)
{
    entry_t *cur, *head;
    unsigned hash;

    MOZVM_PROFILE_INC(SYMTBL_CONTAIN);

    if (ARRAY_size(tbl->table) == 0) {
        return 0;
    }
    hash = fnv1a(t->s, t->len);
    cur = ARRAY_last(tbl->table);
    head = ARRAY_get(entry_t, &tbl->table, 0);
    for (; cur >= head; --cur) {
        if (cur->tag == tableName) {
            if (cur->sym.s == NULL) {
                return 0;
            }
            if (hash == cur->hash && token_equal(t, &cur->sym)) {
                return 1;
            }
        }
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
