#include "ast.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AstMachine {
};

long ast_save_tx(AstMachine *ast)
{
    return 0;
}

void ast_rollback_tx(AstMachine *ast, long tx)
{
}

void ast_commit_tx(AstMachine *ast, uint8_t index, long tx)
{
}

void ast_log_replace(AstMachine *ast, char *str)
{
}

void ast_log_capture(AstMachine *ast, char *cur)
{
}

void ast_log_new(AstMachine *ast, char *cur)
{
}

void ast_log_pop(AstMachine *ast, uint8_t index)
{
}

void ast_log_push(AstMachine *ast)
{
}

void ast_log_swap(AstMachine *ast, char *cur)
{
}

void ast_log_tag(AstMachine *ast, char *tag)
{
}

void ast_log_link(AstMachine *ast, uint8_t index, void *result)
{
}

void *ast_get_last_linked_node(AstMachine *ast)
{
    return NULL;
}

#ifdef DEBUG
int main(int argc, char const* argv[])
{
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif
