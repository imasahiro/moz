#include <stdio.h>
#include "instruction.h"
#include "mozvm_config.h"
#include "mozvm.h"
#include "pstring.h"
#include "ast.h"
#include "memo.h"
#include "token.h"
#include "symtable.h"

typedef struct moz_runtime_t {
    AstMachine *ast;
    char *head;
    char *input;
    size_t input_size;
    long *stack;

    bitset_t *sets;
    const char **tags;
    const char **strs;
    int **tables;
} moz_runtime_t;

#ifdef MOZVM_SMALL_STRING_INST
typedef uint16_t STRING_t
#define STRING_GET_IMPL(ID) runtime->strs[(ID)]
#else
typedef char *STRING_t;
#define STRING_GET_IMPL(ID) (ID)
#endif

typedef char tag_t;
#ifdef MOZVM_SMALL_TAG_INST
typedef uint16_t TAG_t
#define TAG_GET_IMPL(ID) runtime->tags[(ID)]
#else
typedef tag_t *TAG_t;
#define TAG_GET_IMPL(ID) (ID)
#endif

#ifdef MOZVM_SMALL_BITSET_INST
typedef uint16_t BITSET_t
#define BITSET_GET_IMPL(ID) runtime->sets[(ID)]
#else
typedef bitset_t *BITSET_t;
#define BITSET_GET_IMPL(ID) (ID)
#endif

#ifdef MOZVM_SMALL_JMPTBL_INST
typedef uint16_t JMPTBL_t
#define JMPTBL_GET_IMPL(ID) runtime->tables[(ID)]
#else
typedef int *JMPTBL_t;
#define JMPTBL_GET_IMPL(ID) (ID)
#endif

#define CONSUME() ++CURRENT;
#define CONSUME_N(N) CURRENT += N;

#define FAIL() do {\
    long saved_, ast_tx_, jump_; \
    char *pos_; \
    POP_FRAME(pos_, jump_, ast_tx_, saved_); \
    if (pos_ < CURRENT) { \
        HEAD = (HEAD < CURRENT) ? CURRENT : HEAD; \
        CURRENT = pos_; \
    } \
    ast_rollback_tx(AST_MACHINE_GET(), ast_tx_); \
    symtable_rollback(saved_); \
    JUMP(jump_); \
} while (0)

typedef uint8_t moz_inst_t;
int parse(moz_runtime_t *runtime, char *input, moz_inst_t *inst)
{
    register char *CURRENT = input;
    register moz_inst_t *PC = inst;
    register long *SP = runtime->stack;
    char *tail = input + runtime->input_size;
    AstMachine *AST = runtime->ast;
#define PUSH(X) *SP++ = (long)(X)
#define POP()  --*SP
#define PEEK() PEEK_N(1)
#define PEEK_N(N) ((SP)+N)
#define ABORT() __asm__ __volatile("int3")
#define TODO() __asm__ __volatile("int3")
#define PUSH_FRAME(POS, NEXT, AST, SYMTBL) do {\
    PUSH(POS);\
    PUSH(NEXT);\
    PUSH(AST);\
    PUSH(SYMTBL);\
} while (0)
#define POP_FRAME(POS, NEXT, AST, SYMTBL) do {\
    SYMTBL = POP();\
    AST    = POP();\
    NEXT   = POP();\
    POS    = (char *)POP();\
} while (0)

#define AST_MACHINE_GET() (AST)
#define HEAD (runtime)->head
#define EOS() (CURRENT == tail)
#define DISPATCH() goto L_vm_head
#define NEXT() ++PC; DISPATCH()
#define JUMP(N) PC += N; DISPATCH()
#define DISPATCH_START(PC) L_vm_head:;switch (*PC) {
#define DISPATCH_END() default: ABORT(); }
#define OP_CASE(OP) case OP:
    DISPATCH_START(PC);
#include "vm_core.c"
    DISPATCH_END();
    assert(0 && "unreachable");
    return 0;
}
