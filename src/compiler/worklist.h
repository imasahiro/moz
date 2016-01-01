/****************************************************************************
 * Copyright (c) 2016, Masahiro Ide <imasahiro9 at gmail.com>
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

#include "core/karray.h"

#ifndef WORKLIST_H
#define WORKLIST_H

#define WORK_LIST(T, CTX_T) worklist_##T##_##CTX_T##_t
#define WORK_LIST_FUNC(T, CTX_T) worklist_##T##_##CTX_T##_func_t
#define WORK_LIST_INIT_FUNC(T, CTX_T) worklist_##T##_##CTX_T##_init_func_t

#define DEF_WORK_LIST_TYPE(T, CTX_T) \
typedef struct WORK_LIST(T, CTX_T) { \
    ARRAY(T) list; \
    CTX_T context; \
} WORK_LIST(T, CTX_T)

#define DEF_WORK_LIST_FUNC_TYPE(T, CTX_T) \
typedef int (*WORK_LIST_FUNC(T, CTX_T))(WORK_LIST(T, CTX_T) *, T); \
typedef void (*WORK_LIST_INIT_FUNC(T, CTX_T))(WORK_LIST(T, CTX_T) *, CTX_T)

#define DEF_WORK_LIST_TYPES(T, CTX_T) \
    DEF_WORK_LIST_TYPE(T, CTX_T); \
    DEF_WORK_LIST_FUNC_TYPE(T, CTX_T)

#define DEF_WORK_LIST_OP(T, CTX_T) \
static inline int worklist_##T##_##CTX_T##_empty(WORK_LIST(T, CTX_T) *W) \
{ \
    return ARRAY_size(W->list) == 0; \
} \
static inline void worklist_##T##_##CTX_T##_push(WORK_LIST(T, CTX_T) *W, T v) \
{ \
    ARRAY_add_once(T, &W->list, v); \
} \
static inline T worklist_##T##_##CTX_T##_pop(WORK_LIST(T, CTX_T) *W) \
{ \
    T v; \
    assert(!worklist_##T##_##CTX_T##_empty(W)); \
    v = *ARRAY_last(W->list); \
    ARRAY_size(W->list) -= 1; \
    return v; \
} \
static inline void worklist_##T##_##CTX_T##_init(WORK_LIST(T, CTX_T) *W, CTX_T context) \
{ \
    ARRAY_init(T, &W->list, 1); \
    W->context = context; \
} \
static inline void worklist_##T##_##CTX_T##_dispose(WORK_LIST(T, CTX_T) *W) \
{ \
    ARRAY_dispose(T, &W->list); \
} \
static inline int worklist_##T##_##CTX_T##_apply(CTX_T ctx, WORK_LIST_INIT_FUNC(T, CTX_T) init, WORK_LIST_FUNC(T, CTX_T) func) \
{ \
    int modified = 0; \
    WORK_LIST(T, CTX_T) worklist; \
    worklist_##T##_##CTX_T##_init(&worklist, ctx); \
    init(&worklist, ctx); \
    while (!worklist_##T##_##CTX_T##_empty(&worklist)) { \
        T obj = worklist_##T##_##CTX_T##_pop(&worklist); \
        modified += func(&worklist, obj); \
    } \
    worklist_##T##_##CTX_T##_dispose(&worklist); \
    return modified; \
}

#define WORK_LIST_push(T, C, W, val)  worklist_##T##_##C##_push(W, val)
#define WORK_LIST_apply(T, C, ctx, init, func) \
    worklist_##T##_##C##_apply(ctx, init, func)

#endif /* end of include guard */
