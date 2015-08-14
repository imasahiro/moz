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

#define MOZVM_MEMORY_USE_BOEHM_GC 0

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
