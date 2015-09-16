#include <stdint.h>
#include "mozvm_config.h"
#include "node.h"
#ifndef AST_H
#define AST_H

#ifdef __cplusplus
extern "C" {
#endif

// #define AST_LOG_UNBOX
typedef enum AstLogType {
    // TypeNode     = 0,
    TypeTag      = 1,
    TypePop      = 2,
    TypeReplace  = 3,
    TypePush     = 4,
    TypeLeftFold = 5,
    TypeNew      = 6,
    TypeLink     = 7,
    TypeCapture  = 8,
} AstLogType;

// #define AST_DEBUG 1

union ast_log_index {
    const char *tag;
    uintptr_t labelId;
    mozpos_t pos;
};

typedef struct AstLog {
#ifdef AST_DEBUG
    unsigned id;
#endif
#ifdef AST_LOG_UNBOX
#define TypeMask (0xfUL)
#else
    AstLogType type;
#endif
    int shift;
    union ast_log_entry {
        uintptr_t val;
        Node *ref;
    } e;
    union ast_log_index i;
    const char *label;
} AstLog;

DEF_ARRAY_STRUCT0(AstLog, unsigned);
DEF_ARRAY_T(AstLog);

struct AstMachine {
    ARRAY(AstLog) logs;
    Node *last_linked;
    Node *parsed;
    const char *source;
};

typedef struct AstMachine AstMachine;

AstMachine *AstMachine_init(unsigned log_size, const char *source);
void AstMachine_dispose(AstMachine *ast);
static inline void AstMachine_setSource(AstMachine *ast, const char *source)
{
    ast->source = source;
}

static inline long ast_save_tx(AstMachine *ast)
{
    return ARRAY_size(ast->logs);
}

void ast_rollback_tx(AstMachine *ast, long tx);
void ast_commit_tx(AstMachine *ast, uint16_t labelId, long tx);
void ast_log_replace(AstMachine *ast, const char *str);
void ast_log_capture(AstMachine *ast, mozpos_t pos);
void ast_log_new(AstMachine *ast, mozpos_t pos);
void ast_log_pop(AstMachine *ast, uint16_t labelId);
void ast_log_push(AstMachine *ast);
void ast_log_swap(AstMachine *ast, mozpos_t pos, uint16_t labelId);
void ast_log_tag(AstMachine *ast, const char *tag);
void ast_log_link(AstMachine *ast, uint16_t labelId, Node *result);

static inline Node *ast_get_last_linked_node(AstMachine *ast)
{
    return ast->last_linked;
}

Node *ast_get_parsed_node(AstMachine *ast);
#ifdef MOZVM_MEMORY_USE_MSGC
void ast_trace(void *p, NodeVisitor *visitor);
#endif

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
