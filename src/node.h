#include "karray.h"

#ifndef NODE_H
#define NODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pegvm_node *Node;
DEF_ARRAY_STRUCT0(Node, unsigned);
DEF_ARRAY_T(Node);

#define NODE_SMALL_ARRAY_LIMIT 2
struct pegvm_node {
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

Node Node_new(unsigned elm_size);
void Node_free(Node o);
void Node_append(Node o, Node n);
Node Node_get(Node o, unsigned index);
void Node_set(Node o, unsigned index, Node n);
void Node_print(Node o);

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
