/****************************************************************************
 * Copyright (c) 2015, Masahiro Ide <imasahiro9 at gmail.com>
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

#include <stdint.h>
#ifndef BITSET_H
#define BITSET_H

#ifdef __cplusplus
extern "C" {
#endif

#define BITSET_USE_ULONG 1
#if defined(BITSET_USE_ULONG)
typedef unsigned long bitset_entry_t;
#elif defined(BITSET_USE_UINT)
typedef unsigned bitset_entry_t;
#endif

#ifndef BITS
#define BITS (sizeof(bitset_entry_t) * 8)
#endif

typedef struct bitset_t {
    bitset_entry_t data[256/BITS];
} bitset_t;

static inline bitset_t *bitset_init(bitset_t *set)
{
    unsigned i;
    for (i = 0; i < 256 / BITS; i++) {
        set->data[i] = 0;
    }
    return set;
}

static inline void bitset_set(bitset_t *set, unsigned index)
{
    bitset_entry_t mask = ((bitset_entry_t)1) << (index % BITS);
    set->data[index / BITS] |= mask;
}

static inline int bitset_get(bitset_t *set, unsigned index)
{
    bitset_entry_t mask = ((bitset_entry_t)1) << (index % BITS);
    return (set->data[index / BITS] & mask) != 0;
}

static inline void bitset_and(bitset_t *set1, bitset_t *set2)
{
    unsigned i;
    for (i = 0; i < 256 / BITS; i++) {
        set1->data[i] &= set2->data[i];
    }
}

static inline void bitset_or(bitset_t *set1, bitset_t *set2)
{
    unsigned i;
    for (i = 0; i < 256 / BITS; i++) {
        set1->data[i] |= set2->data[i];
    }
}

static inline void bitset_flip(bitset_t *set)
{
    unsigned i;
    for (i = 0; i < 256 / BITS; i++) {
        set->data[i] = ~(set->data[i]);
    }
}

static inline void bitset_copy(bitset_t *set1, bitset_t *set2)
{
    memcpy(set1, set2, sizeof(*set1));
}

static inline bool bitset_equal(bitset_t *set1, bitset_t *set2)
{
    return memcmp(set1, set2, sizeof(*set1)) == 0;
}

#if 0
#include <stdio.h>
int main(int argc, char const* argv[])
{
    bitset_t set;
    bitset_init(&set);
    bitset_set(&set, 2);
    fprintf(stderr, "%d\n", bitset_get(&set, 0));
    fprintf(stderr, "%d\n", bitset_get(&set, 1));
    fprintf(stderr, "%d\n", bitset_get(&set, 2));
    fprintf(stderr, "%d\n", bitset_get(&set, 3));
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
