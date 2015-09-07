#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"

#include "jit.h"
#include "instruction.h"

using namespace llvm;

template<class Vector, class First>
void set_vector(const Vector& dest, const First& first)
{
    dest->push_back(first);
}

template<class Vector, class First, class... Rest>
void set_vector(const Vector& dest, const First& first, const Rest&... rest)
{
    set_vector(dest, first);
    set_vector(dest, rest...);
}

template<class... Args>
StructType *jit_define_struct_type(const char *name, const Args&... args)
{
    std::vector<Type *> elements;
    set_vector(&elements, args...);
    ArrayRef<Type *> elmsRef(elements);

    LLVMContext& context = getGlobalContext();
    StructType *s_ty = StructType::create(context, name);
    s_ty->setBody(elmsRef);
    return s_ty;
}

template<class Builder, class... Args>
Value *jit_create_get_element_ptr(const Builder &builder, Value *Val, const Args&... args)
{
    std::vector<Value *> indexs;
    set_vector(&indexs, args...);
    ArrayRef<Value *> idxsRef(indexs);

    return builder->CreateGEP(Val, idxsRef);
}

template<class Returns, class... Args>
FunctionType *jit_get_function_type(const Returns& returntype, const Args&... args)
{
    std::vector<Type *> parameters;
    set_vector(&parameters, args...);
    ArrayRef<Type *> paramsRef(parameters);

    return FunctionType::get(returntype, paramsRef, false);
}

template<class Builder, class... Args>
Value *jit_create_call_inst(const Builder &builder, Value *F, const Args&... args)
{
    std::vector<Value *> arguments;
    set_vector(&arguments, args...);
    ArrayRef<Value *> argsRef(arguments);

    return builder->CreateCall(F, argsRef);
}

Value *jit_get_current(IRBuilder<> &builder, Value *str, Value *pos)
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    return pos;
#else
    return builder.CreateGEP(str, pos);
#endif
}

Value *jit_consume_n(IRBuilder<> &builder, Value *pos, Value *N)
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    return builder.CreateGEP(pos, N);
#else
    return builder.CreateAdd(pos, N);
#endif
}

Value *jit_bitset_ptr(IRBuilder<> *builder, Value *runtime, BITSET_t id)
{
#if MOZVM_SMALL_BITSET_INST
    Value *r_c_sets = jit_create_get_element_ptr(builder, runtime,
            builder->getInt64(0), builder->getInt32(10), builder->getInt32(0));
    Value *sets_head = builder->CreateLoad(r_c_sets);
    return jit_create_get_element_ptr(builder, sets_head, builder->getInt32(id));
#else
    asm volatile("int3");
    // bitset_t *_set = (bitset_t *)id;
    return NULL;
#endif
}

#ifdef __cplusplus
extern "C" {
#endif

#define MOZVM_OPCODE_SIZE 1
#include "vm_inst.h"

class JitContext {
public:
    ExecutionEngine *EE;
    StructType *bsetType;
    StructType *astType;
    StructType *symtableType;
    StructType *memoType;
    Type *mozposType;
    StructType *runtimeType;
    FunctionType *funcType;

    FunctionType *bitsetgetType;

    ~JitContext() {};
    JitContext();

    FunctionType *Create_bitset_get(IRBuilder<> &builder, Module *M);
};

JitContext::JitContext()
{
    LLVMContext& context = getGlobalContext();
    IRBuilder<> builder(context);

    Type *I8PtrTy = Type::getInt8PtrTy(context);
    Type *I8PtrPtrTy = I8PtrTy->getPointerTo();
    Type *I16Ty   = Type::getInt16Ty(context);
    Type *I32Ty   = Type::getInt32Ty(context);
    Type *I64Ty   = Type::getInt64Ty(context);
    Type *I64PtrTy   = Type::getInt64PtrTy(context);

#if defined(BITSET_USE_ULONG)
    bsetType = jit_define_struct_type("bitset_t", ArrayType::get(I64Ty, (256/BITS)));
#elif defined(BITSET_USE_UINT)
    bsetType = jit_define_struct_type("bitset_t", ArrayType::get(I32Ty, (256/BITS)));
#endif

    StructType *constantType = jit_define_struct_type("mozvm_constant_t",
            bsetType->getPointerTo(),
            I8PtrPtrTy, I8PtrPtrTy, I8PtrPtrTy, I32Ty, // tags, strs, tables, jumps
#ifdef MOZVM_USE_JMPTBL
            I8PtrTy, I8PtrTy, I8PtrTy, // jumps1, jumps2, jumps3
#endif
            I8PtrPtrTy, // nterms
            I16Ty, I16Ty, I16Ty, // set_size, str_size, tag_size
            I16Ty, I16Ty, // table_size, nterm_size
            I32Ty, I32Ty, I32Ty // inst_size, memo_size, input_size
#ifdef MOZVM_PROFILE_INST
            , I64PtrTy // profile
#endif
            );

    astType = StructType::create(context, "AstMachine");
    symtableType = StructType::create(context, "symtable_t");
    memoType = StructType::create(context, "memo_t");
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    mozposType = I8PtrTy;
#else
    mozposType = I64Ty;
#endif
    runtimeType = jit_define_struct_type("moz_runtime_t",
            astType->getPointerTo(), symtableType->getPointerTo(),
            memoType->getPointerTo(), mozposType, // memo, head
            I8PtrTy, I8PtrTy, // tail, input
            I64PtrTy, I64PtrTy, // stack, fp
            I8PtrTy, I8PtrTy, // nterm_entry, jit_context
            constantType, ArrayType::get(I64Ty, 1)
            );

    funcType = jit_get_function_type(Type::getInt8Ty(context),
            runtimeType->getPointerTo(), I8PtrTy, mozposType->getPointerTo());

    Module *M = new Module("top", context);
    bitsetgetType = Create_bitset_get(builder, M);
    EE = EngineBuilder(std::unique_ptr<Module>(M)).create();
    EE->finalizeObject();
}

FunctionType *JitContext::Create_bitset_get(IRBuilder<> &builder, Module *M)
{
    LLVMContext& context = M->getContext();
    Type *I32Ty = builder.getInt32Ty();
    Type *I64Ty = builder.getInt64Ty();
    FunctionType *funcTy = jit_get_function_type(I32Ty,
            bsetType->getPointerTo(), I32Ty);
    Function *F = Function::Create(funcTy,
            Function::ExternalLinkage,
            "bitset_get", M);

    Function::arg_iterator arg_iter=F->arg_begin();
    Value *set = arg_iter++;
    Value *idx = arg_iter++;

    BasicBlock *entry = BasicBlock::Create(context, "entrypoint", F);
    builder.SetInsertPoint(entry);

    Value *i32_0 = builder.getInt32(0);
    Value *i64_0 = builder.getInt64(0);

#if defined(BITSET_USE_ULONG)
    Value *Mod    = builder.CreateAnd(idx, builder.getInt32(63));
    Value *Mod_   = builder.CreateZExt(Mod, I64Ty);
    Value *Mask   = builder.CreateShl(builder.getInt64(1), Mod_);
    Value *Div    = builder.CreateLShr(idx, builder.getInt32(6));
    Value *Div_   = builder.CreateZExt(Div, I64Ty);
    Value *Ptr    = jit_create_get_element_ptr(&builder, set, i64_0, i32_0, Div_);
    Value *Data   = builder.CreateLoad(Ptr);
    Value *Result = builder.CreateAnd(Data, Mask);
    Value *NEq0   = builder.CreateICmpNE(Result, i64_0);
#elif defined(BITSET_USE_UINT)
    Value *Mod    = builder.CreateAnd(idx, builder.getInt32(31));
    Value *Mask   = builder.CreateShl(builder.getInt32(1), Mod);
    Value *Div    = builder.CreateLShr(idx, builder.getInt32(5));
    Value *Div_   = builder.CreateZExt(Div, I64Ty);
    Value *Ptr    = jit_create_get_element_ptr(&builder, set, i64_0, i32_0, Div_);
    Value *Data   = builder.CreateLoad(Ptr);
    Value *Result = builder.CreateAnd(Data, Mask);
    Value *NEq0   = builder.CreateICmpNE(Result, i32_0);
#endif
    Value *RetVal = builder.CreateZExt(NEq0, I32Ty);
    builder.CreateRet(RetVal);
    return funcTy;
}

static inline JitContext *get_context(moz_runtime_t *r)
{
    return reinterpret_cast<JitContext *>(r->jit_context);
}

void mozvm_jit_init(moz_runtime_t *runtime)
{
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    runtime->jit_context = reinterpret_cast<void *>(new JitContext());
}

void mozvm_jit_reset(moz_runtime_t *runtime)
{
    delete get_context(runtime);
    runtime->jit_context = reinterpret_cast<void *>(new JitContext());
}

void mozvm_jit_dispose(moz_runtime_t *runtime)
{
    delete get_context(runtime);
    runtime->jit_context = NULL;
}

moz_jit_func_t mozvm_jit_compile(moz_runtime_t *runtime, mozvm_nterm_entry_t *e)
{
    JitContext *_ctx = get_context(runtime);
    uint16_t nterm = e - runtime->nterm_entry;

    if(e->compiled_code) {
        return e->compiled_code;
    }

    LLVMContext& context = getGlobalContext();
    IRBuilder<> builder(context);
    Module *M = new Module("top", context);
    _ctx->EE->addModule(std::unique_ptr<Module>(M));

    Function *F = Function::Create(_ctx->funcType,
            Function::ExternalLinkage,
            runtime->C.nterms[nterm], M);

    Function::arg_iterator arg_iter=F->arg_begin();
    Value *runtime_ = arg_iter++;
    Value *str = arg_iter++;
    Value *consumed = arg_iter++;

    BasicBlock *entry = BasicBlock::Create(context, "entrypoint", F);
    BasicBlock *currentBB = entry;
    builder.SetInsertPoint(currentBB);

    moz_inst_t *p = e->begin;
    while (p < e->end) {
        uint8_t opcode = *p;
        unsigned shift = opcode_size(opcode);
        switch(opcode) {
#define CASE_(OP) case OP:
            CASE_(Nop) {
                asm volatile("int3");
                break;
            }
            CASE_(Fail) {
                asm volatile("int3");
                break;
            }
            CASE_(Succ) {
                asm volatile("int3");
                break;
            }
            CASE_(Alt) {
                asm volatile("int3");
                break;
            }
            CASE_(Jump) {
                asm volatile("int3");
                break;
            }
            CASE_(Call) {
                asm volatile("int3");
                break;
            }
            CASE_(Ret) {
                builder.CreateRet(builder.getInt8(0));
                break;
            }
            CASE_(Pos) {
                asm volatile("int3");
                break;
            }
            CASE_(Back) {
                asm volatile("int3");
                break;
            }
            CASE_(Skip) {
                asm volatile("int3");
                break;
            }
            CASE_(Byte) {
                asm volatile("int3");
                break;
            }
            CASE_(NByte) {
                asm volatile("int3");
                break;
            }
            CASE_(OByte) {
                asm volatile("int3");
                break;
            }
            CASE_(RByte) {
                asm volatile("int3");
                break;
            }
            CASE_(Any) {
                asm volatile("int3");
                break;
            }
            CASE_(NAny) {
                asm volatile("int3");
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
            CASE_(Str) {
                asm volatile("int3");
                break;
            }
            CASE_(NStr) {
                asm volatile("int3");
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
            CASE_(Set) {
                asm volatile("int3");
                break;
            }
            CASE_(NSet) {
                asm volatile("int3");
                break;
            }
            CASE_(OSet) {
                asm volatile("int3");
                break;
            }
            CASE_(RSet) {
                BasicBlock *rcond = BasicBlock::Create(
                        context, "repetition.cond", F);
                BasicBlock *rbody = BasicBlock::Create(
                        context, "repitition.body", F);
                BasicBlock *rend  = BasicBlock::Create(
                        context, "repitition.end",  F);

                Value *set = jit_bitset_ptr(&builder, runtime_, *((BITSET_t *)(p+1)));
                Value *firstpos = builder.CreateLoad(consumed);
                builder.CreateBr(rcond);

                builder.SetInsertPoint(rcond);
                PHINode *pos = builder.CreatePHI(_ctx->mozposType, 2);
                pos->addIncoming(firstpos, currentBB);
                Value *current = jit_get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *index = builder.CreateZExt(character, builder.getInt32Ty());
                Value *bitsetget = M->getOrInsertFunction(
                        StringRef("bitset_get"), _ctx->bitsetgetType);
                Value *result = jit_create_call_inst(&builder, bitsetget, set, index);
                Value *cond = builder.CreateICmpNE(result, builder.getInt32(0));
                builder.CreateCondBr(cond, rbody, rend);

                builder.SetInsertPoint(rbody);
                Value *nextpos = jit_consume_n(builder, pos, builder.getInt32(1));
                pos->addIncoming(nextpos, rbody);
                builder.CreateBr(rcond);

                builder.SetInsertPoint(rend);
                builder.CreateStore(pos, consumed);
                currentBB = rend;
                break;
            }
            CASE_(Consume) {
                asm volatile("int3");
                break;
            }
            CASE_(First) {
                asm volatile("int3");
                break;
            }
            CASE_(TblJump1) {
                asm volatile("int3");
                break;
            }
            CASE_(TblJump2) {
                asm volatile("int3");
                break;
            }
            CASE_(TblJump3) {
                asm volatile("int3");
                break;
            }
            CASE_(Lookup) {
                asm volatile("int3");
                break;
            }
            CASE_(Memo) {
                asm volatile("int3");
                break;
            }
            CASE_(MemoFail) {
                asm volatile("int3");
                break;
            }
            CASE_(TPush) {
                asm volatile("int3");
                break;
            }
            CASE_(TPop) {
                asm volatile("int3");
                break;
            }
            CASE_(TLeftFold) {
                asm volatile("int3");
                break;
            }
            CASE_(TNew) {
                asm volatile("int3");
                break;
            }
            CASE_(TCapture) {
                asm volatile("int3");
                break;
            }
            CASE_(TTag) {
                asm volatile("int3");
                break;
            }
            CASE_(TReplace) {
                asm volatile("int3");
                break;
            }
            CASE_(TStart) {
                asm volatile("int3");
                break;
            }
            CASE_(TCommit) {
                asm volatile("int3");
                break;
            }
            CASE_(TAbort) {
                asm volatile("int3");
                break;
            }
            CASE_(TLookup) {
                asm volatile("int3");
                break;
            }
            CASE_(TMemo) {
                asm volatile("int3");
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

    // builder.CreateRet(builder.getInt8(0));
    M->dump();
    if(verifyFunction(*F)) {
        F->eraseFromParent();
        return NULL;
    }
    _ctx->EE->finalizeObject();
    e->compiled_code = (moz_jit_func_t) _ctx->EE->getPointerToFunction(F);
    return e->compiled_code;
}

#ifdef __cplusplus
}
#endif
