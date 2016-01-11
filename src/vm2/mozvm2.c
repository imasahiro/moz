#include <stdio.h>
// #define PRINT_INST 1

#if defined(PRINT_INST) &&  PRINT_INST > 2
#define MOZVM_DUMP_OPCODE
#endif

#include "mozvm.h"
#include "compiler/ir.h"
#include "core/pstring.h"
#include "jmptbl.h"

#ifdef __cplusplus
extern "C" {
#endif

FOR_EACH_IR(MOZVM_PROFILE_DECL);

void moz_vm2_print_stats()
{
    FOR_EACH_IR(MOZVM_PROFILE_SHOW);
}

#define FP_FP     0
#define FP_POS    1
#define FP_NEXT   2
#define FP_FAIL   3
#define FP_MAX    (4)

#define PUSH_FRAME(POS, NEXT, FAIL) do {\
    SP[FP_FP]     = (long)FP; \
    SP[FP_POS]    = (long)POS; \
    SP[FP_NEXT]   = (long)(NEXT); \
    SP[FP_FAIL]   = (long)(FAIL); \
    FP = SP;\
    SP = SP + FP_MAX; \
} while (0)

#define DROP_FRAME() do {\
    SP     = FP; \
    FP     = (long *)FP[FP_FP]; \
} while (0)

#define POP_FRAME(POS, NEXT, FAIL) do {\
    SP   = FP; \
    NEXT = (moz_inst_t *)FP[FP_NEXT];\
    FAIL = (moz_inst_t *)FP[FP_FAIL];\
    POS  = (mozpos_t)FP[FP_POS];\
    DROP_FRAME(); \
} while (0)

#define FAIL_IMPL() do { \
    moz_inst_t *jump_; \
    mozpos_t pos_; \
    POP_FRAME(pos_, jump_, ast_tx_, saved_); \
    if (pos_ < CURRENT) { \
        SET_POS(pos_); \
    } \
    ast_rollback_tx(AST_MACHINE_GET(), ast_tx_); \
    symtable_rollback(SYMTABLE_GET(), saved_); \
    PC = jump_; \
} while (0)

#define FAIL(fail) do {\
    JUMP(fail); \
} while (0)

#define PUSH(X) *SP++ = (long)(X)
#define POP()  *--SP

#define ABORT(MSG) fprintf(stderr, "%s\n", MSG);__asm volatile("int3")

int moz_vm2_runtime_parse(moz_runtime_t *runtime,
        const moz_inst_t *PC,
        const char *head, const char *tail)
{
    static const moz_inst_t bytecode[] = {
        IExit, 0, // success
        IExit, 1  // error
    };
    long *SP = runtime->stack;
    long *FP = runtime->fp;

    const char *CURRENT = head;
    runtime->head = runtime->cur = head;
    runtime->tail = tail;
#define SET_POS(P)    CURRENT = (P)
#define CONSUME()     CONSUME_N(1)
#define CONSUME_N(N)  CURRENT += N

    PUSH_FRAME(CURRENT, &bytecode[0], &bytecode[2]);
#define SYMTABLE_GET() (TBL)
#define AST_MACHINE_GET() (AST)
#define MEMO_GET() (MEMO)
#define HEAD (runtime)->head
#define EOS() (CURRENT == runtime->tail)

    AstMachine *AST = runtime->ast;
    symtable_t *TBL = runtime->table;
    memo_t *MEMO    = runtime->memo;

#define LABEL(OP)          MOZVM_##OP
    static const void *__table[] = {
#define DEFINE_TABLE(OP) &&LABEL(OP),
        FOR_EACH_IR(DEFINE_TABLE)
#undef DEFINE_TABLE
    };
#define DISPATCH_START(PC) DISPATCH()
#define DISPATCH_END()     ABORT("unreachable");
#define DISPATCH() goto *__table[*PC++]
#define NEXT() DISPATCH()
#define JUMP(N) PC += N; DISPATCH()
#define OP_CASE(OP) LABEL(OP):

#define read_uint8_t(PC)   *(PC);              PC += sizeof(uint8_t)
#define read_int8_t(PC)    *((int8_t *)PC);    PC += sizeof(int8_t)
#define read_uint16_t(PC)  *((uint16_t *)PC);  PC += sizeof(uint16_t)
#define read_mozaddr_t(PC) *((mozaddr_t *)PC); PC += sizeof(mozaddr_t)
#define read_STRING_t(PC)  *((STRING_t *)PC);  PC += sizeof(STRING_t)
#define read_BITSET_t(PC)  *((BITSET_t *)PC);  PC += sizeof(BITSET_t)
#define read_TAG_t(PC)     *((TAG_t *)PC);     PC += sizeof(TAG_t)
#define read_JMPTBL_t(PC)  *((JMPTBL_t *)PC);  PC += sizeof(JMPTBL_t)
    DISPATCH_START(PC);
#include "vm2_core.c"
    DISPATCH_END();
    // assert(0 && "unreachable");
    return 0;
}

#ifdef __cplusplus
}
#endif
