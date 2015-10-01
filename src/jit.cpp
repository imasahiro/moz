#ifdef MOZVM_ENABLE_JIT
#include "jit.h"
#include "pstring.h"
#include "jmptbl.h"
#include "instruction.h"
#include "karray.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/PassManager.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "jit_types.cpp"

// #define MOZVM_JIT_OPTIMIZE_MODULE 1
// #define MOZVM_JIT_DUMP 1
#define MOZVM_JIT_RESET_COMPILED_CODE 1

using namespace std;
using namespace llvm;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define CREATE_FUNCTION(F, M) \
    Function::Create(GetFuncType(F), Function::InternalLinkage, #F, M)

typedef unordered_map<moz_inst_t *, BasicBlock *> BBMap;
typedef unordered_map<void *, GlobalVariable *> ConstMap;

class JitContext {
private:
public:
    ExecutionEngine *EE;
    Module *curMod;
    Module *topMod;

    ~JitContext() {};
    JitContext(moz_runtime_t *r);
};

struct Symbol {
    const char *name;
    void *func;
};
static const Symbol symbols[] = {
#define DEFINE_SYMBOL(S) { #S, reinterpret_cast<void *>(S) }
    DEFINE_SYMBOL(ast_rollback_tx),
    DEFINE_SYMBOL(ast_commit_tx),
    DEFINE_SYMBOL(ast_log_replace),
    DEFINE_SYMBOL(ast_log_new),
    DEFINE_SYMBOL(ast_log_pop),
    DEFINE_SYMBOL(ast_log_push),
    DEFINE_SYMBOL(ast_log_swap),
    DEFINE_SYMBOL(ast_log_tag),
    DEFINE_SYMBOL(ast_log_link),
    DEFINE_SYMBOL(memo_set),
    DEFINE_SYMBOL(memo_fail),
    DEFINE_SYMBOL(memo_get)
#undef DEFINE_SYMBOL
};

static inline bool nterm_has_inst(mozvm_nterm_entry_t *e, moz_inst_t *inst)
{
    return e->begin <= inst && inst <= e->end;
}

static BasicBlock *get_jump_destination(mozvm_nterm_entry_t *e, moz_inst_t *dest, BasicBlock *failBB)
{
    if(!nterm_has_inst(e, dest) && *dest == Fail) {
        return failBB;
    } else {
        LLVMContext &Ctx = getGlobalContext();
        return BasicBlock::Create(Ctx, "jump.label");
    }
    return nullptr;
}

template<typename Vector, typename First>
void set_vector(const Vector &dest, const First &first)
{
    dest->push_back(first);
}

template<typename Vector, typename First, typename... Rest>
void set_vector(const Vector &dest, const First &first, const Rest&... rest)
{
    set_vector(dest, first);
    set_vector(dest, rest...);
}

template<typename... Args>
Value *create_gep(IRBuilder<> &builder, Value *V, const Args&... args)
{
    vector<Value *> indexs;
    set_vector(&indexs, args...);
    ArrayRef<Value *> idxsRef(indexs);

    return builder.CreateGEP(V, idxsRef);
}

template<typename... Args>
Value *create_call(IRBuilder<> &builder, Value *F, const Args&... args)
{
    vector<Value *> arguments;
    set_vector(&arguments, args...);
    ArrayRef<Value *> argsRef(arguments);

    return builder.CreateCall(F, argsRef);
}

static Value *GetCur(IRBuilder<> &builder, Value *str, Value *pos)
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    return pos;
#else
    return builder.CreateGEP(str, pos);
#endif
}

static Value *get_length(IRBuilder<> &builder, Value *begin, Value *end)
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    Type *I64Ty = builder.getInt64Ty();
    begin = builder.CreatePtrToInt(begin, I64Ty);
    end   = builder.CreatePtrToInt(end,   I64Ty);
    return builder.CreateSub(end, begin);
#else
    return builder.CreateSub(end, begin);
#endif
}

static Value *consume_n(IRBuilder<> &builder, Value *pos, Value *N)
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    return builder.CreateGEP(pos, N);
#else
    return builder.CreateAdd(pos, N);
#endif
}

static Value *consume(IRBuilder<> &builder, Value *pos)
{
    return consume_n(builder, pos, builder.getInt64(1));
}

static void stack_push(IRBuilder<> &builder, Value *sp, Value *I64Val)
{
    Value *top = builder.CreateLoad(sp);
    Value *next_top = builder.CreateGEP(top, builder.getInt64(1));
    builder.CreateStore(next_top, sp);
    builder.CreateStore(I64Val, top);
}

static Value *stack_pop(IRBuilder<> &builder, Value *sp)
{
    Value *top = builder.CreateLoad(sp);
    Value *prev_top = builder.CreateGEP(top, builder.getInt64(-1));
    builder.CreateStore(prev_top, sp);
    return builder.CreateLoad(prev_top);
}

static void stack_push_pos(IRBuilder<> &builder, Value *sp, Value *pos)
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    Value *pos_ = builder.CreatePtrToInt(pos, builder.getInt64Ty());
    stack_push(builder, sp, pos_);
#else
    stack_push(builder, sp, pos);
#endif
}

static Value *stack_pop_pos(IRBuilder<> &builder, Value *sp)
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    Value *pos_ = stack_pop(builder, sp);
    return builder.CreateIntToPtr(pos_, builder.getInt8PtrTy());
#else
    return stack_pop(builder, sp);
#endif
}

DEF_ARRAY_T_OP_NOPOINTER(long);

struct StackFrame {
    ARRAY(long) stack;
    long sp;
    long fp;
};

static void StackFrame_init(StackFrame *frame)
{
    ARRAY_init(long, &frame->stack, 0);
    frame->sp = frame->fp = 0;
    ARRAY_add(long, &frame->stack, INTPTR_MAX);
}

static void StackFrame_dispose(StackFrame *frame)
{
    ARRAY_pop(long, &frame->stack);
    ARRAY_dispose(long, &frame->stack);
}

#if 0
static void stack_push_frame(StackFrame *stack,
        Value *pos, Value *next, Value *ast, Value *tbl)
{
    long fp = stack->fp;
    stack->fp = ARRAY_size(stack->stack);
    ARRAY_ensureSize(long, &stack->stack, 5);
    ARRAY_set(long, &stack->stack, fp + 0, fp);
    ARRAY_set(long, &stack->stack, fp + 1, (long) pos);
    ARRAY_set(long, &stack->stack, fp + 2, (long) next);
    ARRAY_set(long, &stack->stack, fp + 3, (long) ast);
    ARRAY_set(long, &stack->stack, fp + 4, (long) tbl);
    stack->sp = fp + 5;
    ARRAY_size(stack->stack) = fp + 5;
}

static void stack_pop_frame(StackFrame *stack,
        Value **pos, Value **next, Value **ast, Value **tbl)
{
    long fp = stack->fp;
    stack->sp = stack->fp;
    stack->fp = ARRAY_get(long, &stack->stack, fp + 0);
    *pos  = (Value *)ARRAY_get(long, &stack->stack, fp + 1);
    *next = (Value *)ARRAY_get(long, &stack->stack, fp + 2);
    *ast  = (Value *)ARRAY_get(long, &stack->stack, fp + 3);
    *tbl  = (Value *)ARRAY_get(long, &stack->stack, fp + 4);
    ARRAY_size(stack->stack) = stack->fp;
}

static void stack_peek_frame(StackFrame *stack,
        Value **pos, Value **next, Value **ast, Value **tbl)
{
    long fp = stack->fp;
    *pos  = (Value *)ARRAY_get(long, &stack->stack, fp + 1);
    *next = (Value *)ARRAY_get(long, &stack->stack, fp + 2);
    *ast  = (Value *)ARRAY_get(long, &stack->stack, fp + 3);
    *tbl  = (Value *)ARRAY_get(long, &stack->stack, fp + 4);
}
#endif

static void stack_push_frame(IRBuilder<> &builder, Value *sp, Value *fp,
        Value *pos, Value *next, Value *ast, Value *symtable)
{
    Type *I64Ty = builder.getInt64Ty();
    Value *top = builder.CreateLoad(sp);

    Value *frame  = builder.CreateLoad(fp);
    Value *frame_ = builder.CreatePtrToInt(frame, I64Ty);
    Value *sp_fp  = top;
    builder.CreateStore(frame_, sp_fp);

    Value *sp_pos = builder.CreateGEP(top, builder.getInt64(1));
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    Value *pos_   = builder.CreatePtrToInt(pos, I64Ty);
    builder.CreateStore(pos_, sp_pos);
#else
    builder.CreateStore(pos,  sp_pos);
#endif

    Value *sp_next = builder.CreateGEP(top, builder.getInt64(2));
    Value *next_  = builder.CreatePtrToInt(next, I64Ty);
    builder.CreateStore(next_, sp_next);

    Value *sp_ast = builder.CreateGEP(top, builder.getInt64(3));
    builder.CreateStore(ast, sp_ast);

    Value *sp_symtable = builder.CreateGEP(top, builder.getInt64(4));
    builder.CreateStore(symtable, sp_symtable);

    builder.CreateStore(top, fp);
    Value *next_top = builder.CreateGEP(top, builder.getInt64(5));
    builder.CreateStore(next_top, sp);
}

static void stack_drop_frame(IRBuilder<> &builder, Value *sp, Value *fp)
{
    Type *I64PtrTy = builder.getInt64Ty()->getPointerTo();
    Value *frame = builder.CreateLoad(fp);
    builder.CreateStore(frame, sp);

    Value *fp_fp       = frame;
    Value *prev_frame_ = builder.CreateLoad(fp_fp);
    Value *prev_frame  = builder.CreateIntToPtr(prev_frame_, I64PtrTy);
    builder.CreateStore(prev_frame, fp);
}

static void stack_pop_frame(IRBuilder<> &builder, Value *sp, Value *fp,
        Value **pos, Value **next, Value **ast, Value **symtable)
{
    Type *I8PtrTy  = builder.getInt8PtrTy();
    Type *I64PtrTy = builder.getInt64Ty()->getPointerTo();
    Value *frame = builder.CreateLoad(fp);
    builder.CreateStore(frame, sp);

    Value *fp_symtable = builder.CreateGEP(frame, builder.getInt64(4));
    *symtable = builder.CreateLoad(fp_symtable);

    Value *fp_ast = builder.CreateGEP(frame, builder.getInt64(3));
    *ast = builder.CreateLoad(fp_ast);

    Value *fp_next = builder.CreateGEP(frame, builder.getInt64(2));
    Value *next_   = builder.CreateLoad(fp_next);
    *next = builder.CreateIntToPtr(next_, I8PtrTy);

    Value *fp_pos = builder.CreateGEP(frame, builder.getInt64(1));
    Value *pos_   = builder.CreateLoad(fp_pos);
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    *pos = builder.CreateIntToPtr(pos_, I8PtrTy);
#else
    *pos = pos_;
#endif

    Value *fp_fp       = frame;
    Value *prev_frame_ = builder.CreateLoad(fp_fp);
    Value *prev_frame  = builder.CreateIntToPtr(prev_frame_, I64PtrTy);
    builder.CreateStore(prev_frame, fp);
}

static void stack_peek_frame(IRBuilder<> &builder, Value *sp, Value *fp,
        Value **pos, Value **next, Value **ast, Value **symtable)
{
    Type *I8PtrPtrTy  = builder.getInt8PtrTy()->getPointerTo();
    Value *frame = builder.CreateLoad(fp);

    *symtable = builder.CreateGEP(frame, builder.getInt64(4));

    *ast = builder.CreateGEP(frame, builder.getInt64(3));

    Value *next_ = builder.CreateGEP(frame, builder.getInt64(2));
    *next = builder.CreateBitCast(next_, I8PtrPtrTy);

    Value *pos_ = builder.CreateGEP(frame, builder.getInt64(1));
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    *pos = builder.CreateBitCast(pos_, I8PtrPtrTy);
#else
    *pos = pos_;
#endif
}

static Value *get_callee_function(IRBuilder<> &builder, Value *runtime, uint16_t nterm)
{
    Value *r_nterm_entry = create_gep(builder, runtime,
            builder.getInt64(0),
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
            builder.getInt32(11)
#else
            builder.getInt32(10)
#endif
            );
    Value *entry_head = builder.CreateLoad(r_nterm_entry);
    Value *callee     = create_gep(builder, entry_head,
            builder.getInt64(nterm), builder.getInt32(3));
    return builder.CreateLoad(callee);
}

static Value *get_bitset_ptr(IRBuilder<> &builder, Value *runtime, BITSET_t id)
{
#if MOZVM_SMALL_BITSET_INST
    Value *r_c_sets = create_gep(builder, runtime,
            builder.getInt64(0),
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
            builder.getInt32(12),
#else
            builder.getInt32(11),
#endif /*MOZVM_USE_DYNAMIC_DEACTIVATION*/
            builder.getInt32(0));
    Value *sets_head = builder.CreateLoad(r_c_sets);
    return builder.CreateGEP(sets_head, builder.getInt64(id));
#else
    asm volatile("int3");
    // bitset_t *_set = (bitset_t *)id;
    return nullptr;
#endif /*MOZVM_SMALL_BITSET_INST*/
}

static Value *get_tag_id(IRBuilder<> &builder, Value *runtime, TAG_t id)
{
#if MOZVM_SMALL_TAG_INST
    return builder.getInt16(id);
#else
    asm volatile("int3");
    // uint16_t _tagId = (uint16_t)(runtime->C.tags - tagId);
    return nullptr;
#endif /*MOZVM_SMALL_TAG_INST*/
}

static Value *get_tag_ptr(IRBuilder<> &builder, JitContext *ctx, Value *runtime, TAG_t id)
{
#if MOZVM_SMALL_TAG_INST
    Value *r_c_tags = create_gep(builder, runtime,
            builder.getInt64(0),
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
            builder.getInt32(12),
#else
            builder.getInt32(11),
#endif /*MOZVM_USE_DYNAMIC_DEACTIVATION*/
            builder.getInt32(1));
    Value *tags_head = builder.CreateLoad(r_c_tags);
    Value *tag_head  = builder.CreateGEP(tags_head, builder.getInt64(id));
    return builder.CreateLoad(tag_head);
#else
    asm volatile("int3");
    // tag_t *_tag = (tag_t *)id;
    return nullptr;
#endif /*MOZVM_SMALL_TAG_INST*/
}

template<unsigned N>
static Value *get_jump_table(IRBuilder<> &builder, Value *runtime, uint16_t id)
{
#ifdef MOZVM_USE_JMPTBL
    Value *r_c_jumps = create_gep(builder, runtime,
            builder.getInt64(0),
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
            builder.getInt32(12),
#else
            builder.getInt32(11),
#endif /*MOZVM_USE_DYNAMIC_DEACTIVATION*/
            builder.getInt32(4 + N));
    Value *jumps_head = builder.CreateLoad(r_c_jumps);
    return builder.CreateGEP(jumps_head, builder.getInt64(id));
#else
    return nullptr;
#endif /*MOZVM_USE_JMPTBL*/
}

// static Value *emit_global_variable(JitContext *ctx, Type *Ty, const char *name, void *addr)
// {
//     GlobalVariable *G;
//     Module *M  = ctx->curMod;
//     ExecutionEngine *EE = ctx->EE;
//
//     // if ((G = M->getNamedGlobal(name)) != 0) {
//     //     void *oldAddr = EE->getPointerToGlobal(G);
//     //     if (oldAddr == addr) {
//     //         return G;
//     //     }
//     // }
//     G = new GlobalVariable(*M, Ty, true,
//             GlobalVariable::ExternalLinkage, NULL, name);
//     EE->addGlobalMapping(G, addr);
//     return G;
// }
//
// static void register_constants(JitContext *ctx, moz_runtime_t *r)
// {
//     char name[128];
//     mozvm_constant_t *C = &r->C;
//     Type *StrTy = Type::getInt8PtrTy(getGlobalContext());
//     Type *SetTy = GetType<bitset_t>();
//     for (int i = 0; i < C->tag_size; i++) {
//         snprintf(name, 128, "tag%d", i);
//         emit_global_variable(ctx, StrTy, name, (void *)C->tags[i]);
//     }
//     for (int i = 0; i < C->str_size; i++) {
//         snprintf(name, 128, "str%d", i);
//         emit_global_variable(ctx, StrTy, name, (void *)C->tags[i]);
//     }
//     for (int i = 0; i < C->set_size; i++) {
//         snprintf(name, 128, "set%d", i);
//         emit_global_variable(ctx, SetTy, name, (void *)&C->sets[i]);
//     }
// }
//
// typedef enum MozConstType {
//     MozConstTypeTag,
//     MozConstTypeStr,
//     MozConstTypeSet
// } MozConstType;
//
// Value *find_constant(IRBuilder<> &builder, JitContext *ctx, MozConstType Ty, uint16_t id, void *p)
// {
//     char name[128];
//     Type *StrTy = Type::getInt8PtrTy(getGlobalContext());
//     Type *SetTy = GetType<bitset_t *>();
//     Type *Type  = StrTy;
//     switch (Ty) {
//     case MozConstTypeTag:
//         snprintf(name, 128, "tag%d", id);
//         break;
//     case MozConstTypeStr:
//         snprintf(name, 128, "str%d", id);
//         break;
//     case MozConstTypeSet:
//         snprintf(name, 128, "set%d", id);
//         Type = SetTy;
//         break;
//     }
//     Value  *V = builder.getInt64((uintptr_t)p);
//     return builder.CreateIntToPtr(V, Type);
// }

#if 0
static Value *get_string_ptr(IRBuilder<> &builder, JitContext *ctx, moz_runtime_t *runtime, STRING_t id)
{
    STRING_t id_ = 0;
    void *impl = NULL;
#ifdef MOZVM_SMALL_STRING_INST
    id_  = id;
    impl = (void *)STRING_GET_IMPL(runtime, id);
#else
    id_  = id - R->C.strs;
    impl = (void *)id;
#endif
    return find_constant(builder, ctx, MozConstTypeStr, id_, impl);
}
#else
static Value *get_string_ptr(IRBuilder<> &builder, Value *runtime, STRING_t id)
{
#if MOZVM_SMALL_STRING_INST
    Value *r_c_strs = create_gep(builder, runtime,
            builder.getInt64(0),
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
            builder.getInt32(12),
#else
            builder.getInt32(11),
#endif /*MOZVM_USE_DYNAMIC_DEACTIVATION*/
            builder.getInt32(2));
    Value *strs_head = builder.CreateLoad(r_c_strs);
    Value *str_head  = builder.CreateGEP(strs_head, builder.getInt64(id));
    return builder.CreateLoad(str_head);
#else
    asm volatile("int3");
    // const char *_str = (const char *)id;
    return nullptr;
#endif /*MOZVM_SMALL_STRING_INST*/
}
#endif

static void create_bitset_get(IRBuilder<> &builder, Module *M)
{
    LLVMContext &Ctx = M->getContext();
    Type *I32Ty = builder.getInt32Ty();
    Type *I64Ty = builder.getInt64Ty();
    Constant *i32_0 = builder.getInt32(0);
    Constant *i64_0 = builder.getInt64(0);
    Function *F = CREATE_FUNCTION(bitset_get, M);

    Function::arg_iterator arg_iter = F->arg_begin();
    Value *set = arg_iter++;
    Value *idx = arg_iter++;

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);


#if defined(BITSET_USE_ULONG)
    Value *Mod    = builder.CreateAnd(idx, builder.getInt32(63));
    Value *Mod_   = builder.CreateZExt(Mod, I64Ty);
    Value *Mask   = builder.CreateShl(builder.getInt64(1), Mod_);
    Value *Div    = builder.CreateLShr(idx, builder.getInt32(6));
    Value *Div_   = builder.CreateZExt(Div, I64Ty);
    Value *Ptr    = create_gep(builder, set, i64_0, i32_0, Div_);
    Value *Data   = builder.CreateLoad(Ptr);
    Value *Result = builder.CreateAnd(Data, Mask);
    Value *NEq0   = builder.CreateICmpNE(Result, i64_0);
#elif defined(BITSET_USE_UINT)
    Value *Mod    = builder.CreateAnd(idx, builder.getInt32(31));
    Value *Mask   = builder.CreateShl(builder.getInt32(1), Mod);
    Value *Div    = builder.CreateLShr(idx, builder.getInt32(5));
    Value *Div_   = builder.CreateZExt(Div, I64Ty);
    Value *Ptr    = create_gep(builder, set, i64_0, i32_0, Div_);
    Value *Data   = builder.CreateLoad(Ptr);
    Value *Result = builder.CreateAnd(Data, Mask);
    Value *NEq0   = builder.CreateICmpNE(Result, i32_0);
#endif
    Value *RetVal = builder.CreateZExt(NEq0, I32Ty);
    builder.CreateRet(RetVal);
}

#ifdef MOZVM_USE_JMPTBL
template<unsigned N>
static void create_jump_table_index(IRBuilder<> &builder, Module *M)
{
    char fname[128];
    assert(N <= 3);
    snprintf(fname, 128, "jump_table%d_index", N);
    FunctionType *FuncTypes[] = {
        GetFuncType(jump_table1_jump),
        GetFuncType(jump_table2_jump),
        GetFuncType(jump_table3_jump)
    };

    LLVMContext &Ctx = M->getContext();
    Type *I32Ty = builder.getInt32Ty();
    FunctionType *FTy = FuncTypes[N - 1];
    Constant *i32_0 = builder.getInt32(0);
    Constant *i64_0 = builder.getInt64(0);

    Constant *f_bitsetget = REGISTER_FUNC(M, bitset_get);
    Function *F = Function::Create(FTy, Function::InternalLinkage, fname, M);

    Function::arg_iterator arg_iter = F->arg_begin();
    Value *tbl = arg_iter++;
    Value *ch  = arg_iter++;

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

    Value *ch_ = builder.CreateZExt(ch, I32Ty);
    Value *idx = nullptr;
    for(int i = 0; i < N; i++) {
        Value *C = builder.getInt64(i);
        Value *tbl_set = create_gep(builder, tbl, i64_0, i32_0, C);
        Value *idx_ = create_call(builder, f_bitsetget, tbl_set, ch_);
        if(i == 0) {
            idx = idx_;
        }
        else {
            Value *idx_shl = builder.CreateShl(idx_, builder.getInt32(i));
            idx = builder.CreateOr(idx, idx_shl);
        }
    }
    builder.CreateRet(idx);
}
#endif

static void create_pstring_startswith(IRBuilder<> &builder, Module *M)
{
    LLVMContext &Ctx = M->getContext();
    Type *I64Ty = builder.getInt64Ty();
    Type *I8PtrTy  = builder.getInt8PtrTy();
    Function *F = CREATE_FUNCTION(pstring_starts_with, M);

    Function::arg_iterator arg_iter = F->arg_begin();
    Value *str  = arg_iter++;
    Value *text = arg_iter++;
    Value *len  = arg_iter++;

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

// #ifdef __AVX2__
// #endif
#if defined(PSTRING_USE_STRCMP)
    FunctionType *strncmpTy = GetFuncType(strncmp);
    Constant *f_strncmp = REGISTER_FUNC(M, strncmp);

    Value *len_   = builder.CreateZExt(len, I64Ty);
    Value *result = create_call(builder, f_strncmp, str, text, len_);
    Value *cond   = builder.CreateICmpEQ(result, builder.getInt32(0));
    Value *cond_  = builder.CreateZExt(cond, I32Ty);
    builder.CreateRet(cond_);
#else
    Constant *i64_1 = builder.getInt64(1);
    BasicBlock *wcondBB = BasicBlock::Create(Ctx, "while.cond", F);
    BasicBlock *wbodyBB = BasicBlock::Create(Ctx, "while.body", F);
    BasicBlock *wendBB  = BasicBlock::Create(Ctx, "while.end", F);
    BasicBlock *wretBB  = BasicBlock::Create(Ctx, "while.return", F);
    Value *len_ = builder.CreateZExt(len, I64Ty);
    Value *end  = builder.CreateGEP(text, len_);
    builder.CreateBr(wcondBB);

    builder.SetInsertPoint(wcondBB);
    PHINode *currentText = builder.CreatePHI(I8PtrTy, 2);
    currentText->addIncoming(text, entryBB);
    PHINode *currentStr  = builder.CreatePHI(I8PtrTy, 2);
    currentStr->addIncoming(str, entryBB);
    Value *whilecond = builder.CreateICmpULT(currentText, end);
    builder.CreateCondBr(whilecond, wbodyBB, wendBB);

    builder.SetInsertPoint(wbodyBB);
    Value *text_char = builder.CreateLoad(currentText);
    Value *str_char  = builder.CreateLoad(currentStr);
    Value *nextText  = builder.CreateGEP(currentText, i64_1);
    currentText->addIncoming(nextText, wbodyBB);
    Value *nextStr   = builder.CreateGEP(currentStr, i64_1);
    currentStr->addIncoming(nextStr, wbodyBB);
    Value *cond = builder.CreateICmpEQ(str_char, text_char);
    builder.CreateCondBr(cond, wcondBB, wretBB);

    builder.SetInsertPoint(wendBB);
    builder.CreateRet(builder.getInt32(1));

    builder.SetInsertPoint(wretBB);
    builder.CreateRet(builder.getInt32(0));
#endif
}

static void create_ast_save_tx(IRBuilder<> &builder, Module *M)
{
    LLVMContext &Ctx = M->getContext();
    Type *I64Ty = builder.getInt64Ty();
    Constant *i32_0 = builder.getInt32(0);
    Constant *i64_0 = builder.getInt64(0);

    Function *F = CREATE_FUNCTION(ast_save_tx, M);

    Value *ast = F->arg_begin();

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

    Value *Ptr   = create_gep(builder, ast, i64_0, i32_0, i32_0);
    Value *Size  = builder.CreateLoad(Ptr);
    Value *Size_ = builder.CreateZExt(Size, I64Ty);
    builder.CreateRet(Size_);
}

static void create_ast_get_last_linked_node(IRBuilder<> &builder, Module *M)
{
    LLVMContext &Ctx = M->getContext();
    Constant *i32_1 = builder.getInt32(1);
    Constant *i64_0 = builder.getInt64(0);

    Function *F = CREATE_FUNCTION(ast_get_last_linked_node, M);

    Value *ast = F->arg_begin();

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

    Value *Ptr    = create_gep(builder, ast, i64_0, i32_1);
    Value *Linked = builder.CreateLoad(Ptr);
    builder.CreateRet(Linked);
}

static void create_symtable_savepoint(IRBuilder<> &builder, Module *M)
{
    LLVMContext &Ctx = M->getContext();
    Type *I64Ty = builder.getInt64Ty();
    Constant *i32_0 = builder.getInt32(0);
    Constant *i32_1 = builder.getInt32(1);
    Constant *i64_0 = builder.getInt64(0);

    Function *F = CREATE_FUNCTION(symtable_savepoint, M);

    Value *tbl = F->arg_begin();

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

    Value *Ptr   = create_gep(builder, tbl, i64_0, i32_1, i32_0);
    Value *Size  = builder.CreateLoad(Ptr);
    Value *Size_ = builder.CreateZExt(Size, I64Ty);
    builder.CreateRet(Size_);
}

static void create_symtable_rollback(IRBuilder<> &builder, Module *M)
{
    LLVMContext &Ctx = M->getContext();
    Type *I32Ty = builder.getInt32Ty();
    Constant *i32_0 = builder.getInt32(0);
    Constant *i32_1 = builder.getInt32(1);
    Constant *i64_0 = builder.getInt64(0);

    Function *F = CREATE_FUNCTION(symtable_rollback, M);

    Function::arg_iterator arg_iter = F->arg_begin();
    Value *tbl   = arg_iter++;
    Value *saved = arg_iter++;

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

    Value *Ptr    = create_gep(builder, tbl, i64_0, i32_1, i32_0);
    Value *saved_ = builder.CreateTrunc(saved, I32Ty);
    builder.CreateStore(saved_, Ptr);
    builder.CreateRetVoid();
}

#ifdef MOZVM_USE_JMPTBL
template<unsigned N>
void create_call_tbljmp(IRBuilder<> &builder, Value *tbl,
        JitContext *ctx, Value *cur, Value *str, BasicBlock *unreachableBB,
        BBMap &BBMap, moz_inst_t *offset, int *jumps)
{
    char buf[128];
    snprintf(buf, 128, "jump_table%d_index", N);
    Module *M = ctx->curMod;
    FunctionType *FuncTypes[] = {
        GetFuncType(jump_table1_jump),
        GetFuncType(jump_table2_jump),
        GetFuncType(jump_table3_jump)
    };
    FunctionType *FTy = FuncTypes[N - 1];
    Constant *f = M->getOrInsertFunction(buf, FTy);

    Value *pos = builder.CreateLoad(cur);
    Value *Cur = GetCur(builder, str, pos);
    Value *Char = builder.CreateLoad(Cur);
    Value *idx = create_call(builder, f, tbl, Char);

    SwitchInst *jump = builder.CreateSwitch(idx, unreachableBB, 1 << N);
    for(int i = 0; i < 1 << N; i++) {
        if(jumps[i] != INT_MAX) {
            moz_inst_t *dest = offset + jumps[i];
            jump->addCase(builder.getInt32(i), BBMap[dest]);
        }
        else {
            jump->addCase(builder.getInt32(i), unreachableBB);
        }
    }
}
#endif

JitContext::JitContext(moz_runtime_t *r)
{
    LLVMContext &Ctx = getGlobalContext();
    IRBuilder<> builder(Ctx);

    for(unsigned i = 0; i < ARRAY_SIZE(symbols); i++) {
        const Symbol *sym = symbols + i;
        sys::DynamicLibrary::AddSymbol(sym->name, sym->func);
    }

    Module *M = new Module("libnez", Ctx);
    topMod = curMod = M;
    create_bitset_get(builder, M);
#ifdef MOZVM_USE_JMPTBL
    create_jump_table_index<1>(builder, M);
    create_jump_table_index<2>(builder, M);
    create_jump_table_index<3>(builder, M);
#endif
    create_pstring_startswith(builder, M);
    create_ast_save_tx(builder, M);
    create_ast_get_last_linked_node(builder, M);
    create_symtable_savepoint(builder, M);
    create_symtable_rollback(builder, M);

    EE = EngineBuilder(unique_ptr<Module>(M)).create();

    EE->finalizeObject();
    curMod = NULL;
}

#ifdef __cplusplus
extern "C" {
#endif

#define MOZVM_OPCODE_SIZE 1
#include "vm_inst.h"

static inline JitContext *get_context(moz_runtime_t *r)
{
    return reinterpret_cast<JitContext *>(r->jit_context);
}

void mozvm_jit_init(moz_runtime_t *runtime)
{
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    runtime->jit_context = reinterpret_cast<void *>(new JitContext(runtime));
}

void mozvm_jit_reset(moz_runtime_t *runtime)
{
#ifdef MOZVM_JIT_RESET_COMPILED_CODE
    for (int i = 0; i < runtime->C.nterm_size; i++) {
        runtime->nterm_entry[i].call_counter  = 0;
        runtime->nterm_entry[i].compiled_code = mozvm_jit_call_nterm;
    }
#endif
    delete get_context(runtime);
    runtime->jit_context = reinterpret_cast<void *>(new JitContext(runtime));
}

void mozvm_jit_dispose(moz_runtime_t *runtime)
{
    delete get_context(runtime);
    runtime->jit_context = NULL;
}

uint8_t mozvm_jit_call_nterm(moz_runtime_t *runtime, const char *str, uint16_t nterm)
{
    mozvm_nterm_entry_t *e = runtime->nterm_entry + nterm;
    if (++(e->call_counter) >  MOZVM_JIT_COUNTER_THRESHOLD) {
        moz_jit_func_t func = mozvm_jit_compile(runtime, e);
        if(func) {
            return func(runtime, str, nterm);
        }
    }
    long *SP = runtime->stack;
    long *FP = runtime->fp;
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    const char *current = runtime->cur;
#else
    const char *current = str + runtime->cur;
#endif
    long parsed;

    moz_runtime_parse_init(runtime, current, NULL);
    parsed = moz_runtime_parse(runtime, current, e->begin);
    runtime->stack = SP;
    runtime->fp    = FP;
    return parsed;
}

static int optimize(Module *M, Function *F)
{
    const int OptLevel  = 3;
    const int SizeLevel = 2;

    PassManager         MPM;
    FunctionPassManager FPM(M);
    PassManagerBuilder Builder;

    FPM.add(createVerifierPass());
    Builder.OptLevel = OptLevel;
    Builder.Inliner  = createFunctionInliningPass(OptLevel, SizeLevel);

    Builder.populateFunctionPassManager(FPM);
#ifdef MOZVM_JIT_OPTIMIZE_MODULE
    Builder.populateModulePassManager(MPM);
    // Builder.populateLTOPassManager(MPM);
#endif
    FPM.doInitialization();
    FPM.run(*F);
#ifdef MOZVM_JIT_OPTIMIZE_MODULE
    MPM.run(*M);
#endif

#ifdef MOZVM_JIT_DUMP
    F->dump();
    // M->dump();
#endif
    // if(verifyFunction(*F)) {
    //     return 1;
    // }
    return 0;
}

static void mozvm_jit_compile_init(moz_runtime_t *runtime,
        mozvm_nterm_entry_t *e, BBMap &BBMap,
        BasicBlock *failBB, BasicBlock *errBB,
        vector<BasicBlock *> &failjumpList)
{
    int *table_jumps = NULL;
    int  table_size  = 0;
    moz_inst_t *p = e->begin;
    while (p < e->end) {
        uint8_t opcode = *p;
        unsigned shift = opcode_size(opcode);
        switch (opcode) {
        case Alt:
        case Jump: {
            mozaddr_t jump = *(mozaddr_t *)(p + 1);
            moz_inst_t *dest = p + shift + jump;
            if(BBMap.find(dest) == BBMap.end()) {
                BasicBlock *label = get_jump_destination(e, dest, failBB);
                assert(label != NULL);
                BBMap[dest] = label;
                if(opcode == Alt) {
                    failjumpList.push_back(label);
                }
            }
            break;
        }
        case Call: {
            mozaddr_t next = *(mozaddr_t *)(p + 1 + sizeof(uint16_t));
            moz_inst_t *dest = p + shift + next;
            if(BBMap.find(dest) == BBMap.end()) {
                BasicBlock *label = get_jump_destination(e, dest, failBB);
                assert(label != NULL);
                BBMap[dest] = label;
            }
            break;
        }
        case Lookup:
        case TLookup: {
            mozaddr_t skip = *(mozaddr_t *)(p + shift - sizeof(mozaddr_t));
            moz_inst_t *dest = p + shift + skip;
            if(BBMap.find(dest) == BBMap.end()) {
                BasicBlock *label = get_jump_destination(e, dest, failBB);
                assert(label != NULL);
                BBMap[dest] = label;
            }
            break;
        }
        case First: {
            JMPTBL_t tblId = *(JMPTBL_t *)(p + 1);
            moz_inst_t *jmpoffset = p + shift;
            table_jumps = JMPTBL_GET_IMPL(runtime, tblId);
            for(int i = 0; i < MOZ_JMPTABLE_SIZE; i++) {
                moz_inst_t *dest = jmpoffset + table_jumps[i];
                if(BBMap.find(dest) == BBMap.end()) {
                    BasicBlock *label = get_jump_destination(e, dest, failBB);
                    assert(label != nullptr);
                    BBMap[dest] = label;
                }
            }
            break;
        }
#ifdef MOZVM_USE_JMPTBL
        case TblJump1: {
            uint16_t tblId = *(uint16_t *)(p + 1);
            table_jumps = runtime->C.jumps1[tblId].jumps;
            table_size  = 2;
            goto L_prepare_table;
        }
        case TblJump2: {
            uint16_t tblId = *(uint16_t *)(p + 1);
            table_jumps = runtime->C.jumps2[tblId].jumps;
            table_size  = 4;
            goto L_prepare_table;
        }
        case TblJump3: {
            {
                uint16_t tblId = *(uint16_t *)(p + 1);
                table_jumps = runtime->C.jumps3[tblId].jumps;
                table_size  = 8;
            }
L_prepare_table:
            for(int i = 0; i < table_size; i++) {
                moz_inst_t *jmpoffset = p + shift;
                if(table_jumps[i] != INT_MAX) {
                    moz_inst_t *dest = jmpoffset + table_jumps[i];
                    if(BBMap.find(dest) == BBMap.end()) {
                        BasicBlock *label = get_jump_destination(e, dest, failBB);
                        assert(label != nullptr);
                        BBMap[dest] = label;
                    }
                }
            }
            break;
        }
#endif /*MOZVM_USE_JMPTBL*/
        }
        p += opcode_size(opcode);
    }

    failjumpList.push_back(errBB);

}

moz_jit_func_t mozvm_jit_compile(moz_runtime_t *runtime, mozvm_nterm_entry_t *e)
{
    JitContext *_ctx = get_context(runtime);
    uint16_t nterm = e - runtime->nterm_entry;

    if(mozvm_nterm_is_already_compiled(e)) {
        return e->compiled_code;
    }

    StackFrame Frame;
    StackFrame_init(&Frame);
    LLVMContext &Ctx = getGlobalContext();
    IRBuilder<> builder(Ctx);
    Module *M = new Module(runtime->C.nterms[nterm], Ctx);
    _ctx->curMod = M;
    _ctx->EE->addModule(unique_ptr<Module>(M));

    Type *memoentryPtrTy = GetType<MemoEntry_t *>();
    Constant *nullentry         = Constant::getNullValue(memoentryPtrTy);
    Constant *memo_entry_failed = builder.getInt64(MEMO_ENTRY_FAILED);

    Constant *f_pstrstwith = REGISTER_FUNC(M, pstring_starts_with);
    Constant *f_bitsetget   = REGISTER_FUNC(M, bitset_get);
    Constant *f_astsave     = REGISTER_FUNC(M, ast_save_tx);
    Constant *f_astrollback = REGISTER_FUNC(M, ast_rollback_tx);
    Constant *f_astcommit   = REGISTER_FUNC(M, ast_commit_tx);
    Constant *f_astreplace  = REGISTER_FUNC(M, ast_log_replace);
    Constant *f_astcapture  = REGISTER_FUNC(M, ast_log_capture);
    Constant *f_astnew      = REGISTER_FUNC(M, ast_log_new);
    Constant *f_astpop      = REGISTER_FUNC(M, ast_log_pop);
    Constant *f_astpush     = REGISTER_FUNC(M, ast_log_push);
    Constant *f_astswap     = REGISTER_FUNC(M, ast_log_swap);
    Constant *f_asttag      = REGISTER_FUNC(M, ast_log_tag);
    Constant *f_astlink     = REGISTER_FUNC(M, ast_log_link);
    Constant *f_astlastnode = REGISTER_FUNC(M, ast_get_last_linked_node);
    Constant *f_memoset     = REGISTER_FUNC(M, memo_set);
    Constant *f_memofail    = REGISTER_FUNC(M, memo_fail);
    Constant *f_memoget     = REGISTER_FUNC(M, memo_get);
    Constant *f_tblsave     = REGISTER_FUNC(M, symtable_savepoint);
    Constant *f_tblrollback = REGISTER_FUNC(M, symtable_rollback);

    FunctionType *FuncTy = GetFuncType(mozvm_jit_call_nterm);
    Function *F = Function::Create(FuncTy,
            Function::ExternalLinkage,
            runtime->C.nterms[nterm], M);

    Function::arg_iterator arg_iter=F->arg_begin();
    Value *runtime_ = arg_iter++;
    Value *str = arg_iter++;
    // Value *nterm_ = arg_iter++;

    vector<BasicBlock *> failjumpList;
    BBMap BBMap;
    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    BasicBlock *failBB  = BasicBlock::Create(Ctx, "fail");
    BasicBlock *retBB   = BasicBlock::Create(Ctx, "success");
    BasicBlock *errBB   = BasicBlock::Create(Ctx, "error");
    BasicBlock *unreachableBB = BasicBlock::Create(Ctx, "switch.default");

    mozvm_jit_compile_init(runtime, e, BBMap, failBB, errBB, failjumpList);

    builder.SetInsertPoint(entryBB);
    Constant *i8_0  = builder.getInt8(0);
    Constant *i32_0 = builder.getInt32(0);
    Constant *i32_1 = builder.getInt32(1);
    Constant *i32_2 = builder.getInt32(2);
    Constant *i64_0 = builder.getInt64(0);

    Value *ast_   = create_gep(builder, runtime_, i64_0, i32_0);
    Value *ast    = builder.CreateLoad(ast_);
    Value *tbl_   = create_gep(builder, runtime_, i64_0, i32_1);
    Value *tbl    = builder.CreateLoad(tbl_);
    Value *memo_  = create_gep(builder, runtime_, i64_0, i32_2);
    Value *memo   = builder.CreateLoad(memo_);
    Value *head_  = create_gep(builder, runtime_, i64_0, builder.getInt32(3));
    Value *tail_  = create_gep(builder, runtime_, i64_0, builder.getInt32(4));
    Value *tail   = builder.CreateLoad(tail_);

    Value *sp = create_gep(builder, runtime_, i64_0, builder.getInt32(6));
    Value *fp = create_gep(builder, runtime_, i64_0, builder.getInt32(7));
    Value *cur = create_gep(builder, runtime_, i64_0,
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
            builder.getInt32(9)
#else
            builder.getInt32(8)
#endif
            );

    {
        BlockAddress *addr = BlockAddress::get(F, errBB);
        Value *pos = builder.CreateLoad(cur);
        Value *ast_tx = create_call(builder, f_astsave, ast);
        Value *saved  = create_call(builder, f_tblsave, tbl);
        stack_push_frame(builder, sp, fp, pos, addr, ast_tx, saved);
    }

    BasicBlock *CurBB = entryBB;
    moz_inst_t *p = e->begin;
    while (p < e->end) {
        uint8_t opcode = *p;
        unsigned shift = opcode_size(opcode);

        if(BBMap.find(p) != BBMap.end()) {
            BasicBlock *newBB = BBMap[p];
            newBB->insertInto(F);
            if(CurBB->getTerminator() == nullptr) {
                builder.CreateBr(newBB);
            }
            builder.SetInsertPoint(newBB);
            CurBB = newBB;
        }
        else if(CurBB->getTerminator() != nullptr) {
            BasicBlock *newBB = BasicBlock::Create(Ctx, "unreachable", F);
            builder.SetInsertPoint(newBB);
            CurBB = newBB;
        }

        switch(opcode) {
#define CASE_(OP) case OP:
        CASE_(Nop) {
            break;
        }
        CASE_(Fail) {
            builder.CreateBr(failBB);
            break;
        }
        CASE_(Succ) {
            stack_drop_frame(builder, sp, fp);
            break;
        }
        CASE_(Alt) {
            mozaddr_t failjump = *(mozaddr_t *)(p + 1);
            moz_inst_t *dest = p + shift + failjump;

            BlockAddress *addr = BlockAddress::get(F, BBMap[dest]);
            Value *pos = builder.CreateLoad(cur);
            Value *ast_tx = create_call(builder, f_astsave, ast);
            Value *saved  = create_call(builder, f_tblsave, tbl);
            stack_push_frame(builder, sp, fp, pos, addr, ast_tx, saved);
            break;
        }
        CASE_(Jump) {
            mozaddr_t jump = *(mozaddr_t *)(p + 1);
            moz_inst_t *dest = p + shift + jump;

            builder.CreateBr(BBMap[dest]);
            break;
        }
        CASE_(Call) {
            uint16_t nterm   = *(uint16_t *)(p + 1);
            mozaddr_t next   = *(mozaddr_t *)(p + 1 + sizeof(uint16_t));
            moz_inst_t *dest = p + shift + next;
            mozvm_nterm_entry_t *target = &runtime->nterm_entry[nterm];

            Constant *ID = builder.getInt16(nterm);
            Value *func;

            // Value *prev_pos_;
            // Value *next_;
            // Value *ast_tx_;
            // Value *saved_;
            //
            // BlockAddress *addr = BlockAddress::get(F, BBMap[dest]);
            // Value *pos = builder.CreateLoad(cur);
            // stack_peek_frame(&Frame, &prev_pos_, &next_, &ast_tx_, &saved_);
            // stack_push_frame(builder, sp, fp, pos, addr, ast_tx_, saved_);
            if(mozvm_nterm_is_already_compiled(target)) {
                const char *nterm_name = runtime->C.nterms[nterm];
                func = M->getOrInsertFunction(nterm_name, FuncTy);
            }
            else {
                func = get_callee_function(builder, runtime_, nterm);
            }
            // stack_pop_frame(builder, sp, fp, &pos, &addr, &ast_tx_, &saved_);
            Value *result = create_call(builder, func, runtime_, str, ID);
            Value *cond   = builder.CreateICmpNE(result, i8_0);
            builder.CreateCondBr(cond, failBB, BBMap[dest]);
            break;
        }
        CASE_(Ret) {
            builder.CreateBr(retBB);
            break;
        }
        CASE_(Pos) {
            Value *pos = builder.CreateLoad(cur);
            stack_push_pos(builder, sp, pos);
            break;
        }
        CASE_(Back) {
            Value *pos = stack_pop_pos(builder, sp);
            builder.CreateStore(pos, cur);
            break;
        }
        CASE_(Skip) {
            BasicBlock *next = BasicBlock::Create(Ctx, "skip.next", F);

            Value *prev_pos_;
            Value *next_;
            Value *ast_tx_;
            Value *saved_;
            stack_peek_frame(builder, sp, fp, &prev_pos_, &next_, &ast_tx_, &saved_);
            Value *pos = builder.CreateLoad(cur);
            Value *prev_pos = builder.CreateLoad(prev_pos_);
            Value *cond = builder.CreateICmpEQ(prev_pos, pos);
            builder.CreateCondBr(cond, failBB, next);

            builder.SetInsertPoint(next);
            builder.CreateStore(pos, prev_pos_);
            Value *ast_tx = create_call(builder, f_astsave, ast);
            builder.CreateStore(ast_tx, ast_tx_);
            Value *saved = create_call(builder, f_tblsave, tbl);
            builder.CreateStore(saved, saved_);
            CurBB = next;
            break;
        }
        CASE_(Byte);
        CASE_(NByte) {
            BasicBlock *succ = BasicBlock::Create(Ctx, "byte.succ", F);
            uint8_t ch = *(uint8_t *)(p + 1);
            Constant *C = builder.getInt8(ch);

            Value *pos = builder.CreateLoad(cur);
            Value *Cur = GetCur(builder, str, pos);
            Value *Char = builder.CreateLoad(Cur);
            Value *cond;
            if (opcode == Byte) {
                cond = builder.CreateICmpNE(Char, C);
            }
            else {
                cond = builder.CreateICmpEQ(Char, C);
            }
            builder.CreateCondBr(cond, failBB, succ);

            builder.SetInsertPoint(succ);
            if (opcode == Byte) {
                Value *nextpos = consume(builder, pos);
                builder.CreateStore(nextpos, cur);
            }
            CurBB = succ;
            break;
        }
        CASE_(OByte) {
            BasicBlock *obody = BasicBlock::Create(Ctx, "obyte.body", F);
            BasicBlock *oend  = BasicBlock::Create(Ctx, "obyte.end", F);
            uint8_t ch = (uint8_t)*(p + 1);
            Constant *C = builder.getInt8(ch);

            Value *pos = builder.CreateLoad(cur);
            Value *Cur = GetCur(builder, str, pos);
            Value *Char = builder.CreateLoad(Cur);
            Value *cond = builder.CreateICmpEQ(Char, C);
            builder.CreateCondBr(cond, obody, oend);

            builder.SetInsertPoint(obody);
            Value *nextpos = consume(builder, pos);
            builder.CreateStore(nextpos, cur);
            builder.CreateBr(oend);

            builder.SetInsertPoint(oend);
            CurBB = oend;
            break;
        }
        CASE_(RByte) {
            asm volatile("int3");
            break;
        }
        CASE_(Any);
        CASE_(NAny) {
            BasicBlock *succ = BasicBlock::Create(Ctx, "any.succ", F);

            Value *pos = builder.CreateLoad(cur);
            Value *Cur = GetCur(builder, str, pos);
            Value *cond;
            if (opcode == Any) {
                cond = builder.CreateICmpEQ(Cur, tail);
            } else {
                cond = builder.CreateICmpNE(Cur, tail);
            }
            builder.CreateCondBr(cond, failBB, succ);

            builder.SetInsertPoint(succ);
            if (opcode == Any) {
                Value *nextpos = consume(builder, pos);
                builder.CreateStore(nextpos, cur);
            }
            CurBB = succ;
            break;
        }
        CASE_(OAny) {
            asm volatile("int3");
            break;
        }
        CASE_(RAny) {
            asm volatile("int3");
            break;
        }
        CASE_(Str);
        CASE_(NStr) {
            STRING_t   strId = *(STRING_t *)(p + 1);
            const char *impl = STRING_GET_IMPL(runtime, strId);
            BasicBlock *succ = BasicBlock::Create(Ctx, "str.succ", F);

            Value *str = get_string_ptr(builder, runtime_, strId);
            Value *len = builder.getInt32(pstring_length(impl));
            Value *pos = builder.CreateLoad(cur);
            Value *Cur = GetCur(builder, str, pos);
            Value *result = create_call(builder, f_pstrstwith, Cur, str, len);
            Value *C = opcode == Str ? i32_0 : i32_1;
            Value *cond = builder.CreateICmpEQ(result, C);
            builder.CreateCondBr(cond, failBB, succ);

            builder.SetInsertPoint(succ);
            if (opcode == Str) {
                Value *len_ = builder.CreateZExt(len, builder.getInt64Ty());
                Value *nextpos = consume_n(builder, pos, len_);
                builder.CreateStore(nextpos, cur);
            }
            CurBB = succ;
            break;
        }
        CASE_(OStr) {
            asm volatile("int3");
            break;
        }
        CASE_(RStr) {
            asm volatile("int3");
            break;
        }
        CASE_(Set);
        CASE_(NSet) {
            BasicBlock *succ = BasicBlock::Create(Ctx, "set.succ", F);
            BITSET_t setId = *(BITSET_t *)(p + 1);

            Value *set = get_bitset_ptr(builder, runtime_, setId);
            Value *pos = builder.CreateLoad(cur);
            Value *Cur = GetCur(builder, str, pos);
            Value *Char = builder.CreateLoad(Cur);
            Value *index = builder.CreateZExt(Char, builder.getInt32Ty());
            Value *result = create_call(builder, f_bitsetget, set, index);
            Value *cond = NULL;
            if (opcode == Set) {
                cond = builder.CreateICmpEQ(result, i32_0);
            }
            else {
                cond = builder.CreateICmpNE(result, i32_0);
            }
            builder.CreateCondBr(cond, failBB, succ);

            builder.SetInsertPoint(succ);
            if (opcode == Set) {
                Value *nextpos = consume(builder, pos);
                builder.CreateStore(nextpos, cur);
            }
            CurBB = succ;
            break;
        }
        CASE_(OSet) {
            BasicBlock *obody = BasicBlock::Create(Ctx, "oset.body", F);
            BasicBlock *oend  = BasicBlock::Create(Ctx, "oset.end", F);
            BITSET_t setId = *(BITSET_t *)(p + 1);

            Value *set = get_bitset_ptr(builder, runtime_, setId);
            Value *pos = builder.CreateLoad(cur);
            Value *Cur = GetCur(builder, str, pos);
            Value *Char = builder.CreateLoad(Cur);
            Value *index = builder.CreateZExt(Char, builder.getInt32Ty());
            Value *result = create_call(builder, f_bitsetget, set, index);
            Value *cond = builder.CreateICmpNE(result, i32_0);
            builder.CreateCondBr(cond, obody, oend);

            builder.SetInsertPoint(obody);
            Value *nextpos = consume(builder, pos);
            builder.CreateStore(nextpos, cur);
            builder.CreateBr(oend);

            builder.SetInsertPoint(oend);
            CurBB = oend;
            break;
        }
        CASE_(RSet) {
            BasicBlock *rcond = BasicBlock::Create(Ctx, "rset.cond", F);
            BasicBlock *rbody = BasicBlock::Create(Ctx, "rset.body", F);
            BasicBlock *rend  = BasicBlock::Create(Ctx, "rset.end",  F);

            BITSET_t setId = *(BITSET_t *)(p + 1);
            Value *set = get_bitset_ptr(builder, runtime_, setId);
            Value *firstpos = builder.CreateLoad(cur);
            builder.CreateBr(rcond);

            builder.SetInsertPoint(rcond);
            PHINode *pos = builder.CreatePHI(GetType<mozpos_t>(), 2);
            pos->addIncoming(firstpos, CurBB);
            Value *Cur = GetCur(builder, str, pos);
            Value *Char = builder.CreateLoad(Cur);
            Value *index = builder.CreateZExt(Char, builder.getInt32Ty());
            Value *result = create_call(builder, f_bitsetget, set, index);
            Value *cond = builder.CreateICmpNE(result, i32_0);
            builder.CreateCondBr(cond, rbody, rend);

            builder.SetInsertPoint(rbody);
            Value *nextpos = consume(builder, pos);
            pos->addIncoming(nextpos, rbody);
            builder.CreateBr(rcond);

            builder.SetInsertPoint(rend);
            builder.CreateStore(pos, cur);
            CurBB = rend;
            break;
        }
        CASE_(Consume) {
            asm volatile("int3");
            break;
        }
        CASE_(First) {
            JMPTBL_t tblId = *(JMPTBL_t *)(p + 1);
            moz_inst_t *offset = p + shift;
            int *jmpTable = JMPTBL_GET_IMPL(runtime, tblId);

            Value *pos = builder.CreateLoad(cur);
            Value *Cur = GetCur(builder, str, pos);
            Value *Char = builder.CreateLoad(Cur);
            Value *index = builder.CreateZExt(Char, builder.getInt64Ty());
            SwitchInst *jump = builder.CreateSwitch(index, unreachableBB, MOZ_JMPTABLE_SIZE);
            for(int i = 0; i < MOZ_JMPTABLE_SIZE; i++) {
                moz_inst_t *dest = offset + jmpTable[i];
                jump->addCase(builder.getInt64(i), BBMap[dest]);
            }
            break;
        }
        CASE_(TblJump1) {
            uint16_t tblId = *(uint16_t *)(p + 1);
            moz_inst_t *offset = p + shift;
            int *jumps = runtime->C.jumps1[tblId].jumps;
            Value *tbl = get_jump_table<1>(builder, runtime_, tblId);
            create_call_tbljmp<1>(builder, tbl, _ctx, cur, str,
                    unreachableBB, BBMap, offset, jumps);
            break;
        }
        CASE_(TblJump2) {
            uint16_t tblId = *(uint16_t *)(p + 1);
            moz_inst_t *offset = p + shift;
            int *jumps = runtime->C.jumps2[tblId].jumps;
            Value *tbl = get_jump_table<2>(builder, runtime_, tblId);
            create_call_tbljmp<2>(builder, tbl, _ctx, cur, str,
                    unreachableBB, BBMap, offset, jumps);
            break;
        }
        CASE_(TblJump3) {
            uint16_t tblId = *(uint16_t *)(p + 1);
            moz_inst_t *offset = p + shift;
            int *jumps = runtime->C.jumps3[tblId].jumps;
            Value *tbl = get_jump_table<3>(builder, runtime_, tblId);
            create_call_tbljmp<3>(builder, tbl, _ctx, cur, str,
                    unreachableBB, BBMap, offset, jumps);
            break;
        }
        CASE_(Lookup) {
            BasicBlock *hit  = BasicBlock::Create(Ctx, "lookup.hit",  F);
            BasicBlock *succ = BasicBlock::Create(Ctx, "lookup.succ",  F);
            BasicBlock *miss = BasicBlock::Create(Ctx, "lookup.miss", F);

            uint8_t state    = *(uint8_t   *)(p + 1);
            uint16_t memoId  = *(uint16_t  *)(p + 2);
            mozaddr_t skip   = *(mozaddr_t *)(p + 2 + sizeof(uint16_t));
            moz_inst_t *dest = p + shift + skip;
            Constant *state_  = builder.getInt8(state);
            Constant *memoId_ = builder.getInt32(memoId);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
#error not implemented
#endif
            Value *pos     = builder.CreateLoad(cur);
            Value *entry   = create_call(builder, f_memoget, memo, pos, memoId_, state_);
            Value *hitcond = builder.CreateICmpNE(entry, nullentry);
            builder.CreateCondBr(hitcond, hit, miss);

            builder.SetInsertPoint(hit);
            Value *result_  = create_gep(builder, entry, i64_0, i32_1);
            Value *result   = builder.CreateLoad(result_);
            Value *failed   = builder.CreatePtrToInt(result, builder.getInt64Ty());
            Value *failcond = builder.CreateICmpEQ(failed, memo_entry_failed);
            builder.CreateCondBr(failcond, failBB, succ);

            builder.SetInsertPoint(succ);
            Value *consumed_ = create_gep(builder, entry, i64_0, i32_2);
            Value *consumed  = builder.CreateLoad(consumed_);
            Value *newpos    = consume_n(builder, pos, consumed);
            builder.CreateStore(newpos, cur);
            builder.CreateBr(BBMap[dest]);

            builder.SetInsertPoint(miss);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
#endif
            CurBB = miss;
            break;
        }
        CASE_(Memo) {
            uint8_t  state  = *(uint8_t  *)(p + 1);
            uint16_t memoId = *(uint16_t *)(p + 2);
            Constant *state_  = builder.getInt32(state);
            Constant *memoId_ = builder.getInt32(memoId);

            Value *startpos;
            Value *next;
            Value *ast_tx;
            Value *saved;
            Value *length;
            stack_pop_frame(builder, sp, fp, &startpos, &next, &ast_tx, &saved);
            Value *endpos  = builder.CreateLoad(cur);
            length = get_length(builder, startpos, endpos);
            length = builder.CreateTrunc(length, builder.getInt32Ty());

            Type *nodePtrTy   = GetType<Node *>();
            Constant *nullnode = Constant::getNullValue(nodePtrTy);
            create_call(builder, f_memoset,
                    memo, startpos, memoId_, nullnode, length, state_);
            break;
        }
        CASE_(MemoFail) {
            // uint8_t state   = *(uint8_t   *)(p + 1);
            uint16_t memoId = *(uint16_t *)(p + 2);
            Constant *memoId_ = builder.getInt32(memoId);

            Value *pos = builder.CreateLoad(cur);
            create_call(builder, f_memofail, memo, pos, memoId_);
            builder.CreateBr(failBB);
            break;
        }
        CASE_(TPush) {
            create_call(builder, f_astpush, ast);
            break;
        }
        CASE_(TPop) {
            TAG_t tagId = *(TAG_t *)(p + 1);

            Value *_tagId = get_tag_id(builder, runtime_, tagId);
            create_call(builder, f_astpop, ast, _tagId);
            break;
        }
        CASE_(TLeftFold) {
            int8_t shift    = *(int8_t *)(p + 1);
            TAG_t tagId      = *(TAG_t   *)(p + 2);
            Constant *shift_ = builder.getInt64(shift);

            Value *_tagId  = get_tag_id(builder, runtime_, tagId);
            Value *pos     = builder.CreateLoad(cur);
            Value *swappos = consume_n(builder, pos, shift_);
            create_call(builder, f_astswap, ast, swappos, _tagId);
            break;
        }
        CASE_(TNew) {
            int8_t shift    = *(int8_t *)(p + 1);
            Constant *shift_ = builder.getInt64(shift);

            Value *pos    = builder.CreateLoad(cur);
            Value *newpos = consume_n(builder, pos, shift_);
            create_call(builder, f_astnew, ast, newpos);
            break;
        }
        CASE_(TCapture) {
            int8_t shift    = *(int8_t *)(p + 1);
            Constant *shift_ = builder.getInt64(shift);

            Value *pos        = builder.CreateLoad(cur);
            Value *capturepos = consume_n(builder, pos, shift_);
            create_call(builder, f_astcapture, ast, capturepos);
            break;
        }
        CASE_(TTag) {
            TAG_t tagId = *(TAG_t *)(p + 1);

            Value *tag = get_tag_ptr(builder, _ctx, runtime_, tagId);
            create_call(builder, f_asttag, ast, tag);
            break;
        }
        CASE_(TReplace) {
            STRING_t strId = *(STRING_t *)(p + 1);

            Value *str = get_string_ptr(builder, runtime_, strId);
            create_call(builder, f_astreplace, ast, str);
            break;
        }
        CASE_(TStart) {
            Value *ast_tx = create_call(builder, f_astsave, ast);
            stack_push(builder, sp, ast_tx);
            break;
        }
        CASE_(TCommit) {
            TAG_t tagId = *(TAG_t *)(p + 1);

            Value *_tagId = get_tag_id(builder, runtime_, tagId);
            Value *tx     = stack_pop(builder, sp);
            create_call(builder, f_astcommit, ast, _tagId, tx);
            break;
        }
        CASE_(TAbort) {
            asm volatile("int3");
            break;
        }
        CASE_(TLookup) {
            BasicBlock *hit  = BasicBlock::Create(Ctx, "lookup.hit",  F);
            BasicBlock *succ = BasicBlock::Create(Ctx, "lookup.succ",  F);
            BasicBlock *miss = BasicBlock::Create(Ctx, "lookup.miss", F);

            uint8_t state = *(uint8_t *)(p + 1);
            TAG_t   tagId = *(TAG_t   *)(p + 2);
            uint16_t  memoId = *(uint16_t  *)(p + 2 + sizeof(TAG_t));
            mozaddr_t skip   = *(mozaddr_t *)(p + 4 + sizeof(TAG_t));
            moz_inst_t *dest = p + shift + skip;
            Constant *state_  = builder.getInt8(state);
            Constant *memoId_ = builder.getInt32(memoId);

            Value *_tagId = get_tag_id(builder, runtime_, tagId);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
            asm volatile("int3");
#endif
            Value *pos   = builder.CreateLoad(cur);
            Value *entry = create_call(builder, f_memoget, memo, pos, memoId_, state_);
            Value *hitcond = builder.CreateICmpNE(entry, nullentry);
            builder.CreateCondBr(hitcond, hit, miss);

            builder.SetInsertPoint(hit);
            Value *result_  = create_gep(builder, entry, i64_0, i32_1);
            Value *result   = builder.CreateLoad(result_);
            Value *failed   = builder.CreatePtrToInt(result, builder.getInt64Ty());
            Value *failcond = builder.CreateICmpEQ(failed, memo_entry_failed);
            builder.CreateCondBr(failcond, failBB, succ);

            builder.SetInsertPoint(succ);
            Value *consumed_ = create_gep(builder, entry, i64_0, i32_2);
            Value *consumed  = builder.CreateLoad(consumed_);
            Value *newpos    = consume_n(builder, pos, consumed);
            builder.CreateStore(newpos, cur);
            create_call(builder, f_astlink, ast, _tagId, result);
            builder.CreateBr(BBMap[dest]);

            builder.SetInsertPoint(miss);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
#endif
            CurBB = miss;
            break;
        }
        CASE_(TMemo) {
            uint8_t state    = *(uint8_t *)(p + 1);
            uint16_t memoId  = *(uint16_t *)(p + 2);
            Constant *state_  = builder.getInt32(state);
            Constant *memoId_ = builder.getInt32(memoId);

            Value *startpos;
            Value *next;
            Value *ast_tx;
            Value *saved;
            stack_pop_frame(builder, sp, fp, &startpos, &next, &ast_tx, &saved);
            Value *endpos  = builder.CreateLoad(cur);
            Value *length  = get_length(builder, startpos, endpos);
            Value *length_ = builder.CreateTrunc(length, builder.getInt32Ty());
            Value *node    = create_call(builder, f_astlastnode, ast);
            create_call(builder, f_memoset,
                    memo, startpos, memoId_, node, length_, state_);
            break;
        }
        CASE_(SOpen) {
            asm volatile("int3");
        }
        CASE_(SClose) {
            asm volatile("int3");
        }
        CASE_(SMask) {
            asm volatile("int3");
        }
        CASE_(SDef) {
            asm volatile("int3");
        }
        CASE_(SIsDef) {
            asm volatile("int3");
        }
        CASE_(SExists) {
            asm volatile("int3");
        }
        CASE_(SMatch) {
            asm volatile("int3");
        }
        CASE_(SIs) {
            asm volatile("int3");
        }
        CASE_(SIsa) {
            asm volatile("int3");
        }
        CASE_(SDefNum) {
            asm volatile("int3");
        }
        CASE_(SCount) {
            asm volatile("int3");
        }
        CASE_(Exit) {
            asm volatile("int3");
            break;
        }
        CASE_(Label) {
            asm volatile("int3");
            break;
        }
#undef CASE_
        }
        p += shift;
    }

    failBB->insertInto(F);
    builder.SetInsertPoint(failBB);
    {
        BasicBlock *posback  = BasicBlock::Create(Ctx, "fail.posback",  F);
        BasicBlock *rollback = BasicBlock::Create(Ctx, "fail.rollback", F);

        Value *pos = builder.CreateLoad(cur);
        Value *pos_;
        Value *next_;
        Value *ast_tx_;
        Value *saved_;
        stack_pop_frame(builder, sp, fp, &pos_, &next_, &ast_tx_, &saved_);
        Value *cond = builder.CreateICmpULT(pos_, pos);
        builder.CreateCondBr(cond, posback, rollback);

        builder.SetInsertPoint(posback);
        Value *head = builder.CreateLoad(head_);
        Value *selectcond = builder.CreateICmpULT(head, pos);
        Value *newhead = builder.CreateSelect(selectcond, pos, head);
        builder.CreateStore(newhead, head_);
        builder.CreateStore(pos_, cur);
        builder.CreateBr(rollback);

        builder.SetInsertPoint(rollback);
        create_call(builder, f_astrollback, ast, ast_tx_);
        create_call(builder, f_tblrollback, tbl, saved_);
        auto indirect_br = builder.CreateIndirectBr(next_, failjumpList.size());
        for(BasicBlock *BB : failjumpList) {
            indirect_br->addDestination(BB);
        }
    }

    retBB->insertInto(F);
    builder.SetInsertPoint(retBB);
    {
        stack_drop_frame(builder, sp, fp);
        builder.CreateRet(i8_0);
    }

    errBB->insertInto(F);
    builder.SetInsertPoint(errBB);
    builder.CreateRet(builder.getInt8(1));

    unreachableBB->insertInto(F);
    builder.SetInsertPoint(unreachableBB);
    builder.CreateUnreachable();

    StackFrame_dispose(&Frame);

    if (optimize(M, F)) {
        return NULL;
    }
    _ctx->EE->finalizeObject();
    e->compiled_code = (moz_jit_func_t) _ctx->EE->getPointerToFunction(F);
    return e->compiled_code;
}

#ifdef __cplusplus
}
#endif
#endif /*MOZVM_ENABLE_JIT*/
