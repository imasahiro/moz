#include "bitset.h"
#include "ast.h"
#include "memo.h"
#include "symtable.h"
#include <stdint.h>

#ifndef MOZ_VM_H
#define MOZ_VM_H

typedef struct moz_bytecode_t {
    /* header */
    unsigned inst_size;
    unsigned memo_size;
    unsigned jumptable_size;
    /* const data */
    uint16_t nterm_size;
    const char **nterms;
    uint16_t set_size;
    bitset_t *sets;
    uint16_t str_size;
    const char **strs;
    uint16_t tag_size;
    const char **tags;
    uint16_t table_size;
    void *table;
} moz_bytecode_t;

typedef struct moz_runtime_t {
    AstMachine *ast;
    symtable_t *table;
    memo_t *memo;
    char *head;
    char *tail;
    char *input;
    long *stack;

    bitset_t *sets;
    const char **tags;
    const char **strs;
    int **tables;
    long stack_[1];
} moz_runtime_t;

typedef uint8_t moz_inst_t;
moz_runtime_t *moz_runtime_init(unsigned memo_size);
void moz_runtime_dispose(moz_runtime_t *r);

int moz_runtime_parse(moz_runtime_t *r, char *start, char *end, moz_inst_t *inst);

#endif /* end of include guard */
