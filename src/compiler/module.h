#include "libnez/ast.h"
#include "libnez/symtable.h"
#include "libnez/memo.h"

#ifndef MOZ_MODULE_H
#define MOZ_MODULE_H

typedef struct moz_parser_runtime_t {
    AstMachine *ast;
    symtable_t *table;
    memo_t *memo;
    const char *input_pos;
    const char *input_begin;
    const char *input_end;
    long *stack;
    long *frame;
} moz_parser_runtime_t;

typedef struct moz_module_t moz_module_t;

struct moz_module_t {
    int (*parse)(moz_module_t *, moz_parser_runtime_t *);
    int (*dispose)(moz_module_t *);
};

struct moz_compiler_t;
moz_module_t *moz_vm2_module_compile(struct moz_compiler_t *C);

#endif /* end of include guard */
