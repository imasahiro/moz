#include "jit.h"

#ifdef MOZVM_ENABLE_JIT
#include "pstring.h"
#include "jmptbl.h"
#include "instruction.h"

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


using namespace std;
using namespace llvm;

static inline int nterm_has_inst(mozvm_nterm_entry_t *e, moz_inst_t *inst)
{
    return e->begin <= inst && inst <= e->end;
}

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
StructType *define_struct_type(const char *name, const Args&... args)
{
    vector<Type *> elements;
    set_vector(&elements, args...);
    ArrayRef<Type *> elmsRef(elements);

    LLVMContext& Ctx = getGlobalContext();
    StructType *s_ty = StructType::create(Ctx, name);
    s_ty->setBody(elmsRef);
    return s_ty;
}

template<class... Args>
Value *create_get_element_ptr(IRBuilder<> &builder, Value *Val, const Args&... args)
{
    vector<Value *> indexs;
    set_vector(&indexs, args...);
    ArrayRef<Value *> idxsRef(indexs);

    return builder.CreateGEP(Val, idxsRef);
}

template<class Returns, class... Args>
FunctionType *get_function_type(const Returns& returntype, const Args&... args)
{
    vector<Type *> parameters;
    set_vector(&parameters, args...);
    ArrayRef<Type *> paramsRef(parameters);

    return FunctionType::get(returntype, paramsRef, false);
}

template<class... Args>
Value *create_call_inst(IRBuilder<> &builder, Value *F, const Args&... args)
{
    vector<Value *> arguments;
    set_vector(&arguments, args...);
    ArrayRef<Value *> argsRef(arguments);

    return builder.CreateCall(F, argsRef);
}

Value *get_current(IRBuilder<> &builder, Value *str, Value *pos)
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    return pos;
#else
    return builder.CreateGEP(str, pos);
#endif
}

Value *consume_n(IRBuilder<> &builder, Value *pos, Value *N)
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    return builder.CreateGEP(pos, N);
#else
    return builder.CreateAdd(pos, N);
#endif
}

Value *consume(IRBuilder<> &builder, Value *pos)
{
    return consume_n(builder, pos, builder.getInt64(1));
}

void stack_push(IRBuilder<> &builder, Value *sp, Value *I64Val)
{
    Value *top = builder.CreateLoad(sp);
    Value *next_top = builder.CreateGEP(top, builder.getInt64(1));
    builder.CreateStore(next_top, sp);
    builder.CreateStore(I64Val, top);
}

Value *stack_pop(IRBuilder<> &builder, Value *sp)
{
    Value *top = builder.CreateLoad(sp);
    Value *prev_top = builder.CreateGEP(top, builder.getInt64(-1));
    builder.CreateStore(prev_top, sp);
    return builder.CreateLoad(prev_top);
}

void stack_push_pos(IRBuilder<> &builder, Value *sp, Value *pos)
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    Value *pos_ = builder.CreatePtrToInt(pos, builder.getInt64Ty());
    stack_push(builder, sp, pos_);
#else
    stack_push(builder, sp, pos);
#endif
}

Value *stack_pop_pos(IRBuilder<> &builder, Value *sp)
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    Value *pos_ = stack_pop(builder, sp);
    return builder.CreateIntToPtr(pos_, builder.getInt8PtrTy());
#else
    return stack_pop(builder, sp);
#endif
}

void stack_push_frame(IRBuilder<> &builder, Value *sp, Value *fp,
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

void stack_pop_frame(IRBuilder<> &builder, Value *sp, Value *fp,
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

void stack_peek_frame(IRBuilder<> &builder, Value *sp, Value *fp,
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

Value *get_bitset_ptr(IRBuilder<> &builder, Value *runtime, BITSET_t id)
{
#if MOZVM_SMALL_BITSET_INST
    Value *r_c_sets = create_get_element_ptr(builder, runtime,
            builder.getInt64(0),
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
            builder.getInt32(11),
#else
            builder.getInt32(10),
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

Value *get_string_ptr(IRBuilder<> &builder, Value *runtime, STRING_t id)
{
#if MOZVM_SMALL_STRING_INST
    Value *r_c_strs = create_get_element_ptr(builder, runtime,
            builder.getInt64(0),
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
            builder.getInt32(11),
#else
            builder.getInt32(10),
#endif /*MOZVM_USE_DYNAMIC_DEACTIVATION*/
            builder.getInt32(2));
    Value *strs_head = builder.CreateLoad(r_c_strs);
    Value *str_head  = builder.CreateGEP(strs_head, builder.getInt64(id));
    return builder.CreateLoad(str_head);
#else
    asm volatile("int3");
    // const char *_set = (const char *)id;
    return nullptr;
#endif /*MOZVM_SMALL_STRING_INST*/
}

template<unsigned n>
Value *get_jump_table(IRBuilder<> &builder, Value *runtime, uint16_t id)
{
#ifdef MOZVM_USE_JMPTBL
    Value *r_c_jumps = create_get_element_ptr(builder, runtime,
            builder.getInt64(0),
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
            builder.getInt32(11),
#else
            builder.getInt32(10),
#endif /*MOZVM_USE_DYNAMIC_DEACTIVATION*/
            builder.getInt32(4 + n));
    Value *jumps_head = builder.CreateLoad(r_c_jumps);
    return builder.CreateGEP(jumps_head, builder.getInt64(id));
#else
    return nullptr;
#endif /*MOZVM_USE_JMPTBL*/
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
#ifdef MOZVM_USE_JMPTBL
    StructType *jmptbl1Type;
    StructType *jmptbl2Type;
    StructType *jmptbl3Type;
#endif
    StructType *nodeType;
    StructType *astType;
    StructType *symtableType;
    StructType *memoType;
    Type *mozposType;
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
    StructType *memopointType;
#endif
    StructType *runtimeType;
    FunctionType *funcType;

    FunctionType *bitsetgetType;
#ifdef MOZVM_USE_JMPTBL
    // FunctionType *tbljmp1idxType;
    FunctionType *tbljmp2idxType;
    // FunctionType *tbljmp3idxType;
#endif
    FunctionType *pstrlenType;
    FunctionType *pstrstwithType;
    FunctionType *astsaveType;
    FunctionType *astrollbackType;
    FunctionType *tblsaveType;
    FunctionType *tblrollbackType;

    ~JitContext() {};
    JitContext();

    FunctionType *create_bitset_get(IRBuilder<> &builder, Module *M);
#ifdef MOZVM_USE_JMPTBL
    // FunctionType *create_jump_table1_index(IRBuilder<> &builder, Module *M);
    FunctionType *create_jump_table2_index(IRBuilder<> &builder, Module *M);
    // FunctionType *create_jump_table3_index(IRBuilder<> &builder, Module *M);
#endif
    FunctionType *create_pstring_length(IRBuilder<> &builder, Module *M);
    FunctionType *create_pstring_startswith(IRBuilder<> &builder, Module *M);
    FunctionType *create_ast_save_tx(IRBuilder<> &builder, Module *M);
    FunctionType *create_symtable_savepoint(IRBuilder<> &builder, Module *M);
    FunctionType *create_symtable_rollback(IRBuilder<> &builder, Module *M);
};

JitContext::JitContext()
{
    LLVMContext& Ctx = getGlobalContext();
    IRBuilder<> builder(Ctx);

    Type *VoidTy = Type::getVoidTy(Ctx);
    Type *I8Ty    = Type::getInt8Ty(Ctx);
    Type *I16Ty   = Type::getInt16Ty(Ctx);
    Type *I32Ty   = Type::getInt32Ty(Ctx);
    Type *I64Ty   = Type::getInt64Ty(Ctx);
    Type *I8PtrTy    = I8Ty->getPointerTo();
    Type *I8PtrPtrTy = I8PtrTy->getPointerTo();
    Type *I32PtrTy   = I32Ty->getPointerTo();
    Type *I64PtrTy   = I64Ty->getPointerTo();

#if defined(BITSET_USE_ULONG)
    bsetType = define_struct_type("bitset_t", ArrayType::get(I64Ty, (256/BITS)));
#elif defined(BITSET_USE_UINT)
    bsetType = define_struct_type("bitset_t", ArrayType::get(I32Ty, (256/BITS)));
#endif
#ifdef MOZVM_USE_JMPTBL
    jmptbl1Type = StructType::create(Ctx, "jump_table1_t");
    jmptbl2Type = define_struct_type("jump_table2_t",
            ArrayType::get(bsetType, 2),
            ArrayType::get(I32Ty, 4)
            );
    jmptbl3Type = StructType::create(Ctx, "jump_table3_t");
#endif

    StructType *constantType = define_struct_type("mozvm_constant_t",
            bsetType->getPointerTo(),
            I8PtrPtrTy, // tags
            I8PtrPtrTy, // strs
            I8PtrPtrTy, // tables
            I32PtrTy,   // jumps
#ifdef MOZVM_USE_JMPTBL
            jmptbl1Type->getPointerTo(), // jumps1
            jmptbl2Type->getPointerTo(), // jumps2
            jmptbl3Type->getPointerTo(), // jumps3
#endif
            I8PtrPtrTy, // nterms
            I16Ty, // set_size
            I16Ty, // str_size
            I16Ty, // tag_size
            I16Ty, // table_size
            I16Ty, // nterm_size
            I32Ty, // inst_size
            I32Ty, // memo_size
            I32Ty  // input_size
#ifdef MOZVM_PROFILE_INST
            , I64PtrTy // profile
#endif
            );

    nodeType = StructType::create(Ctx, "Node");
    StructType *astlogType = StructType::create(Ctx, "AstLog");
    StructType *astlogarrayType = define_struct_type("ARRAY_AstLog_t",
            I32Ty, // size
            I32Ty, // capacity
            astlogType->getPointerTo() // list
            );
    astType = define_struct_type("AstMachine",
            astlogarrayType, // logs
            nodeType->getPointerTo(), // last-linked
            nodeType->getPointerTo(), // parsed
            I8PtrTy // source
            );

    StructType *entryType = StructType::create(Ctx, "entry_t");
    StructType *entryarrayType = define_struct_type("ARRAY_entry_t_t",
            I32Ty, // size
            I32Ty, // capacity
            entryType->getPointerTo() // list
            );
    symtableType = define_struct_type("symtable_t",
            I32Ty, // state
            entryarrayType // table
            );

    memoType = StructType::create(Ctx, "memo_t");
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    mozposType = I8PtrTy;
#else
    mozposType = I64Ty;
#endif
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
    memopointType = StructType::create(Ctx, "MemoPoint");
#endif
    runtimeType = define_struct_type("moz_runtime_t",
            astType->getPointerTo(),
            symtableType->getPointerTo(),
            memoType->getPointerTo(),
            mozposType, // head
            I8PtrTy, // tail
            I8PtrTy, // input
            I64PtrTy, // stack
            I64PtrTy, // fp
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
            memopointType->getPointerTo(),
#endif
            I8PtrTy, // nterm_entry
            I8PtrTy, // jit_context
            constantType,
            ArrayType::get(I64Ty, 1)
            );

    funcType = get_function_type(I8Ty, runtimeType->getPointerTo(),
            I8PtrTy, mozposType->getPointerTo());

    sys::DynamicLibrary::AddSymbol(llvm::StringRef("ast_rollback_tx"),
            reinterpret_cast<void *>(ast_rollback_tx));
    astrollbackType = get_function_type(VoidTy, astType->getPointerTo(), I64Ty);

    Module *M = new Module("top", Ctx);
    bitsetgetType   = create_bitset_get(builder, M);
#ifdef MOZVM_USE_JMPTBL
    // tbljmp1idxType = create_jump_table1_index(builder, M);
    tbljmp2idxType = create_jump_table2_index(builder, M);
    // tbljmp3idxType = create_jump_table3_index(builder, M);
#endif
    pstrlenType     = create_pstring_length(builder, M);
    pstrstwithType  = create_pstring_startswith(builder, M);
    astsaveType     = create_ast_save_tx(builder, M);
    tblsaveType     = create_symtable_savepoint(builder, M);
    tblrollbackType = create_symtable_rollback(builder, M);
    EE = EngineBuilder(unique_ptr<Module>(M)).create();
    EE->finalizeObject();
}

FunctionType *JitContext::create_bitset_get(IRBuilder<> &builder, Module *M)
{
    LLVMContext& Ctx = M->getContext();
    Type *I32Ty = builder.getInt32Ty();
    Type *I64Ty = builder.getInt64Ty();
    Type *bsetPtrTy = bsetType->getPointerTo();
    FunctionType *funcTy = get_function_type(I32Ty, bsetPtrTy, I32Ty);
    Function *F;
    Constant *i32_0 = builder.getInt32(0);
    Constant *i64_0 = builder.getInt64(0);

    F = Function::Create(funcTy, Function::ExternalLinkage, "bitset_get", M);

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
    Value *Ptr    = create_get_element_ptr(builder, set, i64_0, i32_0, Div_);
    Value *Data   = builder.CreateLoad(Ptr);
    Value *Result = builder.CreateAnd(Data, Mask);
    Value *NEq0   = builder.CreateICmpNE(Result, i64_0);
#elif defined(BITSET_USE_UINT)
    Value *Mod    = builder.CreateAnd(idx, builder.getInt32(31));
    Value *Mask   = builder.CreateShl(builder.getInt32(1), Mod);
    Value *Div    = builder.CreateLShr(idx, builder.getInt32(5));
    Value *Div_   = builder.CreateZExt(Div, I64Ty);
    Value *Ptr    = create_get_element_ptr(builder, set, i64_0, i32_0, Div_);
    Value *Data   = builder.CreateLoad(Ptr);
    Value *Result = builder.CreateAnd(Data, Mask);
    Value *NEq0   = builder.CreateICmpNE(Result, i32_0);
#endif
    Value *RetVal = builder.CreateZExt(NEq0, I32Ty);
    builder.CreateRet(RetVal);
    return funcTy;
}

#ifdef MOZVM_USE_JMPTBL
FunctionType *JitContext::create_jump_table2_index(IRBuilder<> &builder, Module *M)
{
    LLVMContext& Ctx = M->getContext();
    Type *I8Ty  = builder.getInt8Ty();
    Type *I32Ty = builder.getInt32Ty();
    Type *jmptbl2PtrTy = jmptbl2Type->getPointerTo();
    FunctionType *funcTy = get_function_type(I32Ty, jmptbl2PtrTy, I8Ty);
    Function *F;
    Constant *i32_0 = builder.getInt32(0);
    Constant *i32_1 = builder.getInt32(1);
    Constant *i64_0 = builder.getInt64(0);
    Constant *i64_1 = builder.getInt64(1);

    Constant *f_bitsetget = M->getOrInsertFunction("bitset_get", bitsetgetType);

    F = Function::Create(funcTy, Function::ExternalLinkage, "jump_table2_index", M);

    Function::arg_iterator arg_iter = F->arg_begin();
    Value *tbl = arg_iter++;
    Value *ch  = arg_iter++;

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

    Value *ch_    = builder.CreateZExt(ch, I32Ty);
    Value *tbl_b0 = create_get_element_ptr(builder, tbl, i64_0, i32_0, i64_0);
    Value *idx0   = create_call_inst(builder, f_bitsetget, tbl_b0, ch_);
    Value *tbl_b1 = create_get_element_ptr(builder, tbl, i64_0, i32_0, i64_1);
    Value *idx1   = create_call_inst(builder, f_bitsetget, tbl_b1, ch_);
    Value *idx1_  = builder.CreateShl(idx1, i32_1);
    Value *idx    = builder.CreateOr(idx1_, idx0);
    builder.CreateRet(idx);
    return funcTy;
}
#endif

FunctionType *JitContext::create_pstring_length(IRBuilder<> &builder, Module *M)
{
    LLVMContext& Ctx = M->getContext();
    Type *I32Ty = builder.getInt32Ty();
    Type *I8PtrTy  = builder.getInt8PtrTy();
    Type *I32PtrTy = I32Ty->getPointerTo();
    FunctionType *funcTy = get_function_type(I32Ty, I8PtrTy);
    Function *F;
    unsigned *lenoffset = &(((pstring_t *)NULL)->len);
    char *stroffset = ((pstring_t *)NULL)->str;
    Constant *offset = builder.getInt64((long)lenoffset - (long)stroffset);

    F = Function::Create(funcTy, Function::ExternalLinkage, "pstring_length", M);

    Function::arg_iterator arg_iter = F->arg_begin();
    Value *str = arg_iter++;

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

    Value *len_ptr_ = builder.CreateGEP(str, offset);
    Value *len_ptr  = builder.CreateBitCast(len_ptr_, I32PtrTy);
    Value *len      = builder.CreateLoad(len_ptr);
    builder.CreateRet(len);
    return funcTy;
}

FunctionType *JitContext::create_pstring_startswith(IRBuilder<> &builder, Module *M)
{
    LLVMContext& Ctx = M->getContext();
    Type *I32Ty = builder.getInt32Ty();
    Type *I64Ty = builder.getInt64Ty();
    Type *I8PtrTy  = builder.getInt8PtrTy();
    FunctionType *funcTy = get_function_type(I32Ty, I8PtrTy, I8PtrTy, I32Ty);
    Function *F;

    F = Function::Create(funcTy, Function::ExternalLinkage, "pstring_starts_with", M);

    Function::arg_iterator arg_iter = F->arg_begin();
    Value *str  = arg_iter++;
    Value *text = arg_iter++;
    Value *len  = arg_iter++;

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

// #ifdef __AVX2__
// #endif
#if defined(PSTRING_USE_STRCMP)
    FunctionType *strncmpTy = get_function_type(I32Ty, I8PtrTy, I8PtrTy, I64Ty);
    Constant *f_strncmp = M->getOrInsertFunction("strncmp", strncmpTy);

    Value *len_   = builder.CreateZExt(len, I64Ty);
    Value *result = create_call_inst(builder, f_strncmp, str, text, len_);
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
    Value *ifcond = builder.CreateICmpEQ(str_char, text_char);
    builder.CreateCondBr(ifcond, wcondBB, wretBB);

    builder.SetInsertPoint(wendBB);
    builder.CreateRet(builder.getInt32(1));

    builder.SetInsertPoint(wretBB);
    builder.CreateRet(builder.getInt32(0));
#endif
    return funcTy;
}

FunctionType *JitContext::create_ast_save_tx(IRBuilder<> &builder, Module *M)
{
    LLVMContext& Ctx = M->getContext();
    Type *I64Ty = builder.getInt64Ty();
    Type *astPtrTy = astType->getPointerTo();
    FunctionType *funcTy = get_function_type(I64Ty, astPtrTy);
    Function *F;
    Constant *i32_0 = builder.getInt32(0);
    Constant *i64_0 = builder.getInt64(0);

    F = Function::Create(funcTy, Function::ExternalLinkage, "ast_save_tx", M);

    Function::arg_iterator arg_iter = F->arg_begin();
    Value *ast = arg_iter++;

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

    Value *Ptr   = create_get_element_ptr(builder, ast, i64_0, i32_0, i32_0);
    Value *Size  = builder.CreateLoad(Ptr);
    Value *Size_ = builder.CreateZExt(Size, I64Ty);
    builder.CreateRet(Size_);
    return funcTy;
}

FunctionType *JitContext::create_symtable_savepoint(IRBuilder<> &builder, Module *M)
{
    LLVMContext& Ctx = M->getContext();
    Type *I64Ty = builder.getInt64Ty();
    Type *tblPtrTy = symtableType->getPointerTo();
    FunctionType *funcTy = get_function_type(I64Ty, tblPtrTy);
    Function *F;
    Constant *i32_0 = builder.getInt32(0);
    Constant *i32_1 = builder.getInt32(1);
    Constant *i64_0 = builder.getInt64(0);

    F = Function::Create(funcTy, Function::ExternalLinkage, "symtable_savepoint", M);

    Function::arg_iterator arg_iter = F->arg_begin();
    Value *tbl = arg_iter++;

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

    Value *Ptr   = create_get_element_ptr(builder, tbl, i64_0, i32_1, i32_0);
    Value *Size  = builder.CreateLoad(Ptr);
    Value *Size_ = builder.CreateZExt(Size, I64Ty);
    builder.CreateRet(Size_);
    return funcTy;
}

FunctionType *JitContext::create_symtable_rollback(IRBuilder<> &builder, Module *M)
{
    LLVMContext& Ctx = M->getContext();
    Type *I32Ty = builder.getInt32Ty();
    Type *I64Ty = builder.getInt64Ty();
    Type *tblPtrTy = symtableType->getPointerTo();
    FunctionType *funcTy = get_function_type(builder.getVoidTy(), tblPtrTy, I64Ty);
    Function *F;
    Constant *i32_0 = builder.getInt32(0);
    Constant *i32_1 = builder.getInt32(1);
    Constant *i64_0 = builder.getInt64(0);

    F = Function::Create(funcTy, Function::ExternalLinkage, "symtable_rollback", M);

    Function::arg_iterator arg_iter = F->arg_begin();
    Value *tbl   = arg_iter++;
    Value *saved = arg_iter++;

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

    Value *Ptr    = create_get_element_ptr(builder, tbl, i64_0, i32_1, i32_0);
    Value *saved_ = builder.CreateTrunc(saved, I32Ty);
    builder.CreateStore(saved_, Ptr);
    builder.CreateRetVoid();
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

    LLVMContext& Ctx = getGlobalContext();
    IRBuilder<> builder(Ctx);
    Module *M = new Module(runtime->C.nterms[nterm], Ctx);
    _ctx->EE->addModule(unique_ptr<Module>(M));

    Constant *f_bitsetget   = M->getOrInsertFunction("bitset_get", _ctx->bitsetgetType);
    // Constant *f_tbljmp1idx;
    Constant *f_tbljmp2idx;
    // Constant *f_tbljmp3idx;
#ifdef MOZVM_USE_JMPTBL
    // f_tbljmp1idx  = M->getOrInsertFunction("jump_table1_index", _ctx->tbljmp1idxType);
    f_tbljmp2idx  = M->getOrInsertFunction("jump_table2_index", _ctx->tbljmp2idxType);
    // f_tbljmp3idx  = M->getOrInsertFunction("jump_table3_index", _ctx->tbljmp3idxType);
#endif
    Constant *f_pstrlen     = M->getOrInsertFunction("pstring_length", _ctx->pstrlenType);
    Constant *f_pstrstwith  = M->getOrInsertFunction("pstring_starts_with", _ctx->pstrstwithType);
    Constant *f_astsave     = M->getOrInsertFunction("ast_save_tx", _ctx->astsaveType);
    Constant *f_astrollback = M->getOrInsertFunction("ast_rollback_tx", _ctx->astrollbackType);
    Constant *f_tblsave     = M->getOrInsertFunction("symtable_savepoint", _ctx->tblsaveType);
    Constant *f_tblrollback = M->getOrInsertFunction("symtable_rollback", _ctx->tblrollbackType);

    Function *F = Function::Create(_ctx->funcType,
            Function::ExternalLinkage,
            runtime->C.nterms[nterm], M);

    Function::arg_iterator arg_iter=F->arg_begin();
    Value *runtime_ = arg_iter++;
    Value *str = arg_iter++;
    Value *consumed = arg_iter++;

    vector<BasicBlock *> failjumpList;
    unordered_map<moz_inst_t *, BasicBlock *> BBMap;

    moz_inst_t *p = e->begin;
    while (p < e->end) {
        uint8_t opcode = *p;
        unsigned shift = opcode_size(opcode);
        if(opcode == Alt || opcode == Jump) {
            mozaddr_t jump = *((mozaddr_t *)(p+1));
            moz_inst_t *dest = p + shift + jump;
            if(BBMap.find(dest) == BBMap.end()) {
                BasicBlock *label = BasicBlock::Create(Ctx, "jump.label");
                BBMap[dest] = label;
                if(opcode == Alt) {
                    failjumpList.push_back(label);
                }
            }
        }
#ifdef MOZVM_USE_JMPTBL
        else if(opcode == TblJump2) {
            uint16_t tblId = *((uint16_t *)(p+1));
            moz_inst_t *jmpoffset = p + shift;
            int *jumps = runtime->C.jumps2[tblId].jumps;
            int i;
            for(i = 0; i < 4; i++) {
                moz_inst_t *dest = jmpoffset + jumps[i];
                if(!nterm_has_inst(e, dest)) {
                    F->eraseFromParent();
                    return NULL;
                }
                if(BBMap.find(dest) == BBMap.end()) {
                    BBMap[dest] = BasicBlock::Create(Ctx, "jump.label");
                }
            }
        }
#endif /*MOZVM_USE_JMPTBL*/
        p += opcode_size(opcode);
    }

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    BasicBlock *failBB  = BasicBlock::Create(Ctx, "fail");
    BasicBlock *retBB   = BasicBlock::Create(Ctx, "success");
    BasicBlock *errBB   = BasicBlock::Create(Ctx, "error");
    BasicBlock *unreachableBB = BasicBlock::Create(Ctx, "switch.default");
    failjumpList.push_back(errBB);
    BasicBlock *currentBB = entryBB;

    builder.SetInsertPoint(currentBB);
    Constant *i32_0 = builder.getInt32(0);
    Constant *i64_0 = builder.getInt64(0);

    Value *ast_  = create_get_element_ptr(builder, runtime_, i64_0, i32_0);
    Value *ast   = builder.CreateLoad(ast_);
    Value *tbl_  = create_get_element_ptr(builder, runtime_, i64_0, builder.getInt32(1));
    Value *tbl   = builder.CreateLoad(tbl_);
    Value *head_ = create_get_element_ptr(builder, runtime_, i64_0, builder.getInt32(3));
    Value *tail_ = create_get_element_ptr(builder, runtime_, i64_0, builder.getInt32(4));
    Value *tail  = builder.CreateLoad(tail_);

    Value *sp = create_get_element_ptr(builder, runtime_, i64_0, builder.getInt32(6));
    Value *fp = create_get_element_ptr(builder, runtime_, i64_0, builder.getInt32(7));

    {
        BlockAddress *addr = BlockAddress::get(F, errBB);
        Value *pos = builder.CreateLoad(consumed);
        Value *ast_tx = create_call_inst(builder, f_astsave, ast);
        Value *saved  = create_call_inst(builder, f_tblsave, tbl);
        stack_push_frame(builder, sp, fp, pos, addr, ast_tx, saved);
    }

    p = e->begin;
    while (p < e->end) {
        uint8_t opcode = *p;
        unsigned shift = opcode_size(opcode);

        if(BBMap.find(p) != BBMap.end()) {
            BasicBlock *newBB = BBMap[p];
            newBB->insertInto(F);
            if(currentBB->getTerminator() == nullptr) {
                builder.CreateBr(newBB);
            }
            builder.SetInsertPoint(newBB);
            currentBB = newBB;
        }

        switch(opcode) {
#define CASE_(OP) case OP:
            CASE_(Nop) {
                asm volatile("int3");
                break;
            }
            CASE_(Fail) {
                builder.CreateBr(failBB);
                break;
            }
            CASE_(Succ) {
                Value *pos;
                Value *next;
                Value *ast_tx;
                Value *saved;
                stack_pop_frame(builder, sp, fp, &pos, &next, &ast_tx, &saved);
                break;
            }
            CASE_(Alt) {
                mozaddr_t failjump = *((mozaddr_t *)(p+1));
                moz_inst_t *dest = p + shift + failjump;

                BlockAddress *addr = BlockAddress::get(F, BBMap[dest]);
                Value *pos = builder.CreateLoad(consumed);
                Value *ast_tx = create_call_inst(builder, f_astsave, ast);
                Value *saved  = create_call_inst(builder, f_tblsave, tbl);
                stack_push_frame(builder, sp, fp, pos, addr, ast_tx, saved);
                break;
            }
            CASE_(Jump) {
                mozaddr_t jump = *((mozaddr_t *)(p+1));
                moz_inst_t *dest = p + shift + jump;

                builder.CreateBr(BBMap[dest]);
                break;
            }
            CASE_(Call) {
                asm volatile("int3");
                break;
            }
            CASE_(Ret) {
                builder.CreateBr(retBB);
                break;
            }
            CASE_(Pos) {
                Value *pos = builder.CreateLoad(consumed);
                stack_push_pos(builder, sp, pos);
                break;
            }
            CASE_(Back) {
                Value *pos = stack_pop_pos(builder, sp);
                builder.CreateStore(pos, consumed);
                break;
            }
            CASE_(Skip) {
                BasicBlock *next = BasicBlock::Create(Ctx, "skip.next", F);

                Value *prev_pos_;
                Value *next_;
                Value *ast_tx_;
                Value *saved_;
                stack_peek_frame(builder, sp, fp, &prev_pos_, &next_, &ast_tx_, &saved_);
                Value *pos = builder.CreateLoad(consumed);
                Value *prev_pos = builder.CreateLoad(prev_pos_);
                Value *ifcond = builder.CreateICmpEQ(prev_pos, pos);
                builder.CreateCondBr(ifcond, failBB, next);

                builder.SetInsertPoint(next);
                builder.CreateStore(pos, prev_pos_);
                Value *ast_tx = create_call_inst(builder, f_astsave, ast);
                builder.CreateStore(ast_tx, ast_tx_);
                Value *saved = create_call_inst(builder, f_tblsave, tbl);
                builder.CreateStore(saved, saved_);
                break;
            }
            CASE_(Byte) {
                BasicBlock *succ = BasicBlock::Create(Ctx, "byte.succ", F);
                uint8_t ch = (uint8_t)*(p+1);
                Constant *C = builder.getInt8(ch);

                Value *pos = builder.CreateLoad(consumed);
                Value *current = get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *cond = builder.CreateICmpNE(character, C);
                builder.CreateCondBr(cond, failBB, succ);

                builder.SetInsertPoint(succ);
                Value *nextpos = consume(builder, pos);
                builder.CreateStore(nextpos, consumed);
                currentBB = succ;
                break;
            }
            CASE_(NByte) {
                asm volatile("int3");
                break;
            }
            CASE_(OByte) {
                BasicBlock *obody = BasicBlock::Create(Ctx, "obyte.body", F);
                BasicBlock *oend  = BasicBlock::Create(Ctx, "obyte.end", F);
                uint8_t ch = (uint8_t)*(p+1);
                Constant *C = builder.getInt8(ch);

                Value *pos = builder.CreateLoad(consumed);
                Value *current = get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *cond = builder.CreateICmpEQ(character, C);
                builder.CreateCondBr(cond, obody, oend);

                builder.SetInsertPoint(obody);
                Value *nextpos = consume(builder, pos);
                builder.CreateStore(nextpos, consumed);
                builder.CreateBr(oend);

                builder.SetInsertPoint(oend);
                currentBB = oend;
                break;
            }
            CASE_(RByte) {
                asm volatile("int3");
                break;
            }
            CASE_(Any) {
                BasicBlock *succ = BasicBlock::Create(Ctx, "any.succ", F);

                Value *pos = builder.CreateLoad(consumed);
                Value *current = get_current(builder, str, pos);
                Value *cond = builder.CreateICmpEQ(current, tail);
                builder.CreateCondBr(cond, failBB, succ);

                builder.SetInsertPoint(succ);
                Value *nextpos = consume(builder, pos);
                builder.CreateStore(nextpos, consumed);
                currentBB = succ;
                break;
            }
            CASE_(NAny) {
                BasicBlock *succ = BasicBlock::Create(Ctx, "nany.succ", F);

                Value *pos = builder.CreateLoad(consumed);
                Value *current = get_current(builder, str, pos);
                Value *cond = builder.CreateICmpNE(current, tail);
                builder.CreateCondBr(cond, failBB, succ);

                builder.SetInsertPoint(succ);
                currentBB = succ;
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
                BasicBlock *succ = BasicBlock::Create(Ctx, "str.succ", F);
                STRING_t strId = *((STRING_t *)(p+1));

                Value *str = get_string_ptr(builder, runtime_, strId);
                Value *len = create_call_inst(builder, f_pstrlen, str);
                Value *pos = builder.CreateLoad(consumed);
                Value *current = get_current(builder, str, pos);
                Value *result = create_call_inst(builder, f_pstrstwith, current, str, len);
                Value *cond = builder.CreateICmpEQ(result, i32_0);
                builder.CreateCondBr(cond, failBB, succ);

                builder.SetInsertPoint(succ);
                Value *len_ = builder.CreateZExt(len, builder.getInt64Ty());
                Value *nextpos = consume_n(builder, pos, len_);
                builder.CreateStore(nextpos, consumed);
                currentBB = succ;
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
                BasicBlock *succ = BasicBlock::Create(Ctx, "set.succ", F);
                BITSET_t setId = *((BITSET_t *)(p+1));

                Value *set = get_bitset_ptr(builder, runtime_, setId);
                Value *pos = builder.CreateLoad(consumed);
                Value *current = get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *index = builder.CreateZExt(character, builder.getInt32Ty());
                Value *result = create_call_inst(builder, f_bitsetget, set, index);
                Value *cond = builder.CreateICmpEQ(result, i32_0);
                builder.CreateCondBr(cond, failBB, succ);

                builder.SetInsertPoint(succ);
                Value *nextpos = consume(builder, pos);
                builder.CreateStore(nextpos, consumed);
                currentBB = succ;
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
                BasicBlock *rcond = BasicBlock::Create(Ctx, "rset.cond", F);
                BasicBlock *rbody = BasicBlock::Create(Ctx, "rset.body", F);
                BasicBlock *rend  = BasicBlock::Create(Ctx, "rset.end",  F);

                BITSET_t setId = *((BITSET_t *)(p+1));
                Value *set = get_bitset_ptr(builder, runtime_, setId);
                Value *firstpos = builder.CreateLoad(consumed);
                builder.CreateBr(rcond);

                builder.SetInsertPoint(rcond);
                PHINode *pos = builder.CreatePHI(_ctx->mozposType, 2);
                pos->addIncoming(firstpos, currentBB);
                Value *current = get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *index = builder.CreateZExt(character, builder.getInt32Ty());
                Value *result = create_call_inst(builder, f_bitsetget, set, index);
                Value *cond = builder.CreateICmpNE(result, i32_0);
                builder.CreateCondBr(cond, rbody, rend);

                builder.SetInsertPoint(rbody);
                Value *nextpos = consume(builder, pos);
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
                uint16_t tblId = *((uint16_t *)(p+1));
                moz_inst_t *jmpoffset = p + shift;
                int *jumps = runtime->C.jumps2[tblId].jumps;

                Value *tbl = get_jump_table<2>(builder, runtime_, tblId);
                Value *pos = builder.CreateLoad(consumed);
                Value *current = get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *idx = create_call_inst(builder, f_tbljmp2idx, tbl, character);

                SwitchInst *jump = builder.CreateSwitch(idx, unreachableBB, 4);
                for(int i = 0; i < 4; i++) {
                    moz_inst_t *dest = jmpoffset + jumps[i];
                    assert(nterm_has_inst(e, dest));
                    jump->addCase(builder.getInt32(i), BBMap[dest]);
                }
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

    failBB->insertInto(F);
    builder.SetInsertPoint(failBB);
    {
        BasicBlock *posback  = BasicBlock::Create(Ctx, "fail.posback",  F);
        BasicBlock *rollback = BasicBlock::Create(Ctx, "fail.rollback", F);

        Value *pos = builder.CreateLoad(consumed);
        Value *pos_;
        Value *next_;
        Value *ast_tx_;
        Value *saved_;
        stack_pop_frame(builder, sp, fp, &pos_, &next_, &ast_tx_, &saved_);
        Value *ifcond = builder.CreateICmpULT(pos_, pos);
        builder.CreateCondBr(ifcond, posback, rollback);

        builder.SetInsertPoint(posback);
        Value *head = builder.CreateLoad(head_);
        Value *selectcond = builder.CreateICmpULT(head, pos);
        Value *newhead = builder.CreateSelect(selectcond, pos, head);
        builder.CreateStore(newhead, head_);
        builder.CreateStore(pos_, consumed);
        builder.CreateBr(rollback);

        builder.SetInsertPoint(rollback);
        create_call_inst(builder, f_astrollback, ast, ast_tx_);
        create_call_inst(builder, f_tblrollback, tbl, saved_);
        IndirectBrInst *indirect_br = builder.CreateIndirectBr(next_, failjumpList.size());
        vector<BasicBlock *>::iterator itr;
        for(itr = failjumpList.begin(); itr != failjumpList.end(); itr++) {
            indirect_br->addDestination(*itr);
        }
    }

    retBB->insertInto(F);
    builder.SetInsertPoint(retBB);
    {
        Value *pos;
        Value *next;
        Value *ast_tx;
        Value *saved;
        stack_pop_frame(builder, sp, fp, &pos, &next, &ast_tx, &saved);
        builder.CreateRet(builder.getInt8(0));
    }

    errBB->insertInto(F);
    builder.SetInsertPoint(errBB);
    builder.CreateRet(builder.getInt8(1));

    unreachableBB->insertInto(F);
    builder.SetInsertPoint(unreachableBB);
    builder.CreateUnreachable();

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
#endif /*MOZVM_ENABLE_JIT*/
