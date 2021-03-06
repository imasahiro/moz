/****************************************************************************
 * Copyright (c) 2012-2013, Masahiro Ide <imasahiro9 at gmail.com>
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************/

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "memory.h"

#ifndef KJSON_MALLOC
#define KJSON_MALLOC(N) VM_MALLOC(N)
#define KJSON_REALLOC(PTR, N) VM_REALLOC(PTR, N)
#define KJSON_FREE(PTR) VM_FREE(PTR)
#endif

#ifndef KJSON_ARRAY_H_
#define KJSON_ARRAY_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOG2
#define LOG2(N) ((unsigned)((sizeof(void *) * 8) - __builtin_clzl((N) - 1)))
#endif

/* ARRAY */
#define ARRAY(T) ARRAY_##T##_t
#define DEF_ARRAY_STRUCT0(T, SizeTy)\
struct ARRAY(T) {\
    SizeTy size;\
    SizeTy capacity;\
    T *list;\
}
#define DEF_ARRAY_STRUCT(T) DEF_ARRAY_STRUCT0(T, unsigned)

#define DEF_ARRAY_T(T)\
struct ARRAY(T);\
typedef struct ARRAY(T) ARRAY(T)

#define DEF_ARRAY_OP__(T, ValueType)\
__attribute__((unused))\
static inline ARRAY(T) *ARRAY_init_##T (ARRAY(T) *a, size_t initsize) {\
    a->list = (T *) KJSON_MALLOC(sizeof(T)*initsize);\
    a->capacity  = initsize;\
    a->size  = 0;\
    return a;\
}\
__attribute__((unused))\
static inline void ARRAY_##T##_ensureSize(ARRAY(T) *a, size_t size) {\
    size_t capacity = a->capacity; \
    if(a->size + size <= (unsigned long)a->capacity) {\
        return;\
    }\
    while(a->size + size > a->capacity) {\
        a->capacity = 1 << LOG2(a->capacity * 2 + 1);\
    }\
    a->list = (T *)KJSON_REALLOC(a->list, sizeof(T) * a->capacity);\
    memset(a->list + capacity, 0, sizeof(T) * (a->capacity - capacity));\
}\
__attribute__((unused))\
static inline void ARRAY_##T##_dispose(ARRAY(T) *a) {\
    KJSON_FREE(a->list);\
    a->size     = 0;\
    a->capacity = 0;\
    a->list     = NULL;\
}\
__attribute__((unused))\
static inline void ARRAY_##T##_add(ARRAY(T) *a, ValueType v) {\
    ARRAY_##T##_ensureSize(a, 1);\
    ARRAY_##T##_set(a, a->size++, v);\
}\
__attribute__((unused))\
static inline void ARRAY_##T##_add_once(ARRAY(T) *a, ValueType v) {\
    int i; \
    for (i = 0; i < (int)a->size; i++) { \
        ValueType o = ARRAY_##T##_get(a, i); \
        if (v == o) { \
            return; \
        } \
    } \
    ARRAY_##T##_add(a, v); \
} \
__attribute__((unused))\
static inline void ARRAY_##T##_remove(ARRAY(T) *a, int idx) {\
    assert(a->size > 0); \
    memmove(a->list+idx, a->list + idx + 1, sizeof(T) * (a->size - idx - 1));\
    a->size -= 1;\
}\
__attribute__((unused))\
static inline void ARRAY_##T##_remove2(ARRAY(T) *a, ValueType o) {\
    int i; \
    assert(a->size > 0); \
    for (i = 0; i < (int)a->size; i++) { \
        ValueType v = ARRAY_##T##_get(a, i); \
        if (v == o) { \
            ARRAY_##T##_remove(a, i); \
            break; \
        } \
    } \
}\
__attribute__((unused))\
static inline void ARRAY_##T##_insert(ARRAY(T) *a, int idx, ValueType v) {\
    assert((int)a->size >= idx && idx >= 0); \
    if ((int)a->size == idx) { \
        ARRAY_##T##_add(a, v); \
    } \
    else { \
        ARRAY_##T##_ensureSize(a, 1);\
        memmove(a->list + idx + 1, a->list + idx, sizeof(T) * (a->size - idx));\
        a->size += 1;\
        ARRAY_##T##_set(a, idx, v);\
    } \
}\
__attribute__((unused))\
static inline int ARRAY_##T##_index(ARRAY(T) *a, ValueType o) {\
    int i; \
    assert(a->size > 0); \
    for (i = 0; i < (int)a->size; i++) { \
        ValueType v = ARRAY_##T##_get(a, i); \
        if (v == o) { \
            return i; \
        } \
    } \
    return -1; \
}

#define DEF_ARRAY_OP(T)\
static inline T *ARRAY_##T##_get(ARRAY(T) *a, int idx) {\
    return a->list+idx;\
}\
static inline void ARRAY_##T##_set(ARRAY(T) *a, int idx, T *v) {\
    memcpy(a->list+idx, v, sizeof(T));\
}\
DEF_ARRAY_OP__(T, T *)

#define DEF_ARRAY_OP_NOPOINTER(T)\
static inline T ARRAY_##T##_get(ARRAY(T) *a, int idx) {\
    return a->list[idx];\
}\
static inline void ARRAY_##T##_set(ARRAY(T) *a, int idx, T v) {\
    a->list[idx] = v;\
}\
DEF_ARRAY_OP__(T, T)

#define DEF_ARRAY_S_T(T) DEF_ARRAY_STRUCT(T); DEF_ARRAY_T(T)
#define DEF_ARRAY_T_OP(T)           DEF_ARRAY_S_T(T); DEF_ARRAY_OP(T)
#define DEF_ARRAY_T_OP_NOPOINTER(T) DEF_ARRAY_S_T(T); DEF_ARRAY_OP_NOPOINTER(T)

#define ARRAY_get(T, a, idx)     ARRAY_##T##_get(a, idx)
#define ARRAY_set(T, a, idx, v)  ARRAY_##T##_set(a, idx, v)
#define ARRAY_add(T, a, v)       ARRAY_##T##_add(a, v)
#define ARRAY_add_once(T, a, v)  ARRAY_##T##_add_once(a, v)
#define ARRAY_insert(T, a, n, v) ARRAY_##T##_insert(a, n, v)
#define ARRAY_index(T, a, v)     ARRAY_##T##_index(a, v)
#define ARRAY_remove(T, a, n)    ARRAY_##T##_remove(a, n)
#define ARRAY_remove_element(T, a, o) ARRAY_##T##_remove2(a, o)
#define ARRAY_dispose(T, a)      ARRAY_##T##_dispose(a)
#define ARRAY_init(T, a, s)      ARRAY_init_##T (a, s)
#define ARRAY_pop(T, a)          ARRAY_get(T, (a), (--(a)->size))
#define ARRAY_ensureSize(T, a, size) ARRAY_##T##_ensureSize(a, size)
#define ARRAY_n(a, n)  ((a).list+n)
#define ARRAY_size(a)  ((a).size)
#define ARRAY_first(a) ARRAY_n(a, 0)
#define ARRAY_last(a)  ARRAY_n(a, ((a).size-1))
#define ARRAY_init_1(T, a, e1) do {\
    ARRAY_init(T, a, 4);\
    ARRAY_add(T, a, e1);\
} while(0)

#define FOR_EACH_ARRAY_(a, x, i)\
    for(i = 0, x = ARRAY_n(a, i); i < ARRAY_size(a); x = ARRAY_n(a,(++i)))

#define ARRAY_BEGIN(A) ARRAY_n(A, 0)
#define ARRAY_END(A)   ARRAY_n(A, ARRAY_size(A))

#define FOR_EACH_ARRAY(a, x, e)\
    for(x = ARRAY_BEGIN(a), e = ARRAY_END(a); x != e; ++x)

#define FOR_EACH_ARRAY_R(a, x, e)\
    if (ARRAY_size(a) == 0) {} else \
        for(x = ARRAY_last(a), e = ARRAY_n(a, -1); x != e; --x)

#ifdef __cplusplus
}
#endif
#endif /* end of include guard */
