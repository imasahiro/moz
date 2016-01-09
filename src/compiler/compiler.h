#include "mozvm.h"
#include "node/node.h"
#include "core/pstring.h"

#ifndef MOZ_COMPILER_H
#define MOZ_COMPILER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef pstring_t *pstring_ptr_t;

DEF_ARRAY_STRUCT0(pstring_ptr_t, unsigned);
DEF_ARRAY_T(pstring_ptr_t);

DEF_ARRAY_STRUCT0(bitset_t, unsigned);
DEF_ARRAY_T(bitset_t);

typedef struct decl *decl_ptr_t;
typedef struct expr *expr_ptr_t;

DEF_ARRAY_STRUCT0(decl_ptr_t, unsigned);
DEF_ARRAY_T(decl_ptr_t);

DEF_ARRAY_STRUCT0(expr_ptr_t, unsigned);
DEF_ARRAY_T(expr_ptr_t);

#ifndef LOADER_H
DEF_ARRAY_STRUCT0(uint8_t, unsigned);
DEF_ARRAY_T(uint8_t);
#endif

typedef struct block_t *block_ptr_t;
DEF_ARRAY_STRUCT0(block_ptr_t, unsigned);
DEF_ARRAY_T(block_ptr_t);

typedef struct moz_compiler_t {
    moz_runtime_t *R;
    ARRAY(decl_ptr_t) decls;
    ARRAY(block_ptr_t) blocks;
    ARRAY(pstring_ptr_t) strs;
    ARRAY(pstring_ptr_t) tags;
    ARRAY(bitset_t) sets;
} moz_compiler_t;

moz_compiler_t *moz_compiler_init(moz_compiler_t *C, moz_runtime_t *R);
void moz_compiler_dispose(moz_compiler_t *C);
struct moz_module_t *moz_compiler_compile(moz_runtime_t *R, Node *node);

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
