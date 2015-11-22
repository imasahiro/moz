#ifndef MOZVM_CONFIG_H
#define MOZVM_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* [core] */

/* [profile] */
// #ifndef MOZVM_PROFILE
// #define MOZVM_PROFILE 1
// #endif

#ifdef MOZVM_PROFILE
#ifndef MOZVM_MEMORY_PROFILE
#define MOZVM_MEMORY_PROFILE 1
#endif
// #ifndef MOZVM_PROFILE_INST
// #define MOZVM_PROFILE_INST  1
// #endif
#endif


// AstMachine
#define MOZ_AST_MACHINE_DEFAULT_LOG_SIZE 128

// Memo
#define MOZ_MEMO_DEFAULT_WINDOW_SIZE 32
// #define MOZVM_MEMO_TYPE_NULL    1
// #define MOZVM_MEMO_TYPE_HASH    1
#define MOZVM_MEMO_TYPE_ELASTIC 1
// #define MOZVM_USE_DYNAMIC_DEACTIVATION 1

// Runtime
#define MOZ_DEFAULT_STACK_SIZE  (1024)

// jump table
#define MOZ_JMPTABLE_SIZE 256
// #define MOZ_JMPTABLE_SIZE 257

// node
#define MOZVM_SMALL_ARRAY_LIMIT 2
#define MOZVM_MEMORY_USE_RCGC  1
// #define MOZVM_MEMORY_USE_MSGC  1
#define MOZVM_NODE_ARENA_SIZE  8
#define MOZVM_NODE_USE_MEMPOOL 1
#define MOZVM_USE_FREE_LIST 1
// #define MOZVM_MEMORY_USE_BOEHM_GC 1
#define MOZVM_ENABLE_NODE_DIGEST 1
#define MOZVM_ENABLE_NEZTEST 1

// VM / bytecode
#define MOZVM_SMALL_STRING_INST 1
#define MOZVM_SMALL_TAG_INST    1
#define MOZVM_SMALL_BITSET_INST 1
#define MOZVM_SMALL_JMPTBL_INST 1
#define MOZVM_USE_JMPTBL 1
// #define MOZVM_USE_INT16_ADDR 1
// #define MOZVM_DEBUG_PROD       1
// #define MOZVM_ENABLE_JIT       1
#define MOZVM_JIT_COUNTER_THRESHOLD 1
// #define MOZVM_USE_SSE4_2        1
// #define MOZVM_USE_SWITCH_CASE_DISPATCH 1
#define MOZVM_USE_INDIRECT_THREADING   1
// #define MOZVM_USE_DIRECT_THREADING     1
// #define MOZVM_EMIT_OP_LABEL 1

#if defined(MOZVM_DEBUG_PROD) || defined(MOZVM_ENABLE_JIT)
#define MOZVM_USE_PROD 1
#endif

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

#ifdef MOZVM_PROFILE
#define MOZVM_PROFILE_DECL(X) unsigned long long _PROFILE_##X = 0;
#define MOZVM_PROFILE_INC(X)  (_PROFILE_##X)++
#define MOZVM_PROFILE_DEC(X)  (_PROFILE_##X)--
#define MOZVM_PROFILE_INC_N(X, N)  (_PROFILE_##X) += (N)
#define MOZVM_PROFILE_DEC_N(X, N)  (_PROFILE_##X) -= (N)
#define MOZVM_PROFILE_SHOW(X) fprintf(stderr, "%-10s %llu\n", #X, _PROFILE_##X);
#define MOZVM_PROFILE_ENABLE(X)
#else
#define MOZVM_PROFILE_DECL(X)
#define MOZVM_PROFILE_INC(X)
#define MOZVM_PROFILE_DEC(X)
#define MOZVM_PROFILE_INC_N(X, N)
#define MOZVM_PROFILE_DEC_N(X, N)
#define MOZVM_PROFILE_SHOW(X)
#define MOZVM_PROFILE_ENABLE(X)
#endif

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
