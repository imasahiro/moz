#include "ast.h"
#include "karray.h"
#include "node.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum AstLogType {
    TypeCapture  = 0xf,
    TypeTag      = 1,
    TypePop      = 2,
    TypeReplace  = 3,
    TypePush     = 4,
    TypeLeftFold = 5,
    TypeNew      = 6,
    TypeLink     = 7
};
#define TypeMask (0xfUL)
#define DEBUG2 1

typedef struct AstLog {
#ifdef DEBUG2
    unsigned id;
#endif
    union ast_log_entry {
        uintptr_t val;
        Node ref;
    } e;
    char *pos;
} AstLog;

DEF_ARRAY_STRUCT0(AstLog, unsigned);
DEF_ARRAY_T(AstLog);
DEF_ARRAY_OP(AstLog);

static inline void SetTag(AstLog *log, enum AstLogType type)
{
    assert((log->e.val & TypeMask) == 0);
    log->e.val |= TypeMask & type;
}

static inline enum AstLogType GetTag(AstLog *log)
{
    uintptr_t tag = log->e.val & TypeMask;
    assert(tag && "this log do not have tagged");
    return (enum AstLogType) tag;
}

static inline char *GetPos(AstLog *log)
{
    enum AstLogType tag = GetTag(log);
    assert(tag != TypePop);
    return ((tag & 1) == 0) ? log->pos : NULL;

}

static inline Node GetNode(AstLog *log)
{
    uintptr_t val = log->e.val & ~TypeMask;
    return (Node) val;
}

struct AstMachine {
    ARRAY(AstLog) logs;
    Node last_linked;
    Node parsed;
    char *source;
};

AstMachine *AstMachine_init(unsigned log_size, char *source)
{
    AstMachine *ast = (AstMachine *)malloc(sizeof(*ast));
    ARRAY_init(AstLog, &ast->logs, log_size);
    ARRAY_ensureSize(AstLog, &ast->logs, log_size);
    ast->last_linked = NULL;
    ast->source = source;
    return ast;
}

void AstMachine_dispose(AstMachine *ast)
{
    ARRAY_dispose(AstLog, &ast->logs);
    free(ast);
}

void AstMachine_setSource(AstMachine *ast, char *source)
{
    ast->source = source;
}

#ifdef DEBUG2
static void AstMachine_dumpLog(AstMachine *ast)
{
    AstLog *cur  = ARRAY_BEGIN(ast->logs);
    AstLog *tail = ARRAY_last(ast->logs);
    unsigned i = 0;
    for (; cur <= tail; ++cur) {
        unsigned id = cur->id;
        switch(GetTag(cur)) {
        case TypeNew:
            fprintf(stderr, "[%d] %02d new(%ld)\n", i, id, GetPos(cur) - ast->source);
            break;
        case TypeCapture:
            fprintf(stderr, "[%d] %02d cap(%ld)\n", i, id, (long)GetPos(cur));
            break;
        case TypeTag:
            fprintf(stderr, "[%d] %02d tag(%s)\n", i, id, cur->pos);
            break;
        case TypeReplace:
            fprintf(stderr, "[%d] %02d replace(%s)\n", i, id, cur->pos);
            break;
        case TypeLeftFold:
            fprintf(stderr, "[%d] %02d swap()\n", i, id);
            break;
        case TypePop:
            fprintf(stderr, "[%d] %02d pop(%ld)\n", i, id, (long)cur->pos);
            break;
        case TypePush:
            fprintf(stderr, "[%d] %02d push()\n", i, id);
            break;
        case TypeLink:
            fprintf(stderr, "[%d] %02d link(%ld)\n", i, id, (long)cur->pos);
            break;
        }
        ++i;
    }
}
#endif

#ifdef DEBUG2
static unsigned last_id = 1;
#endif

static void ast_log(AstMachine *ast, enum AstLogType type, char *cur, uintptr_t val)
{
    AstLog log = {};
    log.e.val = val;
    log.pos = cur;
#ifdef DEBUG2
    log.id = last_id;
    last_id++;
#endif
    SetTag(&log, type);
    ARRAY_add(AstLog, &ast->logs, &log);
}

void ast_log_new(AstMachine *ast, char *cur)
{
    ast_log(ast, TypeNew, cur, 0);
}

void ast_log_capture(AstMachine *ast, char *cur)
{
    ast_log(ast, TypeCapture, cur, 0);
}

void ast_log_tag(AstMachine *ast, char *tag)
{
    ast_log(ast, TypeTag, tag, 0);
    // assert(((uintptr_t)tag & TypeMask) == 0);
    // ast_log(ast, TypeTag, NULL, (uintptr_t)tag);
}

void ast_log_replace(AstMachine *ast, char *str)
{
    ast_log(ast, TypeReplace, str, 0);
    // assert(((uintptr_t)str & TypeMask) == 0);
    // ast_log(ast, TypeReplace, NULL, (uintptr_t)str);
}

void ast_log_swap(AstMachine *ast, char *pos)
{
    ast_log(ast, TypeLeftFold, pos, 0);
}

void ast_log_push(AstMachine *ast)
{
    ast_log(ast, TypePush, NULL, 0);
}

void ast_log_pop(AstMachine *ast, int index)
{
    intptr_t val = (intptr_t) index;
    ast_log(ast, TypePop, (char *)val, 0);
}

void ast_log_link(AstMachine *ast, int index, Node node)
{
    uintptr_t val = (uintptr_t) index;
    ast_log(ast, TypeLink, (char *)val, (uintptr_t)node);
    ast->last_linked = node;
}

Node ast_get_last_linked_node(AstMachine *ast)
{
    return ast->last_linked;
}

long ast_save_tx(AstMachine *ast)
{
    return ARRAY_size(ast->logs);
}

void ast_rollback_tx(AstMachine *ast, long tx)
{
    ARRAY_size(ast->logs) = tx;
}

Node constructLeft(AstMachine *ast, AstLog *cur, AstLog *tail, char *spos, char *epos, long objectSize, char *tag, char *value)
{
    long len = epos - spos;
    Node newnode = Node_new(tag, spos, len, objectSize, value);
    if(objectSize == 0) {
        return newnode;
    }
    for (; cur <= tail; ++cur) {
        if(GetTag(cur) == TypeLink) {
            long pos = (long)cur->pos;
            Node child = GetNode(cur);
            if(child == NULL) {
                fprintf(stderr, "@@ linking null child at %ld\n", pos);
            }
            else if (pos < 0) {
                Node_append(newnode, child);
            }
            else {
                Node_set(newnode, pos, child);
            }
        }
    }
    return newnode;
}

static Node ast_create_node(AstMachine *ast, AstLog *cur, AstLog *pushed)
{
    AstLog *head, *tail;
    char *spos, *epos;
    Node tmp;
    char *tag = NULL;
    char *value = NULL;
    long objectSize = 0;
    long index = 0;

    head = cur;
    tail = ARRAY_last(ast->logs);
    spos = (char *)GetPos(cur); epos = spos;
#ifdef DEBUG2
    fprintf(stderr, "createNode.start id=%d\n", cur->id);
#endif
    for (; cur <= tail; ++cur) {
        switch(GetTag(cur)) {
        case TypeNew:
            spos = cur->pos;
            epos = spos;
            objectSize = 0;
            tag = NULL;
            value = NULL;
            head = cur;
            break;
        case TypeCapture:
            epos = cur->pos;
            break;
        case TypeTag:
            tag = cur->pos;
            break;
        case TypeReplace:
            value = cur->pos;
            break;
        case TypeLeftFold:
            tmp = constructLeft(ast, head, cur, spos, epos, objectSize, tag, value);
            NODE_GC_RETAIN(tmp);
            cur->e.ref = tmp;
            cur->pos = 0;
            SetTag(cur, TypeLink);
            tag = NULL;
            value = NULL;
            objectSize = 1;
            head = cur;
            break;
        case TypePop:
            assert(pushed != NULL);
            tmp = constructLeft(ast, head, cur, spos, epos, objectSize, tag, value);
            NODE_GC_RETAIN(tmp);
            pushed->e.ref = tmp;
            pushed->pos = cur->pos;
            SetTag(pushed, TypeLink);
            return tmp;
        case TypePush:
            ast_create_node(ast, cur + 1, cur);
            assert(GetTag(cur) == TypeLink);
            /* fallthrough */
        case TypeLink:
            index = (long)cur->pos;
            if(index == -1) {
                cur->pos = (char *)objectSize;
                objectSize++;
            }
            else if(!(index < objectSize)) {
                objectSize = index + 1;
            }
            break;
        }
    }
    return constructLeft(ast, head, tail, spos, epos, objectSize, tag, value);
}

void ast_commit_tx(AstMachine *ast, int index, long tx)
{
    AstLog *cur;
    assert(ARRAY_size(ast->logs) > tx);
    cur = ARRAY_get(AstLog, &ast->logs, tx);
#ifdef DEBUG2
    fprintf(stderr, "0: %ld %d\n", tx, ARRAY_size(ast->logs));
    AstMachine_dumpLog(ast);
#endif
    Node node = ast_create_node(ast, cur, NULL);
    ast_rollback_tx(ast, tx);
#ifdef DEBUG2
    fprintf(stderr, "R: %ld %d\n", tx, ARRAY_size(ast->logs));
#endif
    if (node) {
        ast_log_link(ast, index, node);
    }
#ifdef DEBUG2
    fprintf(stderr, "1: %ld %d\n", tx, ARRAY_size(ast->logs));
    AstMachine_dumpLog(ast);
#endif
}

Node ast_get_parsed_node(AstMachine *ast)
{
    AstLog *cur, *tail;
    Node parsed = NULL;
    if (ast->parsed) {
        return ast->parsed;
    }
    cur = ARRAY_BEGIN(ast->logs);
    tail = ARRAY_last(ast->logs);
    for (; cur <= tail; ++cur) {
        if (GetTag(cur) == TypeNew) {
            parsed = ast_create_node(ast, cur, NULL);
            break;
        }
    }
    ast_rollback_tx(ast, 0);
    ast->parsed = parsed;
    return parsed;
}

#ifdef __cplusplus
}
#endif
