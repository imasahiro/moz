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

#ifndef LOG2
#define LOG2(N) ((unsigned)((sizeof(void *) * 8) - __builtin_clzl(N - 1)))
#endif

// AstMachine
#define MOZ_AST_MACHINE_DEFAULT_LOG_SIZE 128

// Memo
#define MOZ_MEMO_DEFAULT_WINDOW_SIZE 32
// #define MOZVM_MEMO_TYPE_NULL    1
// #define MOZVM_MEMO_TYPE_HASH    1
#define MOZVM_MEMO_TYPE_ELASTIC 1

// Runtime
#define MOZ_DEFAULT_STACK_SIZE  (128*1024)

// jump table
#define MOZ_JMPTABLE_SIZE 256
// #define MOZ_JMPTABLE_SIZE 257

// node
#define MOZVM_SMALL_ARRAY_LIMIT 2
#define MOZVM_MEMORY_USE_RCGC  1
#define MOZVM_NODE_ARENA_SIZE  8
#define MOZVM_NODE_USE_MEMPOOL 1
#define MOZVM_USE_FREE_LIST 1
// #define MOZVM_MEMORY_USE_BOEHM_GC 1

// VM / bytecode
#define MOZVM_SMALL_STRING_INST 1
#define MOZVM_SMALL_TAG_INST    1
#define MOZVM_SMALL_BITSET_INST 1
#define MOZVM_SMALL_JMPTBL_INST 1
#define MOZVM_USE_JMPTBL 1
// #define MOZVM_USE_INT16_ADDR 1
// #define MOZVM_DEBUG_NTERM       1
#define MOZVM_USE_SSE4_2        1
// #define MOZVM_USE_SWITCH_CASE_DISPATCH 1
#define MOZVM_USE_INDIRECT_THREADING   1
// #define MOZVM_USE_DIRECT_THREADING     1

// #define MOZVM_PROFILE_INST  1

#ifdef MOZVM_USE_DIRECT_THREADING
#define MOZVM_INST_HEADER_SIZE sizeof(long)
#else
#define MOZVM_INST_HEADER_SIZE sizeof(unsigned char)
#endif

#ifdef MOZVM_USE_INT16_ADDR
typedef short mozaddr_t;
#else
typedef int   mozaddr_t;
#endif

#define MOZVM_USE_POINTER_AS_POS_REGISTER 1
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
typedef const char *mozpos_t;
#else
typedef unsigned long mozpos_t;
#endif

// profile
// #define MOZVM_PROFILE 1

#ifdef MOZVM_PROFILE
#define MOZVM_PROFILE_DECL(X) unsigned long long _PROFILE_##X = 0;
#define MOZVM_PROFILE_INC(X)  (_PROFILE_##X)++
#define MOZVM_PROFILE_SHOW(X) fprintf(stderr, "%-10s %llu\n", #X, _PROFILE_##X);
#define MOZVM_PROFILE_ENABLE(X)
#else
#define MOZVM_PROFILE_DECL(X)
#define MOZVM_PROFILE_INC(X)
#define MOZVM_PROFILE_SHOW(X)
#define MOZVM_PROFILE_ENABLE(X)
#endif
#define MOZVM_PROFILE_EACH(F) MOZVM_PROFILE_DEFINE(F)

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
