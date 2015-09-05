#ifndef MOZ_VM_H
#define MOZ_VM_H

#include "mozvm_config.h"
#include "libnez.h"
#include "bitset.h"

#ifdef MOZVM_USE_JMPTBL
#include "jmptbl.h"
#endif

#include <stdint.h>

typedef uint8_t moz_inst_t;
struct moz_runtime_t;

#ifdef MOZVM_ENABLE_JIT
typedef uint8_t (*moz_jit_func_t)(struct moz_runtime_t *, const char *, mozpos_t *);

typedef struct mozvm_nterm_entry_t {
    moz_inst_t *begin;
    moz_inst_t *end;
    unsigned call_counter;
    moz_jit_func_t compiled_code;
} mozvm_nterm_entry_t;

typedef void jit_context_t;
#endif

typedef struct mozvm_constant_t {
    bitset_t *sets;
    const char **tags;
    const char **strs;
    const char **tables;
    int *jumps;
#ifdef MOZVM_USE_JMPTBL
    jump_table1_t *jumps1;
    jump_table2_t *jumps2;
    jump_table3_t *jumps3;
#endif
    const char **nterms;

    uint16_t set_size;
    uint16_t str_size;
    uint16_t tag_size;
    uint16_t table_size;
    uint16_t nterm_size;

    unsigned inst_size;
    unsigned memo_size;
    unsigned input_size;
#ifdef MOZVM_PROFILE_INST
    long *profile;
#endif
} mozvm_constant_t;

typedef struct moz_runtime_t {
    AstMachine *ast;
    symtable_t *table;
    memo_t *memo;
    mozpos_t head;
    const char *tail;
    const char *input;
    long *stack;
    long *fp;

#ifdef MOZVM_ENABLE_JIT
    mozvm_nterm_entry_t *nterm_entry;
    jit_context_t *jit_context;
#endif
    mozvm_constant_t C;
    long stack_[1];
} moz_runtime_t;

#if MOZVM_SMALL_STRING_INST
typedef uint16_t STRING_t;
#define STRING_GET_IMPL(runtime, ID) runtime->C.strs[(ID)]
#else
typedef const char *STRING_t;
#define STRING_GET_IMPL(runtime, ID) (ID)
#endif

typedef const char tag_t;
#if MOZVM_SMALL_TAG_INST
typedef uint16_t TAG_t;
#define TAG_GET_IMPL(runtime, ID) runtime->C.tags[(ID)]
#define TBL_GET_IMPL(runtime, ID) runtime->C.tables[(ID)]
#else
typedef tag_t *TAG_t;
#define TAG_GET_IMPL(runtime, ID) (ID)
#define TBL_GET_IMPL(runtime, ID) (ID)
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

moz_runtime_t *moz_runtime_init(unsigned memo_size, unsigned nterm_size);
void moz_runtime_dispose(moz_runtime_t *r);
void moz_runtime_reset1(moz_runtime_t *r);
void moz_runtime_reset2(moz_runtime_t *r);

static inline void moz_runtime_set_source(moz_runtime_t *r, const char *str, const char *end)
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    r->head = str;
#else
    r->head = 0;
#endif
    r->tail = end;
    AstMachine_setSource(r->ast, str);
}

void moz_runtime_print_stats(moz_runtime_t *r);
moz_inst_t *moz_runtime_parse_init(moz_runtime_t *, const char *, moz_inst_t *);
long moz_runtime_parse(moz_runtime_t *r, const char *str, const moz_inst_t *inst);

#endif /* end of include guard */
