#include "mozvm.h"
#include "node/node.h"

#ifndef MOZ_COMPILER_H
#define MOZ_COMPILER_H

#ifdef __cplusplus
extern "C" {
#endif

void moz_compiler_compile(const char *output_file, moz_runtime_t *R, Node *node);

typedef struct compiler moz_compiler_t;
#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
