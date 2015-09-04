#include "ast.h"
#include "karray.h"
#include "node.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

DEF_ARRAY_OP(AstLog);

static inline void SetTag(AstLog *log, enum AstLogType type)
{
#ifdef AST_LOG_UNBOX
    assert((log->e.val & TypeMask) == 0);
    log->e.val |= TypeMask & type;
#else
    log->type = type;
#endif
}

static inline enum AstLogType GetTag(AstLog *log)
{
#ifdef AST_LOG_UNBOX
    uintptr_t tag = log->e.val & TypeMask;
    assert(tag && "this log do not have tagged");
    return (enum AstLogType) tag;
#else
    return log->type;
#endif
}

static inline mozpos_t GetPos(AstLog *log)
{
#ifdef AST_LOG_UNBOX
    enum AstLogType tag = GetTag(log);
    assert(tag != TypePop);
    return ((tag & 1) == 0) ? log->i.pos : NULL;
#else
    return log->i.pos;
#endif

}

static inline Node *GetNode(AstLog *log)
{
#ifdef AST_LOG_UNBOX
    uintptr_t val = log->e.val & ~TypeMask;
    return (Node) val;
#else
    return log->e.ref;
#endif
}

AstMachine *AstMachine_init(unsigned log_size, const char *source)
{
    AstMachine *ast = (AstMachine *)VM_MALLOC(sizeof(*ast));
    ARRAY_init(AstLog, &ast->logs, log_size);
    ARRAY_ensureSize(AstLog, &ast->logs, log_size);
    ast->last_linked = NULL;
    ast->source = source;
    ast->parsed = NULL;
    return ast;
}

void AstMachine_dispose(AstMachine *ast)
{
    ast_rollback_tx(ast, 0);
    ARRAY_dispose(AstLog, &ast->logs);
    VM_FREE(ast);
    (void)ARRAY_AstLog_add;
}

#ifdef AST_DEBUG
static void AstMachine_dumpLog(AstMachine *ast)
{
    AstLog *cur  = ARRAY_BEGIN(ast->logs);
    AstLog *tail = ARRAY_last(ast->logs);
    unsigned i = 0;
    for (; cur <= tail; ++cur) {
        unsigned id = cur->id;
        switch(GetTag(cur)) {
        case TypeNew:
            fprintf(stderr, "[%d] %02d new(%ld)\n",
                    i, id, GetPos(cur)
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
                    - ast->source
#endif
                    );
            break;
        case TypeCapture:
            fprintf(stderr, "[%d] %02d cap(%ld)\n",
                    i, id, GetPos(cur)
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
                    - ast->source
#endif
                    );
            break;
        case TypeTag:
            fprintf(stderr, "[%d] %02d tag(%s)\n", i, id, cur->i.pos);
            break;
        case TypeReplace:
            fprintf(stderr, "[%d] %02d replace(%s)\n", i, id, cur->i.pos);
            break;
        case TypeLeftFold:
            fprintf(stderr, "[%d] %02d swap()\n", i, id);
            break;
        case TypePop:
            fprintf(stderr, "[%d] %02d pop(%ld)\n", i, id, (long)cur->i.pos);
            break;
        case TypePush:
            fprintf(stderr, "[%d] %02d push()\n", i, id);
            break;
        case TypeLink:
            fprintf(stderr, "[%d] %02d link(%d,%d)\n",
                    i, id, cur->i.idx, cur->shift[1]);
            break;
        }
        ++i;
    }
}
#endif

#ifdef AST_DEBUG
static unsigned last_id = 1;
#endif

static void ast_log(AstMachine *ast, enum AstLogType type, mozpos_t pos, uintptr_t val)
{
    AstLog *log;
    ARRAY_ensureSize(AstLog, &ast->logs, 1);
    log = ARRAY_END(ast->logs);
    SetTag(log, type);
    log->shift = 0;
    log->e.val = val;
    log->i.pos = pos;
#ifdef AST_DEBUG
    log->id = last_id;
    last_id++;
#endif
    ARRAY_size(ast->logs) += 1;
}

void ast_log_new(AstMachine *ast, mozpos_t pos)
{
    ast_log(ast, TypeNew, pos, 0);
}

void ast_log_capture(AstMachine *ast, mozpos_t pos)
{
    ast_log(ast, TypeCapture, pos, 0);
}

void ast_log_tag(AstMachine *ast, const char *tag)
{
    ast_log(ast, TypeTag, (mozpos_t)tag, 0);
}

void ast_log_replace(AstMachine *ast, const char *str)
{
    ast_log(ast, TypeReplace, (mozpos_t)str, 0);
}

void ast_log_swap(AstMachine *ast, mozpos_t pos)
{
    ast_log(ast, TypeLeftFold, pos, 0);
}

void ast_log_push(AstMachine *ast)
{
    ast_log(ast, TypePush, 0, 0);
}

void ast_log_pop(AstMachine *ast, int index)
{
    intptr_t val = (intptr_t) index;
    ast_log(ast, TypePop, (mozpos_t)val, 0);
}

void ast_log_link(AstMachine *ast, int index, Node *node)
{
    union ast_log_index i;
    i.idx = index;
    if (node) {
        NODE_GC_RETAIN(node);
    }
    ast_log(ast, TypeLink, i.pos, (uintptr_t)node);
    ast->last_linked = node;
}

void ast_rollback_tx(AstMachine *ast, long tx)
{
    unsigned len = ARRAY_size(ast->logs);
    if (tx < len) {
        AstLog *cur = ARRAY_n(ast->logs, tx);
        AstLog *tail = ARRAY_last(ast->logs);
        for (; cur <= tail; ++cur) {
            if(GetTag(cur) == TypeLink) {
                Node *o = GetNode(cur);
                if (o) {
                    NODE_GC_RELEASE(o);
                    cur->e.ref = NULL;
                }
            }
        }
    }

    ARRAY_size(ast->logs) = tx;
}

Node *constructLeft(AstMachine *ast, AstLog *cur, AstLog *tail, mozpos_t spos, mozpos_t epos, long objectSize, const char *tag, const char *value)
{
    long len = epos - spos;
    Node *newnode = Node_new(tag,
#ifndef MOZVM_USE_POINTER_AS_POS_REGISTER
            ast->source +
#endif
            spos, len, objectSize, value);

    if(objectSize == 0) {
        return newnode;
    }
    for (; cur <= tail; ++cur) {
        if(GetTag(cur) == TypeLink) {
            int index = cur->i.idx;
            int shift = cur->shift;
            Node *child = GetNode(cur);
            if(child) {
                assert(index >= 0);
                Node_set(newnode, index, child);
            } else {
                fprintf(stderr, "@@ linking null child at %d\n", index);
            }
            cur += shift;
        }
    }
    return newnode;
}

static Node *ast_create_node(AstMachine *ast, AstLog *cur, AstLog *pushed)
{
    AstLog *head, *tail;
    mozpos_t spos, epos;
    Node *tmp;
    const char *tag = NULL;
    const char *value = NULL;
    long objectSize = 0;
    long index = 0;
    long shift = 0;

    head = cur;
    tail = ARRAY_last(ast->logs);
    spos = GetPos(cur); epos = spos;
#ifdef AST_DEBUG
    fprintf(stderr, "createNode.start id=%d\n", cur->id);
#endif
    for (; cur <= tail; ++cur) {
        switch(GetTag(cur)) {
        case TypeNew:
            spos = cur->i.pos;
            epos = spos;
            objectSize = 0;
            tag = NULL;
            value = NULL;
            head = cur;
            break;
        case TypeCapture:
            epos = cur->i.pos;
            break;
        case TypeTag:
            tag = (const char *)cur->i.pos;
            break;
        case TypeReplace:
            value = (const char *)cur->i.pos;
            break;
        case TypeLeftFold:
            tmp = constructLeft(ast, head, cur, spos, epos, objectSize, tag, value);
            NODE_GC_RETAIN(tmp);
            cur->e.ref = tmp;
            cur->i.pos = 0;
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
            pushed->i.idx = (int16_t)(long)cur->i.pos;
            pushed->shift = cur - pushed;
            SetTag(pushed, TypeLink);
            return tmp;
        case TypePush:
            tmp = ast_create_node(ast, cur + 1, cur);
            assert(GetTag(cur) == TypeLink);
            /* fallthrough */
        case TypeLink:
            index = cur->i.idx;
            shift = cur->shift;
            if(index == -1) {
                cur->i.idx = objectSize;
                objectSize++;
            }
            else if(index > objectSize) {
                objectSize = index + 1;
            }
            cur += shift;
            break;
        }
    }
    tmp = constructLeft(ast, head, tail, spos, epos, objectSize, tag, value);
    return tmp;
}

void ast_commit_tx(AstMachine *ast, int index, long tx)
{
    AstLog *cur;
    assert(ARRAY_size(ast->logs) > tx);
    cur = ARRAY_get(AstLog, &ast->logs, tx);
#ifdef AST_DEBUG
    fprintf(stderr, "0: %ld %d\n", tx, ARRAY_size(ast->logs)-1);
    AstMachine_dumpLog(ast);
#endif
    Node *node = ast_create_node(ast, cur, NULL);
    ast_rollback_tx(ast, tx);
    if (node) {
        ast_log_link(ast, index, node);
    }
}

Node *ast_get_parsed_node(AstMachine *ast)
{
    AstLog *cur, *tail;
    Node *parsed = NULL;
    if (ast->parsed) {
        return ast->parsed;
    }
#ifdef AST_DEBUG
    AstMachine_dumpLog(ast);
#endif
    if (ARRAY_size(ast->logs) == 0) {
        return NULL;
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
    NODE_GC_RETAIN(parsed);
    ast->parsed = parsed;
    return parsed;
}

#ifdef MOZVM_MEMORY_USE_MSGC
void ast_trace(void *p, NodeVisitor *visitor)
{
    AstMachine *ast = (AstMachine *) p;
    AstLog *cur = ARRAY_n(ast->logs, 0);
    AstLog *tail = ARRAY_last(ast->logs);
    for (; cur <= tail; ++cur) {
        if(GetTag(cur) == TypeLink) {
            Node *o = GetNode(cur);
            if (o) {
                visitor->fn_visit(visitor, o);
            }
        }
    }
}
#endif

#ifdef __cplusplus
}
#endif
