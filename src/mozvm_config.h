#ifndef MOZVM_CONFIG_H
#define MOZVM_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef unlikely
#define unlikely(x)   __builtin_expect(!!(x), 0)
#endif

#ifndef likely
#define likely(x)     __builtin_expect(!!(x), 1)
#endif

#ifndef VM_MALLOC
#define VM_MALLOC(N) malloc(N)
#endif

#ifndef VM_FREE
#define VM_FREE(PTR) free(PTR)
#endif

#define VM_SMALL_ARRAY_LIMIT 2

#ifndef LOG2
#define LOG2(N) ((uint32_t)((sizeof(void *) * 8) - __builtin_clzl(N - 1)))
#endif

// AstMachine
#define MOZ_AST_MACHINE_DEFAULT_LOG_SIZE 128

// Memo
#define MOZ_MEMO_DEFAULT_WINDOW_SIZE 32

// Runtime
#define MOZ_DEFAULT_STACK_SIZE  128

// jump table
#define MOZ_JMPTABLE_SIZE 256
// #define MOZ_JMPTABLE_SIZE 257

// node
#define MOZVM_MEMORY_USE_RCGC 1
// #define MOZVM_MEMORY_USE_BOEHM_GC 1

// VM / bytecode
#define MOZVM_SMALL_STRING_INST 1
#define MOZVM_SMALL_TAG_INST    1
#define MOZVM_SMALL_BITSET_INST 1
#define MOZVM_SMALL_JMPTBL_INST 1

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
