#include "node.h"
#include "pstring.h"
#include <stdio.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif
DEF_ARRAY_OP_NOPOINTER(NodePtr);

#define MOZVM_NODE_PROFILE_EACH(F) \
    F(NODE_ALLOC) \
    F(NODE_FREE) \
    F(NODE_SWEEP) \
    F(NODE_GET) \
    F(NODE_SET) \
    F(NODE_APPEND)

MOZVM_NODE_PROFILE_EACH(MOZVM_PROFILE_DECL);

// #define DEBUG2 1

static inline Node *node_alloc();

Node *Node_new(const char *tag, const char *str, unsigned len, unsigned elm_size, const char *value)
{
    Node *o = node_alloc();
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
    if (elm_size > MOZVM_SMALL_ARRAY_LIMIT) {
        unsigned i;
        ARRAY_init(NodePtr, &o->entry.array, elm_size);
        for (i = 0; i < elm_size; i++) {
            ARRAY_add(NodePtr, &o->entry.array, NULL);
        }
    }
    else {
        o->entry.raw.ary[0] = NULL;
        o->entry.raw.ary[1] = NULL;
    }
    return o;
}

Node *Node_get(Node *o, unsigned index)
{
    unsigned len = Node_length(o);
    MOZVM_PROFILE_INC(NODE_GET);
    if (index < len) {
        if (len > MOZVM_SMALL_ARRAY_LIMIT) {
            return ARRAY_get(NodePtr, &o->entry.array, index);
        }
        else {
            return o->entry.raw.ary[index];
        }
    }
    return NULL;
}

void Node_set(Node *o, unsigned index, uint16_t labelId, Node *n)
{
    unsigned len;
    assert(o != n);
    MOZVM_PROFILE_INC(NODE_SET);

#ifdef MOZVM_MEMORY_USE_RCGC
    Node *v = Node_get(o, index);
    NODE_GC_RETAIN(n);
    if (v) {
        NODE_GC_RELEASE(v);
    }
#endif
    n->labelId = labelId;
    while (index >= Node_length(o)) {
        Node_append(o, NULL);
    }
    len = Node_length(o);
    if (len > MOZVM_SMALL_ARRAY_LIMIT) {
        ARRAY_set(NodePtr, &o->entry.array, index, n);
    }
    else {
        o->entry.raw.ary[index] = n;
    }
}

void Node_append(Node *o, Node *n)
{
    unsigned len = Node_length(o);
    MOZVM_PROFILE_INC(NODE_APPEND);
    if (n) {
        NODE_GC_RETAIN(n);
    }
    if (len > MOZVM_SMALL_ARRAY_LIMIT) {
        ARRAY_ensureSize(NodePtr, &o->entry.array, 1);
        ARRAY_add(NodePtr, &o->entry.array, n);
    }
    else if (len == 2) {
        Node *e0 = o->entry.raw.ary[0];
        Node *e1 = o->entry.raw.ary[1];
        ARRAY_init(NodePtr, &o->entry.array, 3);
        ARRAY_add(NodePtr, &o->entry.array, e0);
        ARRAY_add(NodePtr, &o->entry.array, e1);
        ARRAY_add(NodePtr, &o->entry.array, n);
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

static inline const char *Node_label(Node *o, const char **tag_list)
{
    assert(o->labelId != NODE_LABEL_UNDEF);
    return tag_list[o->labelId];
}

void Node_free(Node *o)
{
    unsigned i, len = Node_length(o);
    MOZVM_PROFILE_INC(NODE_FREE);

    for (i = 0; i < len; i++) {
        Node *node= ARRAY_get(NodePtr, &o->entry.array, i);
        Node_free(node);
    }
    if (len > MOZVM_SMALL_ARRAY_LIMIT) {
        ARRAY_dispose(NodePtr, &o->entry.array);
    }
    VM_FREE(o);
}

#ifdef NODE_USE_NODE_PRINT
static void print_indent(unsigned level)
{
    unsigned i;
    for (i = 0; i < level; i++) {
        fprintf(stderr, "   ");
    }
}

static void Node_print2(Node *o, const char **tag_list, unsigned level)
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
            Node *node = Node_get(o, i);
            assert(node != o);
            //print_indent(level + 1);
            if (node) {
                Node_print2(node, tag_list, level + 1);
            }
            else {
                fprintf(stderr, "null");
            }
            fprintf(stderr, "\n");
        }
        print_indent(level);
        fprintf(stderr, "]");
    }
}

void Node_print(Node *o, const char **tag_list)
{
    Node_print2(o, tag_list, 0);
    fprintf(stderr, "\n");
}
#endif

#ifdef MOZVM_ENABLE_NODE_DIGEST
#include "md5.c"

static void Node_digest2(Node *o, const char **tag_list, md5_state_t *ctx)
{
    unsigned i, len = Node_length(o);
    md5_append(ctx, (unsigned char *)"#", 1);
    if (o->tag) {
        unsigned tlen = pstring_length(o->tag);
        md5_append(ctx, (unsigned char *)o->tag, tlen);
    }

    for (i = 0; i < len; i++) {
        Node *node = Node_get(o, i);
        assert(node != o);
        if (node) {
            const char *label = Node_label(node, tag_list);
            if (label[0] != 0) {
                md5_append(ctx, (unsigned char *)"$", 1);
                md5_append(ctx, (unsigned char *)label, pstring_length(label));
            }
            Node_digest2(node, tag_list, ctx);
        }
    }
    if (len == 0) {
        if (o->value) {
            unsigned slen = pstring_length(o->value);
            assert(0 && "XXX: need to test");
            md5_append(ctx, (unsigned char *)o->value, slen);
        }
        else {
            md5_append(ctx, (unsigned char *)o->pos, o->len);
        }
    }
}

void Node_digest(Node *o, const char **tag_list, unsigned char buf[32])
{
    int i;
    md5_state_t ctx;
    unsigned char tmp[16];
    unsigned char *p = buf;
    md5_init(&ctx);
    Node_digest2(o, tag_list, &ctx);
    md5_finish(&ctx, tmp);
    for (i = 0; i < 16; i++) {
        uint8_t d = tmp[i];
        *p++ = "0123456789abcdef"[(d >> 4) & 0xf];
        *p++ = "0123456789abcdef"[0xf & d];
    }
}
#endif

#ifdef MOZVM_MEMORY_USE_RCGC
#if defined(MOZVM_USE_FREE_LIST) || defined(MOZVM_NODE_USE_MEMPOOL)
static Node *free_list = NULL;
#endif

#ifdef MOZVM_NODE_USE_MEMPOOL
static size_t free_object_count = 0;
static struct page_header *current_page = NULL;
#ifdef MOZVM_PROFILE
static uint64_t max_arena_size = 0;
static uint64_t arena_size = 0;
#endif

struct page {
#define PAGE_OBJECT_SIZE (MOZVM_NODE_ARENA_SIZE * 4096 / sizeof(Node)-1)
    Node nodes[PAGE_OBJECT_SIZE+1];
};

struct page_header {
    struct page_header *next;
};

static struct page *alloc_page()
{
    struct page *p = (struct page *)calloc(1, sizeof(*p));
    struct page_header *h = (struct page_header *)p;
    Node *head = p->nodes + 1;
    Node *cur  = head;
    Node *tail = p->nodes + PAGE_OBJECT_SIZE;

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
#ifdef MOZVM_PROFILE
    arena_size += 1;
#endif
    return p;
}
#endif

void NodeManager_init()
{
#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
    unsigned offset1 = offsetof(Node, entry.raw.size);
    unsigned offset2 = offsetof(Node, entry.array.size);
    assert(offset1 == offset2);
#ifdef NDEBUG
    (void)offset1; (void)offset2;
#endif
#ifdef MOZVM_NODE_USE_MEMPOOL
    alloc_page();
#endif
}

void NodeManager_dispose()
{
#ifdef MOZVM_NODE_USE_MEMPOOL
#ifdef MOZVM_PROFILE
    if (max_arena_size < arena_size) {
        max_arena_size = arena_size;
    }
#endif
    while (current_page) {
        struct page_header *next = current_page->next;
        free(current_page);
        current_page = next;
#ifdef MOZVM_PROFILE
        arena_size -= 1;
#endif
    }

    free_list = NULL;
    free_object_count = 0;
    current_page = NULL;
#elif defined(MOZVM_USE_FREE_LIST)
    while (free_list) {
        Node *next = (Node)free_list->tag;
        VM_FREE(free_list);
        free_list = next;
    }
#ifdef NODE_CHECK_MALLOC
    if (malloc_size) {
        fprintf(stderr, "memory leak %ld byte (%ld nodes)\n",
                malloc_size, malloc_size / sizeof(struct _Node));
    }
#endif
#endif /*MOZVM_NODE_USE_MEMPOOL*/
}

void NodeManager_print_stats()
{
#ifdef MOZVM_PROFILE
    fprintf(stderr, "%-10s %llu\n", "MAX_ARENA_SIZE", max_arena_size);
    fprintf(stderr, "%-10s %lu\n", "NODE_PER_ARENA", PAGE_OBJECT_SIZE);
#endif
    MOZVM_NODE_PROFILE_EACH(MOZVM_PROFILE_SHOW);
}

void NodeManager_reset()
{
    NodeManager_dispose();
    NodeManager_init();
}

static inline Node *node_alloc()
{
    Node *o;
#ifdef MOZVM_NODE_USE_MEMPOOL
    if (free_list == NULL) {
        alloc_page();
    }
    o = free_list;
    free_list = (Node *)o->tag;
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
    o = (Node) VM_MALLOC(sizeof(struct _Node));
    return o;
#endif
}

static inline void node_free(Node *o)
{
    assert(o->refc == 0);
#ifdef DEBUG2
    memset(o, 0xa, sizeof(*o));
#endif
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


void Node_sweep(Node *o)
{
    // FIXME stack over flow
    unsigned i, len = Node_length(o);
    assert(o->refc == 0);
    for (i = 0; i < len; i++) {
        Node *node= Node_get(o, i);
        if (node) {
            NODE_GC_RELEASE(node);
        }
    }
    if (len > MOZVM_SMALL_ARRAY_LIMIT) {
        ARRAY_dispose(NodePtr, &o->entry.array);
    }
    MOZVM_PROFILE_INC(NODE_SWEEP);
    node_free(o);
}
#elif defined(MOZVM_MEMORY_USE_MSGC)
#include "gc.c"
#endif

#ifdef __cplusplus
}
#endif
