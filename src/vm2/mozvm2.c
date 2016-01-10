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

#define FAIL_IMPL() do { \
    long saved_, ast_tx_; \
    moz_inst_t *jump_; \
    mozpos_t pos_; \
    MOZVM_PROFILE_INC(FAIL_COUNT); \
    POP_FRAME(pos_, jump_, ast_tx_, saved_); \
    if (pos_ < GET_POS()) { \
        HEAD = (HEAD < GET_POS()) ? GET_POS() : HEAD; \
        SET_POS(pos_); \
    } \
    ast_rollback_tx(AST_MACHINE_GET(), ast_tx_); \
    symtable_rollback(SYMTABLE_GET(), saved_); \
    PC = jump_; \
} while (0)
#if 1
#define FAIL() do {\
    FAIL_IMPL(); \
    NEXT(); \
} while (0)
#else
#define FAIL() /*fprintf(stderr, "goto fail\n");*/goto L_fail;
#endif

#define PUSH(X) *SP++ = (long)(X)
#define POP()  *--SP

#define ABORT(MSG) fprintf(stderr, "%s\n", MSG);__asm volatile("int3")

static const moz_inst_t bytecode_base[] = {
    IExit, 0, // success
    IExit, 1  // error
};

//moz_inst_t *moz_runtime_parse_init(moz_runtime_t *runtime, const char *str, moz_inst_t *PC)
//{
//    long *SP = runtime->stack;
//    long *FP = SP;
//#define GET_POS()     CURRENT
//    const char *CURRENT = str;
//
//#ifdef MOZVM_DEFINE_LOCAL_VAR
//    AstMachine *AST = runtime->ast;
//    symtable_t *TBL = runtime->table;
//    memo_t *MEMO    = runtime->memo;
//#else
//#define AST  runtime->ast
//#define TBL  runtime->table
//#define MEMO runtime->memo
//#endif
//
//    PUSH_FRAME(GET_POS(),
//#ifdef MOZVM_USE_DIRECT_THREADING
//            bytecode_base + sizeof(void *) + 1,
//#else
//            bytecode_base + 2,
//#endif
//            ast_save_tx(AST), symtable_savepoint(TBL));
//    PUSH(bytecode_base);
//    runtime->stack = SP;
//    runtime->fp    = FP;
//    return PC;
//}
//
// long moz_runtime_parse(moz_runtime_t *runtime, const char *str, const moz_inst_t *PC)
// {
// #ifdef MOZVM_PROFILE_INST
//    const moz_inst_t *BEGIN = PC;
// #define PROFILE_INST(PC) if (*(PC) != Exit) { runtime->C.profile[(PC) - BEGIN]++; }
// #else
// #define PROFILE_INST(PC)
// #endif
//
//    long *SP = runtime->stack;
//    long *FP = runtime->fp;
//
//    const char *CURRENT = str;
// #define SET_POS(P)    CURRENT = (P)
// #define GET_CURRENT() (CURRENT)
// #define CONSUME()     CURRENT++
// #define CONSUME_N(N)  CURRENT += N
//
// #define SYMTABLE_GET() (TBL)
// #define AST_MACHINE_GET() (AST)
// #define MEMO_GET() (MEMO)
// #define HEAD (runtime)->head
// #define EOS() (GET_CURRENT() == runtime->tail)
//
// #ifdef MOZVM_DEFINE_LOCAL_VAR
//    AstMachine *AST = runtime->ast;
//    symtable_t *TBL = runtime->table;
//    memo_t *MEMO    = runtime->memo;
// #endif
//
// #ifdef MOZVM_USE_SWITCH_CASE_DISPATCH
// #define DISPATCH()         goto L_vm_head
// #define DISPATCH_START(PC) L_vm_head:;switch (*PC++) {
// #define DISPATCH_END()     default: ABORT(); }
// #define LABEL(OP)          case OP
// #else
// #define LABEL(OP)          MOZVM_##OP
//    static const void *__table[] = {
// #define DEFINE_TABLE(OP) &&LABEL(OP),
//        OP_EACH(DEFINE_TABLE)
// #undef DEFINE_TABLE
//    };
// #define DISPATCH_START(PC) DISPATCH()
// #define DISPATCH_END()     ABORT();
//
// #if defined(MOZVM_USE_INDIRECT_THREADING)
// #define DISPATCH() goto *__table[*PC++]
// #elif defined(MOZVM_USE_DIRECT_THREADING)
// #define DISPATCH() do { \
//    void **addr = *((void **)PC);\
//    PC += sizeof(void **);\
//    goto *addr;\
// } while (0)
//    if (PC == NULL) {
//        return (long) __table;
//    }
// #else
// #error please specify dispatch method
// #endif
// #endif
//
// #define NEXT() DISPATCH()
// #define JUMP(N) PC += N; DISPATCH()
//
// #define read_uint8_t(PC)   *(PC);              PC += sizeof(uint8_t)
// #define read_int8_t(PC)    *((int8_t *)PC);    PC += sizeof(int8_t)
// #define read_uint16_t(PC)  *((uint16_t *)PC);  PC += sizeof(uint16_t)
// #define read_mozaddr_t(PC) *((mozaddr_t *)PC); PC += sizeof(mozaddr_t)
// #define read_STRING_t(PC)  *((STRING_t *)PC);  PC += sizeof(STRING_t)
// #define read_BITSET_t(PC)  *((BITSET_t *)PC);  PC += sizeof(BITSET_t)
// #define read_TAG_t(PC)     *((TAG_t *)PC);     PC += sizeof(TAG_t)
// #define read_JMPTBL_t(PC)  *((JMPTBL_t *)PC);  PC += sizeof(JMPTBL_t)
//
// #define OP_CASE_(OP) LABEL(OP): PROFILE_INST(PC-1); MOZVM_PROFILE_INC(INST_COUNT);
// #ifdef PRINT_INST
// // #define OP_CASE(OP) LABEL(OP):; fprintf(stderr, "SP=%p FP=%p %ld %s\n", SP, FP, (long)(PC-1), #OP);
// #if PRINT_INST == 1
// #define OP_CASE(OP) OP_CASE_(OP); fprintf(stdout, "%-8s,\n", #OP);
// #elif PRINT_INST == 2
// #define OP_CASE(OP) OP_CASE_(OP); fprintf(stdout, "%p %-8s,\n", (PC-1), #OP);
// #else
//    uint8_t last = Exit;
// #define OP_CASE(OP) OP_CASE_(OP); fprintf(stdout, "%-8s->%-8s,\n", opcode2str(last), #OP); last = *(PC-1);
// #endif /* PRINT_INST == 2 */
// #else
// #define OP_CASE(OP) OP_CASE_(OP)
// #endif
//    DISPATCH_START(PC);
// #include "vm2_core.c"
//    DISPATCH_END();
//    assert(0 && "unreachable");
//    return 0;
// }

#ifdef __cplusplus
}
#endif
