#include "bitset.h"

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

#endif /* end of include guard */
