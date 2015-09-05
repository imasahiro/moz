#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"

#include "jit.h"
#include "api.h"
#include "instruction.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOZVM_OPCODE_SIZE 1
#include "vm_inst.h"

using namespace llvm;

class JitContext {
public:
    ExecutionEngine *EE;
    StructType *runtimeType;
    FunctionType *funcType;
    ~JitContext() {};
    JitContext();
};

JitContext::JitContext() {
    Type *argTypes[3];
    LLVMContext& context = getGlobalContext();
    SMDiagnostic err;
    std::unique_ptr<llvm::Module> apiModule = parseIR(MemoryBufferRef(StringRef(API_IR), StringRef("api.h")), err, context);
    runtimeType = apiModule.get()->getTypeByName(StringRef("struct.moz_runtime_t"));

    EE = EngineBuilder(std::move(apiModule)).create();

    argTypes[0] = runtimeType->getPointerTo();
    argTypes[1] = Type::getInt8PtrTy(context);
    argTypes[2] = Type::getInt32PtrTy(context);

    ArrayRef<Type *> argsRef(argTypes, 3);
    funcType = FunctionType::get(Type::getInt8Ty(context), argsRef, false);
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

#define DECLARE_FUNC(MODULE, DEST, RETURN_TYPE, FUNCNAME, ...) do {\
    Type *args_[] = { __VA_ARGS__ };\
    ArrayRef<Type *> argsRef_(args_, sizeof(args_) / sizeof(args_[0]));\
    FunctionType *funcType_ = FunctionType::get(RETURN_TYPE, argsRef_, false);\
    DEST = (MODULE)->getOrInsertFunction(StringRef(FUNCNAME), funcType_);\
} while(0)

#define CALL_FUNC(BUILDER, RESULT, FUNC, ...) do {\
    Value *params_[] = { __VA_ARGS__ };\
    ArrayRef<Value *> paramsRef_(params_, sizeof(params_) / sizeof(params_[0]));\
    RESULT = (BUILDER).CreateCall(FUNC, paramsRef_);\
} while(0)

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
    Value *len = arg_iter++;

    BasicBlock *entry = BasicBlock::Create(context, "entrypoint", F);
    builder.SetInsertPoint(entry);

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
                Type *bitsetRefType = M->getTypeByName(StringRef("struct.bitset_t"))->getPointerTo();
                Value *bitsetgetimpl;
                DECLARE_FUNC(M, bitsetgetimpl, bitsetRefType, "bitset_Get_Impl", _ctx->runtimeType->getPointerTo(), builder.getInt16Ty());
                Value *set;
                CALL_FUNC(builder, set, bitsetgetimpl, runtime_, builder.getInt16(*((uint16_t *)p)));
                BasicBlock *rstart = BasicBlock::Create(context, "repetition.start", F);
                builder.CreateBr(rstart);

                builder.SetInsertPoint(rstart);
                Value *pos = builder.CreateLoad(len);
                Value *current = builder.CreateGEP(str, pos);
                Value *character = builder.CreateLoad(current);
                Value *index = builder.CreateZExt(character, builder.getInt32Ty());
                Value *bitsetget;
                DECLARE_FUNC(M, bitsetget, builder.getInt32Ty(), "call_bitset_get", bitsetRefType, builder.getInt32Ty());
                Value *result;
                CALL_FUNC(builder, result, bitsetget, set, index);
                Value *cond = builder.CreateICmpNE(result, builder.getInt32(0));
                BasicBlock *rbody = BasicBlock::Create(context, "repitition.body", F);
                BasicBlock *rend = BasicBlock::Create(context, "repitition.end", F);
                builder.CreateCondBr(cond, rend, rbody);

                builder.SetInsertPoint(rbody);
                Value *newpos = builder.CreateAdd(pos, builder.getInt32(1));
                builder.CreateStore(newpos, len);
                builder.CreateBr(rstart);

                builder.SetInsertPoint(rend);
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

    //builder.CreateRet(builder.getInt8(0));
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
