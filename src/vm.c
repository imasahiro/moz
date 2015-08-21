#include <stdio.h>
// #if 1
// #define MOZVM_DUMP_OPCODE
// #endif
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

moz_runtime_t *moz_runtime_init(unsigned jmptbl, unsigned memo)
{
    moz_runtime_t *r;
    unsigned size = sizeof(*r) + sizeof(long) * (MOZ_DEFAULT_STACK_SIZE - 1);
    r = (moz_runtime_t *)VM_CALLOC(1, size);
    r->ast = AstMachine_init(MOZ_AST_MACHINE_DEFAULT_LOG_SIZE, NULL);
    r->table = symtable_init();
    r->memo = memo_init(MOZ_MEMO_DEFAULT_WINDOW_SIZE, memo, MEMO_TYPE_ELASTIC);
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
    r->memo = memo_init(MOZ_MEMO_DEFAULT_WINDOW_SIZE, memo, MEMO_TYPE_ELASTIC);
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

#define CONSUME() ++CURRENT;
#define CONSUME_N(N) CURRENT += N;

#if 0
#define FAIL() do {\
    long saved_, ast_tx_; \
    moz_inst_t *jump_; \
    char *pos_; \
    POP_FRAME(pos_, jump_, ast_tx_, saved_); \
    if (pos_ < CURRENT) { \
        HEAD = (HEAD < CURRENT) ? CURRENT : HEAD; \
        CURRENT = pos_; \
    } \
    ast_rollback_tx(AST_MACHINE_GET(), ast_tx_); \
    symtable_rollback(SYMTABLE_GET(), saved_); \
    PC = jump_; \
} while (0)
#else
#define FAIL() /*fprintf(stderr, "goto fail\n");*/goto L_fail;
#endif

#if 0
static long _POP(long **SP)
{
    long v;
    *SP -= 1;
    v = **SP;
    if (v == 0xaaaaaaaaaaaaaaaaL) {
        fprintf(stderr, "stack protection(underflow) %ld\n", **SP);
        asm volatile("int3");
    }
    // fprintf(stderr, "pop %ld\n", v);
    return v;
}
static void _PUSH(long **SP, long v)
{
    if (**SP == 0xbbbbbbbbbbbbbbbbL) {
        fprintf(stderr, "stack protection(overflow) %ld\n", **SP);
        asm volatile("int3");
    }
    **SP = v;
    *SP += 1;
    // fprintf(stderr, "push %ld\n", v);
}
#define PUSH(X)  _PUSH(&SP, (long)X)
#define POP()  _POP(&SP)
#else
#define PUSH(X) *SP++ = (long)(X)
#define POP()  *--SP
#endif
#define PEEK() PEEK_N(1)
#define PEEK_N(N) ((SP)+N)
#define ABORT() __asm volatile("int3")
#define TODO() __asm volatile("int3")
#define FP_FP     0
#define FP_POS    1
#define FP_NEXT   2
#define FP_AST    3
#define FP_SYMTBL 4
#define FP_MAX    (5)

#define PUSH_FRAME(POS, NEXT, AST, SYMTBL) do {\
    PUSH((long)FP); \
    PUSH(POS);\
    PUSH(NEXT);\
    PUSH(AST);\
    PUSH(SYMTBL);\
    FP = SP - FP_MAX;\
} while (0)

#define POP_FRAME(POS, NEXT, AST, SYMTBL) do {\
    SP     = FP; \
    SYMTBL = FP[FP_SYMTBL];\
    AST    = FP[FP_AST];\
    NEXT   = (moz_inst_t *)FP[FP_NEXT];\
    POS    = (char *)FP[FP_POS];\
    FP     = (long *)FP[FP_FP]; \
} while (0)

#define PEEK_FRAME(POS, NEXT, AST, SYMTBL) do {\
    SYMTBL = (FP+FP_SYMTBL);\
    AST    = (FP+FP_AST);\
    NEXT   = (moz_inst_t **)(FP+FP_NEXT);\
    POS    = (char **)(FP+FP_POS);\
} while (0)

int moz_runtime_parse(moz_runtime_t *runtime, char *CURRENT, char *end, moz_inst_t *PC)
{
    long *SP = runtime->stack;
    long *FP = SP;
#ifdef MOZVM_DEBUG_NTERM
    long nterm_id = 0;
#endif
    AstMachine *AST = runtime->ast;
    symtable_t *TBL = runtime->table;
    memo_t *memo = runtime->memo;
    runtime->head = CURRENT;
    runtime->tail = end;
    AstMachine_setSource(AST, CURRENT);

    assert(*PC == Exit);
    PUSH_FRAME(CURRENT, PC, ast_save_tx(AST), symtable_savepoint(TBL));
    // fprintf(stderr, "%-8s SP=%p FP=%p %ld %s\n", runtime->nterms[nterm_id], SP, FP, (long)PC, "init");
    // PUSH(0xaaaaaaaaL);
    // PUSH(0xaaaaaaaaL);
#ifdef MOZVM_DEBUG_NTERM
    PUSH(nterm_id);
#endif
    PUSH(PC++);
    // fprintf(stderr, "%-8s SP=%p FP=%p %ld %s\n", runtime->nterms[nterm_id], SP, FP, (long)PC, "init");
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
#define MEMO_GET() (memo)
#define HEAD (runtime)->head
#define EOS() (CURRENT == runtime->tail)
#define DISPATCH() goto L_vm_head
#define NEXT() DISPATCH()
#define JUMP(N) PC += N; DISPATCH()

#if 0
    uint8_t opcode = *PC;
#define DISPATCH_START(PC) L_vm_head:;switch ((opcode = *PC++)) {
#else
#define DISPATCH_START(PC) L_vm_head:;switch (*PC++) {
#endif
#define DISPATCH_END() default: ABORT(); }
#if 0
#define OP_CASE(OP) case OP:fprintf(stderr, "%-8s SP=%p FP=%p %ld %s\n", runtime->nterms[nterm_id], SP, FP, (long)(PC-1), #OP);
#else
#define OP_CASE(OP) case OP:
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
