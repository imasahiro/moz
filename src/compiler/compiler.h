#include "mozvm.h"
#include "node/node.h"

#ifndef MOZ_COMPILER_H
#define MOZ_COMPILER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct compiler moz_compiler_t;
moz_compiler_t *moz_compiler_init(moz_compiler_t *C, moz_runtime_t *R);
void moz_compiler_dispose(moz_compiler_t *C);
void moz_compiler_compile(moz_runtime_t *R, Node *node);
#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
