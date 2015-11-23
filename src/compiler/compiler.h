#include "mozvm.h"
#include "node/node.h"

#ifndef MOZ_COMPILER_H
#define MOZ_COMPILER_H

void moz_compiler_compile(const char *output_file, moz_runtime_t *R, Node *node);
#endif /* end of include guard */
