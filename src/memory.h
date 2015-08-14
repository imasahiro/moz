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

#include "mozvm_config.h"

#ifndef VM_MEMORY_H
#define VM_MEMORY_H

#if defined(HAVE_GC_GC_H) && MOZVM_MEMORY_USE_BOEHM_GC
#include <gc/gc.h>
#define VM_MALLOC(N)        GC_MALLOC(N)
#define VM_REALLOC(PTR, N)  GC_REALLOC(PTR, N)
#define VM_FREE(PTR)        ((void)PTR)

#elif MOZVM_MEMORY_USE_REFCOUNT
#endif

#ifndef VM_MALLOC
#define VM_MALLOC(N) malloc(N)
#endif

#ifndef VM_FREE
#define VM_FREE(PTR) free(PTR)
#endif

#endif /* end of include guard */
