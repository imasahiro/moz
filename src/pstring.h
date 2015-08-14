/****************************************************************************
 * Copyright (c) 2014, Masahiro Ide <imasahiro9 at gmail.com>
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


#ifndef PSTRING_H_
#define PSTRING_H_

#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <x86intrin.h>

#ifndef OFFSET_OF
#define OFFSET_OF(TYPE, FIELD) ((unsigned long)&(((TYPE *)0)->FIELD))
#endif

#ifndef CONTAINER_OF
#define CONTAINER_OF(PTR, TYPE, FIELD) ((TYPE *)((char *)(PTR) - OFFSET_OF(TYPE, FIELD)))
#endif

#define PSTRING_PTR(STR) ((STR)->str)

typedef struct pstring_t {
    unsigned len;
    char str[1];
} pstring_t;

static inline const char *pstring_alloc(const char *t, unsigned len)
{
    pstring_t *str = (pstring_t *) malloc(sizeof(pstring_t) + len);
    str->len = len;
    memcpy(PSTRING_PTR(str), t, len);
    return PSTRING_PTR(str);
}

static inline const char *pstring_alloc2(unsigned len)
{
    pstring_t *str = (pstring_t *) malloc(sizeof(pstring_t) + len);
    str->len = len;
    return PSTRING_PTR(str);
}

static inline void pstring_delete(const char *s)
{
    pstring_t *str = CONTAINER_OF(s, pstring_t, str);
    free(str);
}

static inline unsigned pstring_length(const char *s)
{
    pstring_t *str = CONTAINER_OF(s, pstring_t, str);
    return str->len;
}

#if defined(PSTRING_C_TEST) || defined(PSTRING_USE_STRCMP)
static inline int pstring_starts_with_strcmp(const char *p, const char *text, unsigned len)
{
    return (strncmp(p, text, len) == 0);
}
#endif

static inline int pstring_starts_with_simple(const char *p, const char *text, unsigned len)
{
    const char *end = text + len;
    while (text < end) {
        if (*p++ != *text++) {
            return 0;
        }
    }
    return 1;
}

#ifdef __AVX2__
static inline int pstring_starts_with_avx2(const char *str, const char *text, unsigned len)
{
    uint64_t m, mask;
    __m256i s = _mm256_loadu_si256((const __m256i *)str);
    __m256i t = _mm256_loadu_si256((const __m256i *)text);
    assert(len <= 32);
    s = _mm256_cmpeq_epi8(s, t);
    m = _mm256_movemask_epi8(s);
    mask = ((uint64_t)1 << len) - 1;
    return ((m & mask) == mask);
}
#endif

static inline int pstring_starts_with(const char *str, const char *text, unsigned len)
{
#ifdef __AVX2__
    if (len <= 32) {
        return pstring_starts_with_avx2(str, text, len);
    }
    else
#endif
    {
#if defined(PSTRING_USE_STRCMP)
        return pstring_starts_with_strcmp(str, text, len);
#else
        return pstring_starts_with_simple(str, text, len);
#endif
    }
}

#if 0
static int pstring_starts_with_(pstring_t *str, const char *text, unsigned len)
{
    const char *p = (const char *)PSTRING_PTR(str);
    return pstring_starts_with(p, text, len);
}

static int pstring_starts_with__(pstring_t *str1, pstring_t *str2)
{
    return pstring_starts_with_(str1, (const char *)PSTRING_PTR(str2), str2->len);
}
#endif

#endif /* end of include guard */
