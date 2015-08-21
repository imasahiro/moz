#include "karray.h"
#include "mozvm_config.h"
#include <assert.h>

#ifndef NODE_H
#define NODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pegvm_node *Node;
DEF_ARRAY_STRUCT0(Node, unsigned);
DEF_ARRAY_T(Node);

// #define MOZVM_MEMORY_USE_BOEHM_GC
#ifdef MOZVM_MEMORY_USE_BOEHM_GC
#define NODE_GC_HEADER
#define NODE_GC_INIT(O)          ((void)O)
#define NODE_GC_RETAIN(O)        ((void)O)
#define NODE_GC_RELEASE(O)       ((void)O)
// #define NODE_GC_WRITE(DST, SRC) *(DST) = (SRC)

#elif defined(MOZVM_MEMORY_USE_RCGC)
#define NODE_GC_HEADER  long refc
#define NODE_GC_INIT(O) (O)->refc = 0
#define NODE_GC_RETAIN(O)  (O)->refc++
#define NODE_GC_RELEASE(O) do {\
    assert((O)->refc > 0);\
    (O)->refc--; \
    if ((O)->refc == 0) { \
        Node_sweep((O)); \
    } \
} while (0)

#if 0
#define NODE_GC_WRITE(DST, SRC) do {\
    NODE_GC_RETAIN(SRC); \
    NODE_GC_RELEASE(DST); \
    (DST) = (SRC); \
} while (0)
#endif
#else
#error node gc
#endif

#define NODE_SMALL_ARRAY_LIMIT 2
struct pegvm_node {
    NODE_GC_HEADER;
    char *tag;
    char *pos;
    char *value;
    unsigned len;
#if 1
    union entry {
        struct node_small_array {
            unsigned size;
            Node ary[NODE_SMALL_ARRAY_LIMIT];
        } raw;
        ARRAY(Node) array;
    } entry;
#else
    ARRAY(Node) array;
#endif
};

static inline unsigned Node_length(Node o)
{
    return o->entry.raw.size;
}

Node Node_new(char *tag, char *str, unsigned len, unsigned elm_size, char *value);
void Node_free(Node o);
void Node_append(Node o, Node n);
Node Node_get(Node o, unsigned index);
void Node_set(Node o, unsigned index, Node n);
void Node_print(Node o);
#ifdef MOZVM_MEMORY_USE_RCGC
void Node_sweep(Node o);
#endif

void NodeManager_init();
void NodeManager_dispose();

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
