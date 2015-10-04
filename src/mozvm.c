#include <stdio.h>
// #define PRINT_INST 1

#if defined(PRINT_INST) &&  PRINT_INST > 2
#define MOZVM_DUMP_OPCODE
#endif

#include "mozvm.h"
#include "instruction.h"
#include "pstring.h"

#ifdef MOZVM_USE_JMPTBL
#include "jmptbl.h"
#endif

#ifdef MOZVM_ENABLE_JIT
#include "jit.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


#if 0
#define MEMO_DEBUG_FAIL_HIT(ID)   fprintf(stdout, "%d fail_hit\n", ID)
#define MEMO_DEBUG_HIT(ID, N)     fprintf(stdout, "%d hit %d\n", ID, N)
#define MEMO_DEBUG_MISS(ID)       fprintf(stdout, "%d miss\n", ID)
#define MEMO_DEBUG_MEMO(ID)       fprintf(stdout, "%d memo\n", ID)
#define MEMO_DEBUG_MEMOFAIL(ID)   fprintf(stdout, "%d memofail\n", ID)
#define MEMO_DEBUG_T_FAIL_HIT(ID) fprintf(stdout, "%d tlookup fail_hit\n", ID)
#define MEMO_DEBUG_T_HIT(ID, N)   fprintf(stdout, "%d tlookup hit %d\n", ID, N)
#define MEMO_DEBUG_T_MISS(ID)     fprintf(stdout, "%d tlookup miss\n", ID)
#define MEMO_DEBUG_T_MEMO(ID)     fprintf(stdout, "%d tmemo\n", ID)
#else
#define MEMO_DEBUG_FAIL_HIT(ID)
#define MEMO_DEBUG_HIT(ID, N)
#define MEMO_DEBUG_MISS(ID)
#define MEMO_DEBUG_MEMO(ID)
#define MEMO_DEBUG_MEMOFAIL(ID)
#define MEMO_DEBUG_T_FAIL_HIT(ID)
#define MEMO_DEBUG_T_HIT(ID, N)
#define MEMO_DEBUG_T_MISS(ID)
#define MEMO_DEBUG_T_MEMO(ID)

#endif

#define MOZVM_VM_PROFILE_EACH(F) \
    F(INST_COUNT) \
    F(FAIL_COUNT) \
    F(ALT_COUNT)

MOZVM_VM_PROFILE_EACH(MOZVM_PROFILE_DECL);

#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
#define MOZVM_VM_MEMO_PROFILE_EACH(F) \
    F(MEMO_DISABLE_COUNT) \
    F(MEMO_TOTAL_COUNT)

MOZVM_VM_MEMO_PROFILE_EACH(MOZVM_PROFILE_DECL);
#endif

moz_runtime_t *moz_runtime_init(unsigned memo, unsigned prod_size)
{
    moz_runtime_t *r;
    unsigned size = sizeof(*r) + sizeof(long) * (MOZ_DEFAULT_STACK_SIZE - 1);
    r = (moz_runtime_t *)VM_CALLOC(1, size);
    r->ast = AstMachine_init(MOZ_AST_MACHINE_DEFAULT_LOG_SIZE, NULL);
    r->table = symtable_init();
    r->memo = memo_init(MOZ_MEMO_DEFAULT_WINDOW_SIZE, memo);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
    r->memo_points = (MemoPoint *)VM_CALLOC(1, sizeof(MemoPoint) * memo);
#endif
    r->head = 0;
    r->input = r->tail = NULL;
    memset(&r->stack_[0], 0xaa, sizeof(long) * MOZ_DEFAULT_STACK_SIZE);
    memset(&r->stack_[MOZ_DEFAULT_STACK_SIZE - 0xf], 0xbb, sizeof(long) * 0xf);
    r->stack = &r->stack_[0] + 0xf;
    r->fp = r->stack;

    r->C.memo_size = memo;
    size = sizeof(moz_production_t) * (prod_size + 1);
    r->prods = (moz_production_t *) VM_CALLOC(1, size);
#ifdef MOZVM_ENABLE_JIT
    r->cur = 0;
    mozvm_jit_init(r);
#endif
#ifdef MOZVM_MEMORY_USE_MSGC
    NodeManager_add_gc_root(r->ast, ast_trace);
    NodeManager_add_gc_root(r->memo, memo_trace);
#endif
    return r;
}

void moz_runtime_reset1(moz_runtime_t *r)
{
    unsigned memo = r->C.memo_size;
    AstMachine_dispose(r->ast);
    symtable_dispose(r->table);
    memo_dispose(r->memo);
#ifdef MOZVM_ENABLE_JIT
    mozvm_jit_reset(r);
#endif

    r->ast = AstMachine_init(MOZ_AST_MACHINE_DEFAULT_LOG_SIZE, NULL);
    r->table = symtable_init();
    r->memo = memo_init(MOZ_MEMO_DEFAULT_WINDOW_SIZE, memo);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
    memset(r->memo_points, 0, sizeof(MemoPoint) * memo);
#endif
    r->stack = &r->stack_[0] + 0xf;
    r->fp = r->stack;
}
void moz_runtime_reset2(moz_runtime_t *r)
{
#ifdef MOZVM_MEMORY_USE_MSGC
    NodeManager_add_gc_root(r->ast, ast_trace);
    NodeManager_add_gc_root(r->memo, memo_trace);
#endif
}

void moz_runtime_print_stats(moz_runtime_t *r)
{
    MOZVM_VM_PROFILE_EACH(MOZVM_PROFILE_SHOW);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
    MOZVM_VM_MEMO_PROFILE_EACH(MOZVM_PROFILE_SHOW);
#endif
}

void moz_runtime_dispose(moz_runtime_t *r)
{
    unsigned i;
    AstMachine_dispose(r->ast);
    symtable_dispose(r->table);
    memo_dispose(r->memo);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
    VM_FREE(r->memo_points);
#endif
    if (r->C.jumps) {
        VM_FREE(r->C.jumps);
    }
#ifdef MOZVM_USE_JMPTBL
    if (r->C.jumps1) {
        VM_FREE(r->C.jumps1);
    }
    if (r->C.jumps2) {
        VM_FREE(r->C.jumps2);
    }
    if (r->C.jumps3) {
        VM_FREE(r->C.jumps3);
    }
#endif
#ifdef MOZVM_PROFILE_INST
    if (r->C.profile) {
        VM_FREE(r->C.profile);
    }
#endif

    VM_FREE(r->prods);
#ifdef MOZVM_ENABLE_JIT
    mozvm_jit_dispose(r);
#endif
    if (r->C.set_size) {
        VM_FREE(r->C.sets);
    }
    if (r->C.table_size) {
        for (i = 0; i < r->C.table_size; i++) {
            pstring_delete((const char *)r->C.tables[i]);
        }
        VM_FREE(r->C.tables);
    }
    if (r->C.tag_size) {
        for (i = 0; i < r->C.tag_size; i++) {
            pstring_delete((const char *)r->C.tags[i]);
        }
        VM_FREE(r->C.tags);
    }
    if (r->C.str_size) {
        for (i = 0; i < r->C.str_size; i++) {
            pstring_delete((const char *)r->C.strs[i]);
        }
        VM_FREE(r->C.strs);
    }
    if (r->C.prod_size) {
        for (i = 0; i < r->C.prod_size; i++) {
            pstring_delete((const char *)r->C.prods[i]);
        }
        VM_FREE(r->C.prods);
    }
    VM_FREE(r);
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
    /* fprintf(stderr, "%-8s fail  SP=%p FP=%p jump=%p\n", */ \
    /*         runtime->prods[prod_id], SP, FP, jump_);  */ \
    PC = jump_; \
} while (0)
#if 0
#define FAIL() do {\
    FAIL_IMPL(); \
    NEXT(); \
} while (0)
#else
#define FAIL() /*fprintf(stderr, "goto fail\n");*/goto L_fail;
#endif

#define PUSH(X) *SP++ = (long)(X)
#define POP()  *--SP

#define ABORT() __asm volatile("int3")

#define FP_FP     0
#define FP_POS    1
#define FP_NEXT   2
#define FP_AST    3
#define FP_SYMTBL 4
#define FP_MAX    (5)

#define PUSH_FRAME(POS, NEXT, AST, SYMTBL) do {\
    SP[FP_FP]     = (long)FP; \
    SP[FP_POS]    = (long)POS; \
    SP[FP_NEXT]   = (long)NEXT; \
    SP[FP_AST]    = AST; \
    SP[FP_SYMTBL] = SYMTBL; \
    FP = SP;\
    SP = SP + FP_MAX; \
} while (0)

#define DROP_FRAME() do {\
    SP     = FP; \
    FP     = (long *)FP[FP_FP]; \
} while (0)

#define POP_FRAME(POS, NEXT, AST, SYMTBL) do {\
    SP     = FP; \
    SYMTBL = FP[FP_SYMTBL];\
    AST    = FP[FP_AST];\
    NEXT   = (moz_inst_t *)FP[FP_NEXT];\
    POS    = (mozpos_t)FP[FP_POS];\
    DROP_FRAME(); \
} while (0)

#define PEEK_FRAME(POS, NEXT, AST, SYMTBL) do {\
    SYMTBL = (FP+FP_SYMTBL);\
    AST    = (FP+FP_AST);\
    NEXT   = (moz_inst_t **)(FP+FP_NEXT);\
    POS    = (mozpos_t *)(FP+FP_POS);\
} while (0)

#ifdef MOZVM_USE_DIRECT_THREADING
#error not implemented
#else
static const moz_inst_t bytecode_base[] = {
    Exit, 0, // success
    Exit, 1  // error
};
#endif

moz_inst_t *moz_runtime_parse_init(moz_runtime_t *runtime, const char *str, moz_inst_t *PC)
{
    long *SP = runtime->stack;
    long *FP = SP;
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
#define GET_POS()     CURRENT
    const char *CURRENT = str;
#else
#define GET_POS()     __pos
    size_t __pos = 0;
#endif

#ifdef MOZVM_DEFINE_LOCAL_VAR
    AstMachine *AST = runtime->ast;
    symtable_t *TBL = runtime->table;
    memo_t *MEMO    = runtime->memo;
#else
#define AST  runtime->ast
#define TBL  runtime->table
#define MEMO runtime->memo
#endif

    PUSH_FRAME(GET_POS(),
#ifdef MOZVM_USE_DIRECT_THREADING
            bytecode_base + sizeof(void *) + 1,
#else
            bytecode_base + 2,
#endif
            ast_save_tx(AST), symtable_savepoint(TBL));
#ifdef MOZVM_DEBUG_PROD
    PUSH(0/*prod_id*/);
#endif
    PUSH(bytecode_base);
    runtime->stack = SP;
    runtime->fp    = FP;
    return PC;
}

long moz_runtime_parse(moz_runtime_t *runtime, const char *str, const moz_inst_t *PC)
{
#ifdef MOZVM_PROFILE_INST
    const moz_inst_t *BEGIN = PC;
#define PROFILE_INST(PC) if (*(PC) != Exit) { runtime->C.profile[(PC) - BEGIN]++; }
#else
#define PROFILE_INST(PC)
#endif

    long *SP = runtime->stack;
    long *FP = runtime->fp;
#ifdef MOZVM_DEBUG_PROD
    long prod_id = 0;
#endif

#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    const char *CURRENT = str;
#define SET_POS(P)    CURRENT = (P)
#define GET_CURRENT() (CURRENT)
#define CONSUME()     CURRENT++
#define CONSUME_N(N)  CURRENT += N
#else
    size_t __pos = 0;
#define SET_POS(P)    __pos = (P)
#define GET_CURRENT() (str + __pos)
#define CONSUME()    __pos++
#define CONSUME_N(N) __pos += N
#endif

#define SYMTABLE_GET() (TBL)
#define AST_MACHINE_GET() (AST)
#define MEMO_GET() (MEMO)
#define HEAD (runtime)->head
#define EOS() (GET_CURRENT() == runtime->tail)

#ifdef MOZVM_DEFINE_LOCAL_VAR
    AstMachine *AST = runtime->ast;
    symtable_t *TBL = runtime->table;
    memo_t *MEMO    = runtime->memo;
#endif

#ifdef MOZVM_USE_SWITCH_CASE_DISPATCH
#define DISPATCH()         goto L_vm_head
#define DISPATCH_START(PC) L_vm_head:;switch (*PC++) {
#define DISPATCH_END()     default: ABORT(); }
#define LABEL(OP)          case OP
#else
#define LABEL(OP)          MOZVM_##OP
    static const void *__table[] = {
#define DEFINE_TABLE(OP) &&LABEL(OP),
        OP_EACH(DEFINE_TABLE)
#undef DEFINE_TABLE
    };
#define DISPATCH_START(PC) DISPATCH()
#define DISPATCH_END()     ABORT();

#if defined(MOZVM_USE_INDIRECT_THREADING)
#define DISPATCH() goto *__table[*PC++]
#elif defined(MOZVM_USE_DIRECT_THREADING)
#define DISPATCH() do { \
    void **addr = *((void **)PC);\
    PC += sizeof(void **);\
    goto *addr;\
} while (0)
    if (PC == NULL) {
        return (long) __table;
    }
#else
#error please specify dispatch method
#endif
#endif

#define NEXT() DISPATCH()
#define JUMP(N) PC += N; DISPATCH()

#define read_uint8_t(PC)   *(PC);              PC += sizeof(uint8_t)
#define read_int8_t(PC)    *((int8_t *)PC);    PC += sizeof(int8_t)
#define read_uint16_t(PC)  *((uint16_t *)PC);  PC += sizeof(uint16_t)
#define read_mozaddr_t(PC) *((mozaddr_t *)PC); PC += sizeof(mozaddr_t)
#define read_STRING_t(PC)  *((STRING_t *)PC);  PC += sizeof(STRING_t)
#define read_BITSET_t(PC)  *((BITSET_t *)PC);  PC += sizeof(BITSET_t)
#define read_TAG_t(PC)     *((TAG_t *)PC);     PC += sizeof(TAG_t)
#define read_JMPTBL_t(PC)  *((JMPTBL_t *)PC);  PC += sizeof(JMPTBL_t)

#define OP_CASE_(OP) LABEL(OP): PROFILE_INST(PC-1); MOZVM_PROFILE_INC(INST_COUNT);
#ifdef PRINT_INST
#ifdef MOZVM_DEBUG_PROD
#define OP_CASE(OP) OP_CASE_(OP); fprintf(stdout, "%p %-8s \t%s\n", (PC-1), runtime->C.prods[prod_id], #OP);
#else
// #define OP_CASE(OP) LABEL(OP):; fprintf(stderr, "SP=%p FP=%p %ld %s\n", SP, FP, (long)(PC-1), #OP);
#if PRINT_INST == 1
#define OP_CASE(OP) OP_CASE_(OP); fprintf(stdout, "%-8s,\n", #OP);
#elif PRINT_INST == 2
#define OP_CASE(OP) OP_CASE_(OP); fprintf(stdout, "%p %-8s,\n", (PC-1), #OP);
#else
    uint8_t last = Exit;
#define OP_CASE(OP) OP_CASE_(OP); fprintf(stdout, "%-8s->%-8s,\n", opcode2str(last), #OP); last = *(PC-1);
#endif /* PRINT_INST == 2 */
#endif /* endif PRINT_INST */

#else
#define OP_CASE(OP) OP_CASE_(OP)
#endif
    DISPATCH_START(PC);
#include "vm_core.c"
    DISPATCH_END();
    assert(0 && "unreachable");
    return 0;
}

#ifdef __cplusplus
}
#endif
