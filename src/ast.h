#include <stdint.h>
#include "node.h"
#ifndef AST_H
#define AST_H

struct AstMachine;
typedef struct AstMachine AstMachine;

AstMachine *AstMachine_init(unsigned log_size, char *source);
void AstMachine_dispose(AstMachine *ast);
void AstMachine_setSource(AstMachine *ast, char *source);

long ast_save_tx(AstMachine *ast);
void ast_rollback_tx(AstMachine *ast, long tx);
void ast_commit_tx(AstMachine *ast, int index, long tx);
void ast_log_replace(AstMachine *ast, char *str);
void ast_log_capture(AstMachine *ast, char *cur);
void ast_log_new(AstMachine *ast, char *cur);
void ast_log_pop(AstMachine *ast, int index);
void ast_log_push(AstMachine *ast);
void ast_log_swap(AstMachine *ast, char *cur);
void ast_log_tag(AstMachine *ast, char *tag);
void ast_log_link(AstMachine *ast, int index, Node result);
Node ast_get_last_linked_node(AstMachine *ast);

#endif /* end of include guard */
