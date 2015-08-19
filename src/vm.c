#include <stdio.h>
#include "instruction.h"
#include "mozvm_config.h"
#include "mozvm.h"
#include "pstring.h"
#include "ast.h"
#include "memo.h"
#include "token.h"
#include "symtable.h"

#ifdef __cplusplus
extern "C" {
#endif

moz_runtime_t *moz_runtime_init(unsigned jmptbl_size, unsigned memo_size)
{
    moz_runtime_t *r;
    unsigned size = sizeof(*r) + sizeof(long) * (MOZ_DEFAULT_STACK_SIZE - 1);

    r = (moz_runtime_t *)malloc(size);
    r->ast = AstMachine_init(MOZ_AST_MACHINE_DEFAULT_LOG_SIZE, NULL);
    r->table = symtable_init();
    r->memo = memo_init(MOZ_MEMO_DEFAULT_WINDOW_SIZE, memo_size, MEMO_TYPE_ELASTIC);
    r->head = r->input = r->tail = NULL;
    if (jmptbl_size) {
        r->jumps = (int *)malloc(sizeof(int) * MOZ_JMPTABLE_SIZE * jmptbl_size);
    }
    r->stack = r->stack_;
    return r;
}

void moz_runtime_dispose(moz_runtime_t *r)
{
    AstMachine_dispose(r->ast);
    symtable_dispose(r->table);
    memo_dispose(r->memo);
    free(r);
}

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
    symtable_rollback(SYMTABLE_GET(), saved_); \
    JUMP(jump_); \
} while (0)

int parse(moz_runtime_t *runtime, char *start, char *end, moz_inst_t *inst)
{
    register char *CURRENT = start;
    register moz_inst_t *PC = inst;
    register long *SP = runtime->stack;
    AstMachine *AST = runtime->ast;
    symtable_t *TBL = runtime->table;
    memo_t *memo = runtime->memo;
    runtime->head = CURRENT;
    runtime->tail = end;
    AstMachine_setSource(AST, CURRENT);

#define read_uint8_t(PC)  *(PC);             PC += sizeof(uint8_t)
#define read_int8_t(PC)   *((int8_t *)PC);   PC += sizeof(int8_t)
#define read_int(PC)      *((int *)PC);      PC += sizeof(int)
#define read_uint32_t(PC) *((uint32_t *)PC); PC += sizeof(uint32_t)
#define read_STRING_t(PC) *((STRING_t *)PC); PC += sizeof(STRING_t)
#define read_BITSET_t(PC) *((BITSET_t *)PC); PC += sizeof(BITSET_t)
#define read_TAG_t(PC)    *((TAG_t *)PC);    PC += sizeof(TAG_t)
#define read_JMPTBL_t(PC) *((JMPTBL_t *)PC); PC += sizeof(JMPTBL_t)
// #define read_int(PC) ((int *)PC); PC += sizeof(int)
// #define read_int(PC) ((int *)PC); PC += sizeof(int)

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

#define SYMTABLE_GET() (TBL)
#define AST_MACHINE_GET() (AST)
#define MEMO_GET() (memo)
#define HEAD (runtime)->head
#define EOS() (CURRENT == runtime->tail)
#define DISPATCH() goto L_vm_head
#define NEXT() ++PC; DISPATCH()
#define JUMP(N) PC += N; DISPATCH()
#define DISPATCH_START(PC) L_vm_head:;switch (*PC++) {
#define DISPATCH_END() default: ABORT(); }
#define OP_CASE(OP) case OP:
    DISPATCH_START(PC);
#include "vm_core.c"
    DISPATCH_END();
    assert(0 && "unreachable");
    return 0;
}

#ifdef __cplusplus
}
#endif
