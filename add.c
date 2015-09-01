#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <stdio.h>
#include <stdint.h>

typedef int32_t (*addFunc_t)(int32_t, int32_t);

int main() {
	LLVMModuleRef module = LLVMModuleCreateWithName("top");
	LLVMBuilderRef builder = LLVMCreateBuilder();

	/* function */
	LLVMTypeRef argsRef[] = { LLVMInt32Type(), LLVMInt32Type() };
	LLVMTypeRef addType = LLVMFunctionType(LLVMInt32Type(), argsRef, 2, 0);
	LLVMValueRef addFunc = LLVMAddFunction(module, "add", addType);

	/* body */
	LLVMBasicBlockRef entry = LLVMAppendBasicBlock(addFunc, "entrypoint");
	LLVMPositionBuilderAtEnd(builder, entry);

	LLVMValueRef result = LLVMBuildAdd(builder, LLVMGetParam(addFunc, 0), LLVMGetParam(addFunc, 1), "result");
	LLVMBuildRet(builder, result);
	
	//module->dump();

	/* jit */
	LLVMLinkInJIT();
	LLVMInitializeNativeTarget();
	LLVMExecutionEngineRef EE;
	char *error = NULL;
	if(LLVMCreateExecutionEngineForModule(&EE, module, &error) != 0 || error) {
		LLVMDisposeMessage(error);
		return NULL;
	}

	/* run */
	addFunc_t funcp = NULL;
	if(!(funcp = (addFunc_t)LLVMGetPointerToGlobal(EE, addFunc))) {
		return 1;
	}
	printf("%d\n", funcp(10,20));

	LLVMDisposeBuilder(builder);
	LLVMDisposeExecutionEngine(EE);
	return 0;
}
