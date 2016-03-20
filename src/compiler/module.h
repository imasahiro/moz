#include "libnez/ast.h"
#include "libnez/symtable.h"
#include "libnez/memo.h"

#ifndef MOZ_MODULE_H
#define MOZ_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct moz_module_t moz_module_t;

struct moz_module_t {
    struct moz_runtime_t *runtime;
    int (*parse)(moz_module_t *, char *input, size_t input_size, Node **result);
    void (*dump)(moz_module_t *);
    void (*dispose)(moz_module_t *);
};

struct moz_compiler_t;
/* Internal API */
moz_module_t *moz_vm2_module_compile(struct moz_compiler_t *C);

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
