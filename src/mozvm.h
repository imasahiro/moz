#include "bitset.h"
#include "ast.h"
#include "memo.h"
#include "symtable.h"
#include <stdint.h>

#ifndef MOZ_VM_H
#define MOZ_VM_H

typedef struct mozvm_constant_t {
    bitset_t *sets;
    char **tags;
    char **strs;
    int *jumps;
    const char **nterms;

    uint16_t set_size;
    uint16_t str_size;
    uint16_t tag_size;
    uint16_t table_size;
    uint16_t nterm_size;

    unsigned inst_size;
    unsigned memo_size;
    unsigned jumptable_size;
    unsigned input_size;

    // config

} mozvm_constant_t;

typedef struct moz_runtime_t {
    AstMachine *ast;
    symtable_t *table;
    memo_t *memo;
    char *head;
    char *tail;
    char *input;
    long *stack;

    mozvm_constant_t C;
    long stack_[1];
} moz_runtime_t;

#if MOZVM_SMALL_STRING_INST
typedef uint16_t STRING_t;
#define STRING_GET_IMPL(runtime, ID) runtime->C.strs[(ID)]
#else
typedef char *STRING_t;
#define STRING_GET_IMPL(runtime, ID) (ID)
#endif

typedef char tag_t;
#if MOZVM_SMALL_TAG_INST
typedef uint16_t TAG_t;
#define TAG_GET_IMPL(runtime, ID) runtime->C.tags[(ID)]
#else
typedef tag_t *TAG_t;
#define TAG_GET_IMPL(runtime, ID) (ID)
#endif

#if MOZVM_SMALL_BITSET_INST
typedef uint16_t BITSET_t;
#define BITSET_GET_IMPL(runtime, ID) &(runtime->C.sets[(ID)])
#else
typedef bitset_t *BITSET_t;
#define BITSET_GET_IMPL(runtime, ID) (ID)
#endif

#if MOZVM_SMALL_JMPTBL_INST
typedef uint16_t JMPTBL_t;
#define JMPTBL_GET_IMPL(runtime, ID) ((runtime)->C.jumps+(MOZ_JMPTABLE_SIZE * (ID)))
#else
typedef int *JMPTBL_t;
#define JMPTBL_GET_IMPL(runtime, ID) (ID)
#endif

typedef uint8_t moz_inst_t;
moz_runtime_t *moz_runtime_init(unsigned jmptbl_size, unsigned memo_size);
void moz_runtime_dispose(moz_runtime_t *r);
void moz_runtime_reset(moz_runtime_t *r);
void moz_runtime_set_source(moz_runtime_t *r, char *str, char *end);

long moz_runtime_parse(moz_runtime_t *r, char *str, moz_inst_t *inst);

#endif /* end of include guard */
