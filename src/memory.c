#include "memory.h"

#include <stdlib.h>

#if defined(MOZVM_PROFILE) && defined(MOZVM_MEMORY_PROFILE)
static uint64_t profile[MOZVM_MM_PROF_EVENT_MAX] = {};

#define MOZVM_MM_PROFILE_EACH(F) \
    F(MM_MEMORY_MALLOCED)

MOZVM_MM_PROFILE_EACH(MOZVM_PROFILE_DECL);

void *mozvm_mm_malloc(size_t size)
{
    size_t *p = (size_t *) malloc(size + sizeof(size_t));
    MOZVM_PROFILE_INC_N(MM_MEMORY_MALLOCED, size);
    p[0] = size;
    p += 1;
    return (void *)p;
}

void *mozvm_mm_calloc(size_t count, size_t size)
{
    size_t *p = (size_t *) calloc(count, size + sizeof(size_t));
    MOZVM_PROFILE_INC_N(MM_MEMORY_MALLOCED, size);
    p[0] = size;
    p += 1;
    return (void *)p;
}

void *mozvm_mm_realloc(void *ptr, size_t size)
{
    size_t *p = ((size_t *)ptr);
    MOZVM_PROFILE_INC_N(MM_MEMORY_MALLOCED, size);
    if (p) {
        p -= 1;
        MOZVM_PROFILE_DEC_N(MM_MEMORY_MALLOCED, p[0]);
    }
    p = (size_t *) realloc(p, size + sizeof(size_t));
    p[0] = size;
    p += 1;
    return (void *)p;
}

void mozvm_mm_free(void *ptr)
{
    size_t *p = ((size_t *)ptr) - 1;
    MOZVM_PROFILE_DEC_N(MM_MEMORY_MALLOCED, p[0]);
    free(p);
}

void mozvm_mm_snapshot(unsigned event_id)
{
    profile[event_id] = _PROFILE_MM_MEMORY_MALLOCED;
}

void mozvm_mm_print_stats()
{
#define UNIT "KB"
#define SIZE (1024)
    fprintf(stderr, "input txt loaded    %llu " UNIT "\n",
            profile[MOZVM_MM_PROF_EVENT_INPUT_LOAD     ] / SIZE);
    fprintf(stderr, "runtime initialized %llu " UNIT "\n",
            profile[MOZVM_MM_PROF_EVENT_RUNTIME_INIT   ] / SIZE);
    fprintf(stderr, "bytecode loaded     %llu " UNIT "\n",
            profile[MOZVM_MM_PROF_EVENT_BYTECODE_LOAD  ] / SIZE);
    fprintf(stderr, "parse start         %llu " UNIT "\n",
            profile[MOZVM_MM_PROF_EVENT_PARSE_START    ] / SIZE);
    fprintf(stderr, "parse finished      %llu " UNIT "\n",
            profile[MOZVM_MM_PROF_EVENT_PARSE_END      ] / SIZE);
    fprintf(stderr, "parse result gc     %llu " UNIT "\n",
            profile[MOZVM_MM_PROF_EVENT_GC_EXECUTED   ] / SIZE);
}

#endif /*MOZVM_MEMORY_PROFILE*/
