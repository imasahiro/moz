#include "karray.h"
#include "mozvm_config.h"
#include <assert.h>

#ifndef NODE_H
#define NODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Node *Node;
DEF_ARRAY_STRUCT0(Node, unsigned);
DEF_ARRAY_T(Node);

#define NODE_USE_NODE_PRINT 1

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
#elif defined(MOZVM_MEMORY_USE_MSGC)
#define NODE_GC_HEADER  unsigned _flag;
#define NODE_GC_INIT(O) (O)->_flag = 0
#define NODE_GC_RETAIN(O)
#define NODE_GC_RELEASE(O)
typedef struct NodeVisitor NodeVisitor;
struct NodeVisitor {
    void (*fn_visit)(NodeVisitor *visitor, Node object);
    void (*fn_visit_range)(NodeVisitor *visitor, Node *, Node *);
};

#else
#error node gc
#endif

struct Node {
    NODE_GC_HEADER;
    const char *tag;
    const char *pos;
    const char *value;
    unsigned len;
    union NodeEntry {
        struct node_small_array {
            unsigned size;
            Node ary[MOZVM_SMALL_ARRAY_LIMIT];
        } raw;
        ARRAY(Node) array;
    } entry;
};

static inline unsigned Node_length(Node o)
{
    return o->entry.raw.size;
}

Node Node_new(const char *tag, const char *str, unsigned len, unsigned elm_size, const char *value);
void Node_free(Node o);
void Node_append(Node o, Node n);
Node Node_get(Node o, unsigned index);
void Node_set(Node o, unsigned index, Node n);
#ifdef NODE_USE_NODE_PRINT
void Node_print(Node o);
#endif

#ifdef MOZVM_MEMORY_USE_RCGC
void Node_sweep(Node o);
#endif

#ifdef MOZVM_MEMORY_USE_MSGC
typedef void (*f_trace)(void *p, NodeVisitor *v);
void NodeManager_add_gc_root(void *ptr, f_trace f);
#endif


void NodeManager_init();
void NodeManager_dispose();
void NodeManager_print_stats();
void NodeManager_reset();

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
