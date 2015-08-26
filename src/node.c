#include "node.h"
#include <stdio.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOZVM_PROFILE_DEFINE(F) \
    F(NODE_ALLOC) \
    F(NODE_FREE) \
    F(NODE_SWEEP) \
    F(NODE_GET) \
    F(NODE_SET) \
    F(NODE_APPEND)

MOZVM_PROFILE_EACH(MOZVM_PROFILE_DECL);

DEF_ARRAY_OP_NOPOINTER(Node);

// #define DEBUG2 1

#ifdef MOZVM_MEMORY_USE_RCGC

#if defined(MOZVM_USE_FREE_LIST) || defined(MOZVM_NODE_USE_MEMPOOL)
static Node free_list = NULL;
#endif

#ifdef MOZVM_NODE_USE_MEMPOOL
static size_t free_object_count = 0;
static struct page_header *current_page = NULL;

struct page {
#define PAGE_OBJECT_SIZE (MOZVM_NODE_ARENA_SIZE * 4096 / sizeof(struct Node)-1)
    struct Node nodes[PAGE_OBJECT_SIZE+1];
};

struct page_header {
    struct page_header *next;
};

static struct page *alloc_page()
{
    struct page *p = (struct page *)calloc(1, sizeof(*p));
    struct page_header *h = (struct page_header *)p;
    Node head = p->nodes + 1;
    Node cur  = head;
    Node tail = p->nodes + PAGE_OBJECT_SIZE;

    h->next = current_page;
    current_page = h;
    assert(free_object_count == 0);
    while (cur < tail) {
        cur->tag = (const char *)(cur + 1);
        ++cur;
    }
    free_object_count += PAGE_OBJECT_SIZE;
    tail->tag = (const char *)(free_list);
    free_list = head;
    return p;
}
#endif

void NodeManager_init()
{
#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
    unsigned offset1 = offsetof(struct Node, entry.raw.size);
    unsigned offset2 = offsetof(struct Node, entry.array.size);
    assert(offset1 == offset2);
#ifdef MOZVM_NODE_USE_MEMPOOL
    alloc_page();
#endif
}

void NodeManager_dispose()
{
#ifdef MOZVM_NODE_USE_MEMPOOL
    while (current_page) {
        struct page_header *next = current_page->next;
        free(current_page);
        current_page = next;
    }

    free_list = NULL;
    free_object_count = 0;
    current_page = NULL;
#elif defined(MOZVM_USE_FREE_LIST)
    while (free_list) {
        Node next = (Node)free_list->tag;
        VM_FREE(free_list);
        free_list = next;
    }
#ifdef NODE_CHECK_MALLOC
    if (malloc_size) {
        fprintf(stderr, "memory leak %ld byte (%ld nodes)\n",
                malloc_size, malloc_size / sizeof(struct Node));
    }
#endif
#endif /*MOZVM_NODE_USE_MEMPOOL*/
#endif /*MOZVM_USE_FREE_LIST*/
}

void NodeManager_print_stats()
{
    MOZVM_PROFILE_EACH(MOZVM_PROFILE_SHOW);
}

void NodeManager_reset()
{
    NodeManager_dispose();
    NodeManager_init();
}

static inline Node node_alloc()
{
    Node o;
#ifdef MOZVM_NODE_USE_MEMPOOL
    if (free_list == NULL) {
        alloc_page();
    }
    o = free_list;
    free_list = (Node)o->tag;
    free_object_count -= 1;
    return o;
#else
#if MOZVM_USE_FREE_LIST
    if (free_list) {
        o = free_list;
        free_list = (Node)o->tag;
        return o;
    }
#endif /*MOZVM_USE_FREE_LIST*/
    o = (Node) VM_MALLOC(sizeof(struct Node));
    return o;
#endif
}

static inline void node_free(Node o)
{
    assert(o->refc == 0);
    memset(o, 0xa, sizeof(*o));
    o->refc = -1;
#ifdef MOZVM_USE_FREE_LIST
    o->tag = (const char *)free_list;
#ifdef DEBUG2
    fprintf(stderr, "F %p -> %p\n", o, free_list);
#endif
    free_list = o;
#endif /*MOZVM_USE_FREE_LIST*/
#ifdef MOZVM_NODE_USE_MEMPOOL
    free_object_count += 1;
#endif
#if !defined(MOZVM_USE_FREE_LIST) && !defined(MOZVM_NODE_USE_MEMPOOL)
    VM_FREE(o);
#endif
}


void Node_sweep(Node o)
{
    // FIXME stack over flow
    unsigned i, len = Node_length(o);
    assert(o->refc == 0);
    for (i = 0; i < len; i++) {
        Node node = Node_get(o, i);
        if (node) {
            NODE_GC_RELEASE(node);
        }
    }
    if (len > NODE_SMALL_ARRAY_LIMIT) {
        ARRAY_dispose(Node, &o->entry.array);
    }
    MOZVM_PROFILE_INC(NODE_SWEEP);
    node_free(o);
}

Node Node_new(const char *tag, const char *str, unsigned len, unsigned elm_size, const char *value)
{
    Node o = node_alloc();
    MOZVM_PROFILE_INC(NODE_ALLOC);
#ifdef DEBUG2
    fprintf(stderr, "A %p %d\n", o, elm_size);
#endif
    NODE_GC_INIT(o);
    o->tag = tag;
    o->pos = str;
    o->len = len;
    o->value = value;
    o->entry.raw.size = elm_size;
    if (elm_size > NODE_SMALL_ARRAY_LIMIT) {
        unsigned i;
        ARRAY_init(Node, &o->entry.array, elm_size);
        for (i = 0; i < elm_size; i++) {
            ARRAY_add(Node, &o->entry.array, NULL);
        }
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
    MOZVM_PROFILE_INC(NODE_GET);
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
    assert(o != n);
    MOZVM_PROFILE_INC(NODE_SET);

    if (MOZVM_MEMORY_USE_RCGC) {
        Node v = Node_get(o, index);
        NODE_GC_RETAIN(n);
        if (v) {
            NODE_GC_RELEASE(v);
        }
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
    MOZVM_PROFILE_INC(NODE_APPEND);
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
    MOZVM_PROFILE_INC(NODE_FREE);

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
            assert(node != o);
            print_indent(level + 1);
            if (node) {
                Node_print2(node, level + 1);
            }
            else {
                fprintf(stderr, "null");
            }
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

#ifdef __cplusplus
}
#endif
