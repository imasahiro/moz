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
    char **tags;
    char **strs;
    int *jumps;
    long stack_[1];
} moz_runtime_t;

#ifdef MOZVM_SMALL_STRING_INST
typedef uint16_t STRING_t;
#define STRING_GET_IMPL(runtime, ID) runtime->strs[(ID)]
#else
typedef char *STRING_t;
#define STRING_GET_IMPL(runtime, ID) (ID)
#endif

typedef char tag_t;
#ifdef MOZVM_SMALL_TAG_INST
typedef uint16_t TAG_t;
#define TAG_GET_IMPL(runtime, ID) runtime->tags[(ID)]
#else
typedef tag_t *TAG_t;
#define TAG_GET_IMPL(runtime, ID) (ID)
#endif

#ifdef MOZVM_SMALL_BITSET_INST
typedef uint16_t BITSET_t;
#define BITSET_GET_IMPL(runtime, ID) &(runtime->sets[(ID)])
#else
typedef bitset_t *BITSET_t;
#define BITSET_GET_IMPL(runtime, ID) (ID)
#endif

#ifdef MOZVM_SMALL_JMPTBL_INST
typedef uint16_t JMPTBL_t;
#define JMPTBL_GET_IMPL(runtime, ID) ((runtime)->jumps+(MOZ_JMPTABLE_SIZE * (ID)))
#else
typedef int *JMPTBL_t;
#define JMPTBL_GET_IMPL(runtime, ID) (ID)
#endif

typedef uint8_t moz_inst_t;
moz_runtime_t *moz_runtime_init(unsigned jmptbl_size, unsigned memo_size);
void moz_runtime_dispose(moz_runtime_t *r);
void moz_runtime_reset(moz_runtime_t *r);

int moz_runtime_parse(moz_runtime_t *r, char *start, char *end, moz_inst_t *inst);

#endif /* end of include guard */
