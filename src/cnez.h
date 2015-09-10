#ifndef CNEZ_H
#define CNEZ_H

#include "libnez.h"

typedef struct ParsingContext {
    char *cur;
    Node *left;
    AstMachine *ast;
    symtable_t *table;
    memo_t *memo;
    int *flags;
    size_t flags_size;
    size_t input_size;
    char *input;
} *ParsingContext;

#endif /* end of include guard */
