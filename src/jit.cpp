#include "llvm/Support/TargetSelect.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
//#include <stdio.h>
#include "mozvm.h"
#include "vm_inst.h"

#ifdef __cplusplus
extern "C" {
#endif

void *mozvm_jit_compile(moz_runtime_t *runtime, uint16_t nterm) {
	mozvm_nterm_entry_t *e = runtime->nterm_entry + nterm;
	llvm::InitializeNativeTarget();
	llvm::LLVMContext& context = llvm::getGlobalContext();
	llvm::Module *module = new llvm::Module("top", context);
	llvm::IRBuilder<> builder(context);
	llvm::ExecutionEngine *EE;
	if(!(EE = llvm::EngineBuilder(module).create())) {
		return NULL;
	}

	llvm::StructType *runtimeType =
		llvm::StructType::create(context, "struct.moz_runtime_t");

	llvm::Type *argTypes[2] = { runtimeType->getPointerTo(), builder.getInt8PtrTy() };
	llvm::ArrayRef<llvm::Type*> argsRef(argTypes, 2);

	llvm::FunctionType *funcType =
		llvm::FunctionType::get(builder.getInt32Ty(), argsRef, false);
	llvm::Function *F =
		llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, std::to_string(nterm), module);

	llvm::Function::arg_iterator arg_iter=F->arg_begin();
	llvm::Value *runtime_ = arg_iter++;
	llvm::Value *str = arg_iter++;

	llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entrypoint", F);
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
				asm volatile("int3");
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
				asm volatile("int3");
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

	module->dump();
	return EE->getPointerToFunction(F);
}

#ifdef __cplusplus
}
#endif
