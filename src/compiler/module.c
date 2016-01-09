#include "module.h"
#include "core/karray.h"
#include "ir.h"
#include "block.h"

#ifdef __cplusplus
extern "C" {
#endif

DEF_ARRAY_OP_NOPOINTER(decl_ptr_t);
DEF_ARRAY_OP_NOPOINTER(expr_ptr_t);

typedef struct moz_vm2_module_t {
    moz_module_t base;
} moz_vm2_module_t;

static int moz_vm2_module_parse(moz_module_t *_M, moz_parser_runtime_t *R)
{
    moz_vm2_module_t *M = (moz_vm2_module_t *) _M;
    (void)M;
    return 0;
}

static int moz_vm2_module_dispose(moz_module_t *_M)
{
    moz_vm2_module_t *M = (moz_vm2_module_t *) _M;
    VM_FREE(M);
    return 0;
}

moz_module_t *moz_vm2_module_compile(struct moz_compiler_t *C)
{
    moz_vm2_module_t *M = VM_CALLOC(1, sizeof(*M));
    block_t **I, **E;
    FOR_EACH_ARRAY(C->blocks, I, E) {
        IR_t **x, **e;
        FOR_EACH_ARRAY((*I)->insts, x, e) {
        }
    }
    M->base.parse = moz_vm2_module_parse;
    M->base.dispose = moz_vm2_module_dispose;
    return (moz_module_t *) M;
}

#ifdef __cplusplus
}
#endif
