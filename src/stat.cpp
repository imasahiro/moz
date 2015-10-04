#define MOZVM_PROFILE 1
#define MOZVM_MEMORY_PROFILE 1
// #define MOZVM_PROFILE_INST  1

#include "mozvm_config.h"

#include "ast.c"
#include "symtable.c"
#include "memo.c"
#include "loader.c"

#ifdef MOZVM_ENABLE_JIT
#include "jit.cpp"
#endif

#include "node.c"
#include "memory.c"
#include "mozvm.c"
#include "main.c"
