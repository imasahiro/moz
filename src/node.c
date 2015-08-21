#include "node.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

DEF_ARRAY_OP_NOPOINTER(Node);

#ifdef MOZVM_MEMORY_USE_RCGC
static Node free_list = NULL;

void NodeManager_init()
{
}

void NodeManager_dispose()
{
    while (free_list) {
        Node next = (Node)free_list->tag;
        VM_FREE(free_list);
        free_list = next;
    }
}

static inline Node node_alloc()
{
    if (free_list) {
        Node o = free_list;
        free_list = (Node)o->tag;
        return o;
    }
    return (Node) VM_MALLOC(sizeof(struct pegvm_node));
}

static inline void node_free(Node o)
{
    assert(o->refc == 0);
    memset(o, 0xa, sizeof(*o));
    o->refc = -1;
    o->tag = (char *)free_list;
    free_list = o;
}

void Node_sweep(Node o)
{
    // FIXME stack over flow
    unsigned i, len = Node_length(o);
    for (i = 0; i < len; i++) {
        Node node = Node_get(o, i);
        NODE_GC_RELEASE(node);
    }
    node_free(o);
}

#endif

Node Node_new(char *tag, char *str, unsigned len, unsigned elm_size, char *value)
{
    Node o = node_alloc();
    o->tag = tag;
    o->pos = str;
    o->len = len;
    o->value = value;

    // assert(o->len < 100);
    o->entry.raw.size = elm_size;
    if (elm_size > NODE_SMALL_ARRAY_LIMIT) {
        ARRAY_init(Node, &o->entry.array, elm_size);
    }
    else {
        o->entry.raw.ary[0] = NULL;
        o->entry.raw.ary[1] = NULL;
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
    unsigned len;

    if (MOZVM_MEMORY_USE_RCGC) {
        Node v = Node_get(o, index);
        if (v) {
            NODE_GC_RELEASE(v);
        }
        NODE_GC_RETAIN(n);
    }
    while (index >= Node_length(o)) {
        Node_append(o, NULL);
    }
    len = Node_length(o);
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
    if (n) {
        NODE_GC_RETAIN(n);
    }
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
    fprintf(stderr, "#%s", o->tag);
    if (len == 0) {
        fprintf(stderr, "['%.*s']", o->len, o->pos);
        return;
    }
    else {
        fprintf(stderr, "[\n");

        for (i = 0; i < len; i++) {
            Node node = Node_get(o, i);
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
    // fprintf(stderr, "%d\n", sizeof(*o));
    Node_print2(o, 0);
    fprintf(stderr, "\n");
}

#ifdef __cplusplus
}
#endif
