#include "node.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

DEF_ARRAY_OP_NOPOINTER(Node);

Node Node_new(unsigned elm_size)
{
    Node o = (Node) VM_MALLOC(sizeof(*o));
    o->entry.raw.size = elm_size;
    if (elm_size > NODE_SMALL_ARRAY_LIMIT) {
        ARRAY_init(Node, &o->entry.array, elm_size);
    }
    return o;
}

Node Node_get(Node o, unsigned index)
{
    unsigned len = Node_length(o);
    if (index < len) {
        if (len > NODE_SMALL_ARRAY_LIMIT) {
            return ARRAY_get(Node, &o->entry.array, index);
        }
        else {
            return o->entry.raw.ary[index];
        }
    }
    return NULL;
}

void Node_set(Node o, unsigned index, Node n)
{
    unsigned len = Node_length(o);
    assert (index < len);
    if (len > NODE_SMALL_ARRAY_LIMIT) {
        ARRAY_set(Node, &o->entry.array, index, n);
    }
    else {
        o->entry.raw.ary[index] = n;
    }
}

void Node_append(Node o, Node n)
{
    unsigned len = Node_length(o);
    if (len > NODE_SMALL_ARRAY_LIMIT) {
        ARRAY_ensureSize(Node, &o->entry.array, 1);
        ARRAY_add(Node, &o->entry.array, n);
    }
    else if (len == 2) {
        Node e0 = o->entry.raw.ary[0];
        Node e1 = o->entry.raw.ary[1];
        ARRAY_init(Node, &o->entry.array, 3);
        ARRAY_add(Node, &o->entry.array, e0);
        ARRAY_add(Node, &o->entry.array, e1);
        ARRAY_add(Node, &o->entry.array, n);
    }
    else if (len == 1) {
        o->entry.raw.size += 1;
        o->entry.raw.ary[1] = n;
    }
    else { // len == 0
        o->entry.raw.size += 1;
        o->entry.raw.ary[0] = n;
    }
}

void Node_free(Node o)
{
    unsigned i, len = Node_length(o);

    for (i = 0; i < len; i++) {
        Node node = ARRAY_get(Node, &o->entry.array, i);
        Node_free(node);
    }
    if (len > NODE_SMALL_ARRAY_LIMIT) {
        ARRAY_dispose(Node, &o->entry.array);
    }
    VM_FREE(o);
}

static void print_indent(unsigned level)
{
    unsigned i;
    for (i = 0; i < level; i++) {
        fprintf(stderr, "  ");
    }
}

static void Node_print2(Node o, unsigned level)
{
    unsigned i, len = Node_length(o);

    print_indent(level);
    if (len == 0) {
        fprintf(stderr, "[]");
        return;
    }
    else {
        fprintf(stderr, "[\n");

        for (i = 0; i < len; i++) {
            Node node = ARRAY_get(Node, &o->entry.array, i);
            print_indent(level);
            Node_print2(node, level + 1);
            fprintf(stderr, ",\n");
        }
        print_indent(level);
        fprintf(stderr, "]");
    }
}

void Node_print(Node o)
{
    Node_print2(o, 0);
    fprintf(stderr, "\n");
}

#if 0 && DEBUG
int main(int argc, char const* argv[])
{
    Node root = Node_new(0);
    Node_print(root);
    Node_free(root);
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif
