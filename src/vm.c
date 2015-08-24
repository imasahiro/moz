#include <stdio.h>
// #define PRINT_INST 1

#if defined(PRINT_INST) &&  PRINT_INST >= 2
#define MOZVM_DUMP_OPCODE
#endif
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

// #define MOZVM_PROFILE 1

#ifdef MOZVM_PROFILE
#define MOZVM_PROFILE_DECL(X) uint64_t _PROFILE_##X = 0;
#define MOZVM_PROFILE_INC(X)  (_PROFILE_##X)++
#define MOZVM_PROFILE_SHOW(X) fprintf(stderr, "%-10s %llu\n", #X, _PROFILE_##X);
#define MOZVM_PROFILE_ENABLE(X)
#else
#define MOZVM_PROFILE_DECL(X)
#define MOZVM_PROFILE_INC(X)
#define MOZVM_PROFILE_SHOW(X)
#define MOZVM_PROFILE_ENABLE(X)
#endif
#define MOZVM_PROFILE_EACH(F) MOZVM_PROFILE_DEFINE(F)

#define MOZVM_PROFILE_DEFINE(F) \
    F(INST_COUNT) \
    F(FAIL_COUNT) \
    F(ALT_COUNT)

MOZVM_PROFILE_EACH(MOZVM_PROFILE_DECL);

moz_runtime_t *moz_runtime_init(unsigned jmptbl, unsigned memo)
{
    moz_runtime_t *r;
    unsigned size = sizeof(*r) + sizeof(long) * (MOZ_DEFAULT_STACK_SIZE - 1);
    r = (moz_runtime_t *)VM_CALLOC(1, size);
    r->ast = AstMachine_init(MOZ_AST_MACHINE_DEFAULT_LOG_SIZE, NULL);
    r->table = symtable_init();
    r->memo = memo_init(MOZ_MEMO_DEFAULT_WINDOW_SIZE, memo);
    r->head = r->input = r->tail = NULL;
    if (jmptbl) {
        r->C.jumps = (int *)VM_MALLOC(sizeof(int) * MOZ_JMPTABLE_SIZE * jmptbl);
    }
    memset(&r->stack_[0], 0xaa, sizeof(long) * MOZ_DEFAULT_STACK_SIZE);
    memset(&r->stack_[MOZ_DEFAULT_STACK_SIZE - 0xf], 0xbb, sizeof(long) * 0xf);
    r->stack = &r->stack_[0] + 0xf;

    r->C.memo_size = memo;
    r->C.jumptable_size = jmptbl;
    return r;
}

void moz_runtime_reset(moz_runtime_t *r)
{
    unsigned memo = r->C.memo_size;
    AstMachine_dispose(r->ast);
    symtable_dispose(r->table);
    memo_dispose(r->memo);

    r->ast = AstMachine_init(MOZ_AST_MACHINE_DEFAULT_LOG_SIZE, NULL);
    r->table = symtable_init();
    r->memo = memo_init(MOZ_MEMO_DEFAULT_WINDOW_SIZE, memo);
}

void moz_runtime_set_source(moz_runtime_t *r, const char *str, const char *end)
{
    r->head = str;
    r->tail = end;
    AstMachine_setSource(r->ast, str);
}

void moz_runtime_print_stats(moz_runtime_t *r)
{
    MOZVM_PROFILE_EACH(MOZVM_PROFILE_SHOW);
}

void moz_runtime_dispose(moz_runtime_t *r)
{
    unsigned i;
    AstMachine_dispose(r->ast);
    symtable_dispose(r->table);
    memo_dispose(r->memo);
    if (r->C.jumps) {
        VM_FREE(r->C.jumps);
    }

    if (r->C.set_size) {
        VM_FREE(r->C.sets);
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
    if (r->C.nterm_size) {
        for (i = 0; i < r->C.nterm_size; i++) {
            pstring_delete((const char *)r->C.nterms[i]);
        }
        VM_FREE(r->C.nterms);
    }
    VM_FREE(r);
}

#define CONSUME() ++CURRENT
#define CONSUME_N(N) CURRENT += N
#define FAIL_IMPL() do { \
    long saved_, ast_tx_; \
    moz_inst_t *jump_; \
    const char *pos_; \
    MOZVM_PROFILE_INC(FAIL_COUNT); \
    POP_FRAME(pos_, jump_, ast_tx_, saved_); \
    if (pos_ < CURRENT) { \
        HEAD = (HEAD < CURRENT) ? CURRENT : HEAD; \
        CURRENT = pos_; \
    } \
    ast_rollback_tx(AST_MACHINE_GET(), ast_tx_); \
    symtable_rollback(SYMTABLE_GET(), saved_); \
    /* fprintf(stderr, "%-8s fail  SP=%p FP=%p jump=%p\n", */ \
    /*         runtime->nterms[nterm_id], SP, FP, jump_);  */ \
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
#define TODO() __asm volatile("int3")
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

#define POP_FRAME(POS, NEXT, AST, SYMTBL) do {\
    SP     = FP; \
    SYMTBL = FP[FP_SYMTBL];\
    AST    = FP[FP_AST];\
    NEXT   = (moz_inst_t *)FP[FP_NEXT];\
    POS    = (const char *)FP[FP_POS];\
    FP     = (long *)FP[FP_FP]; \
} while (0)

#define PEEK_FRAME(POS, NEXT, AST, SYMTBL) do {\
    SYMTBL = (FP+FP_SYMTBL);\
    AST    = (FP+FP_AST);\
    NEXT   = (moz_inst_t **)(FP+FP_NEXT);\
    POS    = (const char **)(FP+FP_POS);\
} while (0)

long moz_runtime_parse(moz_runtime_t *runtime, const char *CURRENT, const moz_inst_t *PC)
{
    long *SP = runtime->stack;
    long *FP = SP;
#ifdef MOZVM_DEBUG_NTERM
    long nterm_id = 0;
#endif
#if 1
    AstMachine *AST = runtime->ast;
    symtable_t *TBL = runtime->table;
    memo_t *MEMO    = runtime->memo;
#else
#define AST  runtime->ast
#define TBL  runtime->table
#define MEMO runtime->memo
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

    // Instruction layout
    //   PC[0]  Exit
    //   PC[1]  0    /*parse success*/
    //   PC[2]  Exit
    //   PC[3]  0    /*parse fail   */
    //   PC[4]  ...
#ifdef MOZVM_USE_DIRECT_THREADING
    assert(*(void const **)PC == &&LABEL(Exit));
#else
    assert(*PC == Exit);
#endif
    PUSH_FRAME(CURRENT,
#ifdef MOZVM_USE_DIRECT_THREADING
            PC + sizeof(void *) + 1,
#else
            PC + 2,
#endif
            ast_save_tx(AST), symtable_savepoint(TBL));
#ifdef MOZVM_DEBUG_NTERM
    PUSH(nterm_id);
#endif
    PUSH(PC);
#ifdef MOZVM_USE_DIRECT_THREADING
    PC += 2 * (sizeof(void *) + 1);
#else
    PC += 4;
#endif
#define read_uint8_t(PC)  *(PC);             PC += sizeof(uint8_t)
#define read_int8_t(PC)   *((int8_t *)PC);   PC += sizeof(int8_t)
#define read_int(PC)      *((int *)PC);      PC += sizeof(int)
#define read_uint16_t(PC) *((uint16_t *)PC); PC += sizeof(uint16_t)
#define read_uint32_t(PC) *((uint32_t *)PC); PC += sizeof(uint32_t)
#define read_STRING_t(PC) *((STRING_t *)PC); PC += sizeof(STRING_t)
#define read_BITSET_t(PC) *((BITSET_t *)PC); PC += sizeof(BITSET_t)
#define read_TAG_t(PC)    *((TAG_t *)PC);    PC += sizeof(TAG_t)
#define read_JMPTBL_t(PC) *((JMPTBL_t *)PC); PC += sizeof(JMPTBL_t)

#define SYMTABLE_GET() (TBL)
#define AST_MACHINE_GET() (AST)
#define MEMO_GET() (MEMO)
#define HEAD (runtime)->head
#define EOS() (CURRENT == runtime->tail)

#define OP_CASE_(OP) LABEL(OP): MOZVM_PROFILE_INC(INST_COUNT);
#ifdef PRINT_INST
#ifdef MOZVM_DEBUG_NTERM
#define OP_CASE(OP) OP_CASE_(OP); fprintf(stderr, "%-8s SP=%p FP=%p %ld %s\n", runtime->C.nterms[nterm_id], SP, FP, (long)(PC-1), #OP);
#else
// #define OP_CASE(OP) LABEL(OP):; fprintf(stderr, "SP=%p FP=%p %ld %s\n", SP, FP, (long)(PC-1), #OP);
#if PRINT_INST == 1
#define OP_CASE(OP) OP_CASE_(OP); fprintf(stdout, "%-8s,\n", #OP);
#else
    uint8_t last = Exit;
#define OP_CASE(OP) OP_CASE_(OP); fprintf(stdout, "%-8s->%-8s,\n", opcode2str(last), #OP); last = *(PC-1);
#endif
#endif
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
