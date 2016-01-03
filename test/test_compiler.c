#include "compiler/compiler.h"
#include "compiler/expression.h"
#include <stdio.h>
#include <assert.h>

void test_compiler_init_dispose()
{
    moz_compiler_t C;
    moz_compiler_init(&C, NULL);
    moz_compiler_dispose(&C);
}

void test_factory()
{
    moz_compiler_t C;
    moz_compiler_init(&C, NULL);
    moz_expr_factory_t *factory = moz_compiler_get_factory();
    expr_t *expr = factory->_Empty(&C);
    assert(factory != NULL);
    assert(expr->type == Empty);
    moz_compiler_dispose(&C);
}

void test_decl_new()
{
    moz_compiler_t C;
    moz_compiler_init(&C, NULL);
    decl_t *decl = moz_decl_new(&C, "hello", 5);
    assert(decl->name.len == 5 && strncmp(decl->name.str, "hello", 5) == 0);
    assert(ARRAY_size(C.decls) == 1);
    moz_compiler_dispose(&C);
}

int main(int argc, char const* argv[])
{
    test_compiler_init_dispose();
    test_factory();
    test_decl_new();
    return 0;
}
