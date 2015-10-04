#include "ast.h"
#include "karray.h"
#include "node.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

DEF_ARRAY_OP(AstLog);

static inline void SetTag(AstLog *log, AstLogType type)
{
#ifdef AST_LOG_UNBOX
    assert((log->e.val & TypeMask) == 0);
    log->e.val |= TypeMask & type;
#else
    log->type = type;
#endif
}

static inline AstLogType GetTag(AstLog *log)
{
#ifdef AST_LOG_UNBOX
    uintptr_t tag = log->e.val & TypeMask;
    assert(tag && "this log do not have tagged");
    return (AstLogType) tag;
#else
    return log->type;
#endif
}

static inline mozpos_t GetPos(AstLog *log)
{
#ifdef AST_LOG_UNBOX
    AstLogType tag = GetTag(log);
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
            fprintf(stderr, "[%d] %02d pop(%s)\n", i, id, cur->i.tag);
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

static void ast_log(AstMachine *ast, AstLogType type, mozpos_t pos, uintptr_t val)
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

void ast_log_swap(AstMachine *ast, mozpos_t pos, uint16_t labelId)
{
    ast_log(ast, TypeLeftFold, pos, (uintptr_t)labelId);
}

void ast_log_push(AstMachine *ast)
{
    ast_log(ast, TypePush, 0, 0);
}

void ast_log_pop(AstMachine *ast, uint16_t labelId)
{
    ast_log(ast, TypePop, (mozpos_t)(uintptr_t)labelId, 0);
}

void ast_log_link(AstMachine *ast, uint16_t labelId, Node *node)
{
    union ast_log_index i;
    i.labelId = (uintptr_t)labelId;
    if (node) {
        NODE_GC_RETAIN(node);
    }
    ast_log(ast, TypeLink, i.pos, (uintptr_t)node);
    ast->last_linked = node;
}

void ast_rollback_tx(AstMachine *ast, long tx)
{
#ifdef MOZVM_MEMORY_USE_RCGC
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
#endif
    ARRAY_size(ast->logs) = tx;
}

Node *constructLeft(AstMachine *ast, AstLog *cur, AstLog *tail,
        mozpos_t spos, mozpos_t epos, long objSize,
        const char *tag, const char *value)
{
    long len = epos - spos;
    unsigned n = 0;
    Node *newnode = Node_new(tag,
#ifndef MOZVM_USE_POINTER_AS_POS_REGISTER
            ast->source +
#endif
            spos, len, objSize, value);

    if(objSize == 0) {
        return newnode;
    }
    for (; cur <= tail; ++cur) {
        if(GetTag(cur) == TypeLink) {
            unsigned labelId = cur->i.labelId;
            int shift = cur->shift;
            Node *child = GetNode(cur);
            if(child) {
                assert(n >= 0);
                Node_set(newnode, n, labelId, child);
            } else {
#ifdef AST_DEBUG
                fprintf(stderr, "@@ linking null child at %u(%u)\n", n, labelId);
#endif
                assert(child != NULL);
            }
            n++;
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
    long objSize = 0;
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
            objSize = 0;
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
            tmp = constructLeft(ast, head, cur, spos, epos, objSize, tag, value);
            NODE_GC_RETAIN(tmp);
            tag = (const char *)cur->e.val;
            cur->e.ref = tmp;
            spos = cur->i.pos;
            cur->i.pos = tag;
            SetTag(cur, TypeLink);
            tag = NULL;
            value = NULL;
            objSize = 1;
            head = cur;
            break;
        case TypePop:
            assert(pushed != NULL);
            tmp = constructLeft(ast, head, cur, spos, epos, objSize, tag, value);
            NODE_GC_RETAIN(tmp);
            pushed->e.ref = tmp;
            pushed->i.tag = cur->i.tag;
            pushed->shift = cur - pushed;
            SetTag(pushed, TypeLink);
            return tmp;
        case TypePush:
            tmp = ast_create_node(ast, cur + 1, cur);
            assert(GetTag(cur) == TypeLink);
            /* fallthrough */
        case TypeLink:
            shift = cur->shift;
            objSize++;
            cur += shift;
            break;
        }
    }
    tmp = constructLeft(ast, head, tail, spos, epos, objSize, tag, value);
    return tmp;
}

void ast_commit_tx(AstMachine *ast, uint16_t labelId, long tx)
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
        ast_log_link(ast, labelId, node);
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
