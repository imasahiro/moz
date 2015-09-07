#define MOZVM_PROFILE 1
#define MOZVM_MEMORY_PROFILE 1
// #define MOZVM_PROFILE_INST  1

#include "mozvm_config.h"
#include "node.c"
#include "ast.c"
#include "symtable.c"
#include "memo.c"

#include "memory.c"
#include "loader.c"
#include "vm.c"
#include "main.c"

#ifdef MOZVM_ENABLE_JIT
#include "jit.cpp"
#endif
