#include "llvm/Support/TargetSelect.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void *createAddFunc() {
	llvm::LLVMContext& context = llvm::getGlobalContext();
	llvm::Module *module = new llvm::Module("top", context);
	llvm::IRBuilder<> builder(context);

	/* function */
	std::vector<llvm::Type *> putsArgs;
	putsArgs.push_back(builder.getInt32Ty());
	putsArgs.push_back(builder.getInt32Ty());
	llvm::ArrayRef<llvm::Type*> argsRef(putsArgs);

	llvm::FunctionType *addType =
		llvm::FunctionType::get(builder.getInt32Ty(), argsRef, false);
	llvm::Function *addFunc =
		llvm::Function::Create(addType, llvm::Function::ExternalLinkage, "add", module);

	/* args */
	llvm::Function::arg_iterator arg_iter=addFunc->arg_begin();
	llvm::Value *x = arg_iter++;
	x->setName("x");
	llvm::Value *y = arg_iter++;
	y->setName("y");

	/* body */
	llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entrypoint", addFunc);
	builder.SetInsertPoint(entry);
	
	llvm::Value *result = builder.CreateAdd(x, y);
	result->setName("result");
	builder.CreateRet(result);

	//module->dump();
	llvm::InitializeNativeTarget();
	llvm::ExecutionEngine *EE;
	if(!(EE = llvm::EngineBuilder(module).create())) {
		return NULL;
	}
	return EE->getPointerToFunction(addFunc);
}

int main() {
	int32_t (*funcp)(int32_t, int32_t) = NULL;
	if(!(funcp = (int32_t (*)(int32_t, int32_t))createAddFunc())) {
		return 1;
	}
	printf("%d\n", funcp(10,20));
	return 0;
}

#ifdef __cplusplus
}
#endif
