#include "ast.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AstMachine {
};

AstMachine *AstMachine_init(unsigned log_size)
{
    assert(0 && "not implemented");
    return NULL;
}

void AstMachine_dispose(AstMachine *ast)
{
    assert(0 && "not implemented");
}

long ast_save_tx(AstMachine *ast)
{
    assert(0 && "not implemented");
    return 0;
}

void ast_rollback_tx(AstMachine *ast, long tx)
{
    assert(0 && "not implemented");
}

void ast_commit_tx(AstMachine *ast, uint8_t index, long tx)
{
    assert(0 && "not implemented");
}

void ast_log_replace(AstMachine *ast, char *str)
{
    assert(0 && "not implemented");
}

void ast_log_capture(AstMachine *ast, char *cur)
{
    assert(0 && "not implemented");
}

void ast_log_new(AstMachine *ast, char *cur)
{
    assert(0 && "not implemented");
}

void ast_log_pop(AstMachine *ast, uint8_t index)
{
    assert(0 && "not implemented");
}

void ast_log_push(AstMachine *ast)
{
    assert(0 && "not implemented");
}

void ast_log_swap(AstMachine *ast, char *cur)
{
    assert(0 && "not implemented");
}

void ast_log_tag(AstMachine *ast, char *tag)
{
    assert(0 && "not implemented");
}

void ast_log_link(AstMachine *ast, uint8_t index, void *result)
{
    assert(0 && "not implemented");
}

void *ast_get_last_linked_node(AstMachine *ast)
{
    assert(0 && "not implemented");
    return NULL;
}

#ifdef DEBUG
int main(int argc, char const* argv[])
{
    AstMachine *ast = AstMachine_init(128);
    AstMachine_dispose(ast);
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif
