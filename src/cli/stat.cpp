#define MOZVM_PROFILE 1
#define MOZVM_MEMORY_PROFILE 1
// #define MOZVM_PROFILE_INST  1

#include "mozvm_config.h"

#include "libnez/ast.c"
#include "libnez/symtable.c"
#include "libnez/memo.c"
#include "loader.c"

#ifdef MOZVM_ENABLE_JIT
#include "jit.cpp"
#endif

#include "node/node.c"
#include "memory.c"
#include "mozvm.c"
#include "main.c"
