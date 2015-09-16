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

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

using namespace std;
using namespace llvm;

struct Symbol {
    const char *name;
    void *func;
};
static const Symbol symbols[] = {
    { ("ast_rollback_tx"), reinterpret_cast<void *>(ast_rollback_tx) },
    { ("ast_commit_tx"),   reinterpret_cast<void *>(ast_commit_tx) },
    { ("ast_log_replace"), reinterpret_cast<void *>(ast_log_capture) },
    { ("ast_log_new"),     reinterpret_cast<void *>(ast_log_new) },
    { ("ast_log_pop"),     reinterpret_cast<void *>(ast_log_pop) },
    { ("ast_log_push"),    reinterpret_cast<void *>(ast_log_push) },
    { ("ast_log_swap"),    reinterpret_cast<void *>(ast_log_swap) },
    { ("ast_log_tag"),     reinterpret_cast<void *>(ast_log_tag) },
    { ("ast_log_link"),    reinterpret_cast<void *>(ast_log_link) },
    { ("memo_set"),        reinterpret_cast<void *>(memo_set) },
    { ("memo_fail"),       reinterpret_cast<void *>(memo_fail) },
    { ("memo_get"),        reinterpret_cast<void *>(memo_get) }
};


static inline int nterm_has_inst(mozvm_nterm_entry_t *e, moz_inst_t *inst)
{
    return e->begin <= inst && inst <= e->end;
}

BasicBlock *get_jump_destination(mozvm_nterm_entry_t *e, moz_inst_t *dest, BasicBlock *failBB)
{
    if(!nterm_has_inst(e, dest)) {
        if((*dest) == Fail) {
            return failBB;
        }
        else {
            return nullptr;
        }
    }
    else {
        LLVMContext& Ctx = getGlobalContext();
        return BasicBlock::Create(Ctx, "jump.label");
    }
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
void define_struct_body(StructType *s_ty, const Args&... args)
{
    vector<Type *> elements;
    set_vector(&elements, args...);
    ArrayRef<Type *> elmsRef(elements);

    LLVMContext& Ctx = getGlobalContext();
    s_ty->setBody(elmsRef);
}

template<class... Args>
StructType *define_struct_type(const char *name, const Args&... args)
{
    LLVMContext& Ctx = getGlobalContext();
    StructType *s_ty = StructType::create(Ctx, name);

    define_struct_body(s_ty, args...);
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

Value *get_length(IRBuilder<> &builder, Value *startpos, Value *endpos)
{
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    Type *I64Ty = builder.getInt64Ty();
    Value *endpos_   = builder.CreatePtrToInt(endpos,   I64Ty);
    Value *startpos_ = builder.CreatePtrToInt(startpos, I64Ty);
    return builder.CreateSub(endpos_, startpos_);
#else
    return builder.CreateSub(endpos, startpos);
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

Value *get_callee_function(IRBuilder<> &builder, Value *runtime, uint16_t nterm)
{
    Value *r_nterm_entry = create_get_element_ptr(builder, runtime,
            builder.getInt64(0),
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
            builder.getInt32(11)
#else
            builder.getInt32(10)
#endif
            );
    Value *entry_head = builder.CreateLoad(r_nterm_entry);
    Value *callee     = create_get_element_ptr(builder, entry_head,
            builder.getInt64(nterm), builder.getInt32(3));
    return builder.CreateLoad(callee);
}

Value *get_bitset_ptr(IRBuilder<> &builder, Value *runtime, BITSET_t id)
{
#if MOZVM_SMALL_BITSET_INST
    Value *r_c_sets = create_get_element_ptr(builder, runtime,
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

Value *get_tag_id(IRBuilder<> &builder, Value *runtime, TAG_t id)
{
#if MOZVM_SMALL_TAG_INST
    return builder.getInt16(id);
#else
    asm volatile("int3");
    // uint16_t _tagId = (uint16_t)(runtime->C.tags - tagId);
    return nullptr;
#endif /*MOZVM_SMALL_TAG_INST*/
}

Value *get_tag_ptr(IRBuilder<> &builder, Value *runtime, TAG_t id)
{
#if MOZVM_SMALL_TAG_INST
    Value *r_c_tags = create_get_element_ptr(builder, runtime,
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

Value *get_string_ptr(IRBuilder<> &builder, Value *runtime, STRING_t id)
{
#if MOZVM_SMALL_STRING_INST
    Value *r_c_strs = create_get_element_ptr(builder, runtime,
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

template<unsigned n>
Value *get_jump_table(IRBuilder<> &builder, Value *runtime, uint16_t id)
{
#ifdef MOZVM_USE_JMPTBL
    Value *r_c_jumps = create_get_element_ptr(builder, runtime,
            builder.getInt64(0),
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
            builder.getInt32(12),
#else
            builder.getInt32(11),
#endif /*MOZVM_USE_DYNAMIC_DEACTIVATION*/
            builder.getInt32(4 + n));
    Value *jumps_head = builder.CreateLoad(r_c_jumps);
    return builder.CreateGEP(jumps_head, builder.getInt64(id));
#else
    return nullptr;
#endif /*MOZVM_USE_JMPTBL*/
}


class JitContext {
private:
    FunctionType *create_bitset_get(IRBuilder<> &builder, Module *M);
#ifdef MOZVM_USE_JMPTBL
    template<unsigned n>
    FunctionType *create_jump_table_index(IRBuilder<> &builder, Module *M);
#endif
    FunctionType *create_pstring_length(IRBuilder<> &builder, Module *M);
    FunctionType *create_pstring_startswith(IRBuilder<> &builder, Module *M);
    FunctionType *create_ast_save_tx(IRBuilder<> &builder, Module *M);
    FunctionType *create_ast_get_last_linked_node(IRBuilder<> &builder, Module *M);
    FunctionType *create_symtable_savepoint(IRBuilder<> &builder, Module *M);
    FunctionType *create_symtable_rollback(IRBuilder<> &builder, Module *M);

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
    StructType *memoentryType;
    Type *mozposType;
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
    StructType *memopointType;
#endif
    StructType *runtimeType;
    FunctionType *jitfuncType;

    FunctionType *bitsetgetType;
#ifdef MOZVM_USE_JMPTBL
    FunctionType *tbljmp1idxType;
    FunctionType *tbljmp2idxType;
    FunctionType *tbljmp3idxType;
#endif
    FunctionType *pstrlenType;
    FunctionType *pstrstwithType;

    FunctionType *astsaveType;
    FunctionType *astrollbackType;
    FunctionType *astcommitType;
    FunctionType *astreplaceType;
    FunctionType *astcaptureType;
    FunctionType *astnewType;
    FunctionType *astpopType;
    FunctionType *astpushType;
    FunctionType *astswapType;
    FunctionType *asttagType;
    FunctionType *astlinkType;
    FunctionType *astlastnodeType;

    FunctionType *memosetType;
    FunctionType *memofailType;
    FunctionType *memogetType;

    FunctionType *tblsaveType;
    FunctionType *tblrollbackType;

#ifdef MOZVM_USE_JMPTBL
    template<unsigned n>
    StructType *jump_table_type();
#endif

    ~JitContext() {};
    JitContext();
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
    jmptbl1Type = define_struct_type("jump_table1_t",
            ArrayType::get(bsetType, 1),
            ArrayType::get(I32Ty, 2)
            );
    jmptbl2Type = define_struct_type("jump_table2_t",
            ArrayType::get(bsetType, 2),
            ArrayType::get(I32Ty, 4)
            );
    jmptbl3Type = define_struct_type("jump_table3_t",
            ArrayType::get(bsetType, 3),
            ArrayType::get(I32Ty, 8)
            );
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
    Type *nodePtrTy = nodeType->getPointerTo();
    StructType *astlogType = StructType::create(Ctx, "AstLog");
    StructType *astlogarrayType = define_struct_type("ARRAY_AstLog_t",
            I32Ty, // size
            I32Ty, // capacity
            astlogType->getPointerTo() // list
            );
    astType = define_struct_type("AstMachine",
            astlogarrayType, // logs
            nodePtrTy, // last-linked
            nodePtrTy, // parsed
            I8PtrTy // source
            );
    Type *astPtrTy = astType->getPointerTo();

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
    Type *memoPtrTy = memoType->getPointerTo();
    memoentryType = define_struct_type("MemoEntry_t",
            I64Ty, // hash
            nodePtrTy, // result(failed)
            I32Ty, // consumed
            I32Ty // state
            );
    Type *memoentryPtrTy = memoentryType->getPointerTo();
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
    mozposType = I8PtrTy;
#else
    mozposType = I64Ty;
#endif
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
    memopointType = StructType::create(Ctx, "MemoPoint");
#endif
    StructType *ntermentryType = StructType::create(Ctx, "mozvm_nterm_entry_t");
    runtimeType = define_struct_type("moz_runtime_t",
            astPtrTy,
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
            mozposType, // cur
            I8PtrTy, // jit_context
            ntermentryType->getPointerTo(), // nterm_entry
            constantType,
            ArrayType::get(I64Ty, 1)
            );

    jitfuncType = get_function_type(I8Ty,
            runtimeType->getPointerTo(), I8PtrTy, I16Ty);

    define_struct_body(ntermentryType,
            I8PtrTy, // begin
            I8PtrTy, // end
            I32Ty, // call_counter
            jitfuncType->getPointerTo() //compiled_code
            );

    for(int i = 0; i < ARRAY_SIZE(symbols); i++) {
        const Symbol *sym = symbols + i;
        sys::DynamicLibrary::AddSymbol(sym->name, sym->func);
    }

    astrollbackType = get_function_type(VoidTy, astPtrTy, I64Ty);
    astcommitType   = get_function_type(VoidTy, astPtrTy, I16Ty, I64Ty);
    astreplaceType  = get_function_type(VoidTy, astPtrTy, I8PtrTy);
    astcaptureType  = get_function_type(VoidTy, astPtrTy, mozposType);
    astnewType      = astcaptureType;
    astpopType      = get_function_type(VoidTy, astPtrTy, I16Ty);
    astpushType     = get_function_type(VoidTy, astPtrTy);
    astswapType     = get_function_type(VoidTy, astPtrTy, mozposType, I16Ty);
    asttagType      = astreplaceType;
    astlinkType     = get_function_type(VoidTy, astPtrTy, I16Ty, nodePtrTy);
    memosetType  = get_function_type(I32Ty,
            memoPtrTy, mozposType, I32Ty, nodePtrTy, I32Ty, I32Ty);
    memofailType = get_function_type(I32Ty, memoPtrTy, mozposType, I32Ty);
    memogetType  = get_function_type(memoentryPtrTy,
            memoPtrTy, mozposType, I32Ty, I8Ty);

    Module *M = new Module("top", Ctx);
    bitsetgetType   = create_bitset_get(builder, M);
#ifdef MOZVM_USE_JMPTBL
    tbljmp1idxType = create_jump_table_index<1>(builder, M);
    tbljmp2idxType = create_jump_table_index<2>(builder, M);
    tbljmp3idxType = create_jump_table_index<3>(builder, M);
#endif
    pstrlenType     = create_pstring_length(builder, M);
    pstrstwithType  = create_pstring_startswith(builder, M);
    astsaveType     = create_ast_save_tx(builder, M);
    astlastnodeType = create_ast_get_last_linked_node(builder, M);
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

    F = Function::Create(funcTy, Function::InternalLinkage, "bitset_get", M);

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
template<unsigned n>
StructType *JitContext::jump_table_type()
{
    switch(n) {
        case 1: {
            return jmptbl1Type;
        }
        case 2: {
            return jmptbl2Type;
        }
        case 3: {
            return jmptbl3Type;
        }
        default: {
            return nullptr;
        }
    }
    return nullptr;
}

template<unsigned n>
FunctionType *JitContext::create_jump_table_index(IRBuilder<> &builder, Module *M)
{
    LLVMContext& Ctx = M->getContext();
    Type *I8Ty  = builder.getInt8Ty();
    Type *I32Ty = builder.getInt32Ty();
    Type *jmptblPtrTy = jump_table_type<n>()->getPointerTo();
    FunctionType *funcTy = get_function_type(I32Ty, jmptblPtrTy, I8Ty);
    Function *F;
    Constant *i32_0 = builder.getInt32(0);
    Constant *i64_0 = builder.getInt64(0);

    Constant *f_bitsetget = M->getOrInsertFunction("bitset_get", bitsetgetType);

    string func_name = string("jump_table") + to_string(n) + string("_index");
    F = Function::Create(funcTy, Function::InternalLinkage, func_name, M);

    Function::arg_iterator arg_iter = F->arg_begin();
    Value *tbl = arg_iter++;
    Value *ch  = arg_iter++;

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

    Value *ch_    = builder.CreateZExt(ch, I32Ty);
    Value *idx;
    for(int i = 0; i < n; i++) {
        Value *tbl_set = create_get_element_ptr(builder, tbl,
                i64_0, i32_0, builder.getInt64(i));
        Value *idx_ = create_call_inst(builder, f_bitsetget, tbl_set, ch_);
        if(i == 0) {
            idx = idx_;
        }
        else {
            Value *idx_shl = builder.CreateShl(idx_, builder.getInt32(i));
            idx = builder.CreateOr(idx, idx_shl);
        }
    }
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

    F = Function::Create(funcTy, Function::InternalLinkage, "pstring_length", M);

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

    F = Function::Create(funcTy, Function::InternalLinkage, "pstring_starts_with", M);

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

    F = Function::Create(funcTy, Function::InternalLinkage, "ast_save_tx", M);

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

FunctionType *JitContext::create_ast_get_last_linked_node(IRBuilder<> &builder, Module *M)
{
    LLVMContext& Ctx = M->getContext();
    Type *nodePtrTy = nodeType->getPointerTo();
    Type *astPtrTy  = astType->getPointerTo();
    FunctionType *funcTy = get_function_type(nodePtrTy, astPtrTy);
    Function *F;
    Constant *i32_1 = builder.getInt32(1);
    Constant *i64_0 = builder.getInt64(0);

    F = Function::Create(funcTy, Function::InternalLinkage, "ast_get_last_linked_node", M);

    Function::arg_iterator arg_iter = F->arg_begin();
    Value *ast = arg_iter++;

    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    builder.SetInsertPoint(entryBB);

    Value *Ptr    = create_get_element_ptr(builder, ast, i64_0, i32_1);
    Value *Linked = builder.CreateLoad(Ptr);
    builder.CreateRet(Linked);
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

    F = Function::Create(funcTy, Function::InternalLinkage, "symtable_savepoint", M);

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

    F = Function::Create(funcTy, Function::InternalLinkage, "symtable_rollback", M);

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
    runtime->jit_context = reinterpret_cast<void *>(new JitContext());
}

void mozvm_jit_reset(moz_runtime_t *runtime)
{
    for (int i = 0; i < runtime->C.nterm_size; i++) {
        runtime->nterm_entry[i].call_counter  = 0;
        runtime->nterm_entry[i].compiled_code = mozvm_jit_call_nterm;
    }
    delete get_context(runtime);
    runtime->jit_context = reinterpret_cast<void *>(new JitContext());
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

moz_jit_func_t mozvm_jit_compile(moz_runtime_t *runtime, mozvm_nterm_entry_t *e)
{
    JitContext *_ctx = get_context(runtime);
    uint16_t nterm = e - runtime->nterm_entry;

    if(mozvm_nterm_is_already_compiled(e)) {
        return e->compiled_code;
    }

    LLVMContext& Ctx = getGlobalContext();
    IRBuilder<> builder(Ctx);
    Module *M = new Module(runtime->C.nterms[nterm], Ctx);
    _ctx->EE->addModule(unique_ptr<Module>(M));

    PointerType *nodePtrTy = _ctx->nodeType->getPointerTo();
    Constant *nullnode     = Constant::getNullValue(nodePtrTy);

    Constant *f_bitsetget   = M->getOrInsertFunction("bitset_get", _ctx->bitsetgetType);
    Constant *f_tbljmp1idx;
    Constant *f_tbljmp2idx;
    Constant *f_tbljmp3idx;
#ifdef MOZVM_USE_JMPTBL
    f_tbljmp1idx  = M->getOrInsertFunction("jump_table1_index", _ctx->tbljmp1idxType);
    f_tbljmp2idx  = M->getOrInsertFunction("jump_table2_index", _ctx->tbljmp2idxType);
    f_tbljmp3idx  = M->getOrInsertFunction("jump_table3_index", _ctx->tbljmp3idxType);
#endif
    Constant *f_pstrlen     = M->getOrInsertFunction("pstring_length", _ctx->pstrlenType);
    Constant *f_pstrstwith  = M->getOrInsertFunction("pstring_starts_with", _ctx->pstrstwithType);

    Constant *f_astsave     = M->getOrInsertFunction("ast_save_tx", _ctx->astsaveType);
    Constant *f_astrollback = M->getOrInsertFunction("ast_rollback_tx", _ctx->astrollbackType);
    Constant *f_astcommit   = M->getOrInsertFunction("ast_commit_tx", _ctx->astcommitType);
    Constant *f_astreplace  = M->getOrInsertFunction("ast_log_replace", _ctx->astreplaceType);
    Constant *f_astcapture  = M->getOrInsertFunction("ast_log_capture", _ctx->astcaptureType);
    Constant *f_astnew      = M->getOrInsertFunction("ast_log_new", _ctx->astnewType);
    Constant *f_astpop      = M->getOrInsertFunction("ast_log_pop", _ctx->astpopType);
    Constant *f_astpush     = M->getOrInsertFunction("ast_log_push", _ctx->astpushType);
    Constant *f_astswap     = M->getOrInsertFunction("ast_log_swap", _ctx->astswapType);
    Constant *f_asttag      = M->getOrInsertFunction("ast_log_tag", _ctx->asttagType);
    Constant *f_astlink     = M->getOrInsertFunction("ast_log_link", _ctx->astlinkType);
    Constant *f_astlastnode = M->getOrInsertFunction("ast_get_last_linked_node", _ctx->astlastnodeType);

    PointerType *memoentryPtrTy = _ctx->memoentryType->getPointerTo();
    Constant *nullentry         = Constant::getNullValue(memoentryPtrTy);
    Constant *memo_entry_failed = builder.getInt64(UINTPTR_MAX);
    Constant *f_memoset         = M->getOrInsertFunction("memo_set", _ctx->memosetType);
    Constant *f_memofail        = M->getOrInsertFunction("memo_fail", _ctx->memofailType);
    Constant *f_memoget         = M->getOrInsertFunction("memo_get", _ctx->memogetType);

    Constant *f_tblsave     = M->getOrInsertFunction("symtable_savepoint", _ctx->tblsaveType);
    Constant *f_tblrollback = M->getOrInsertFunction("symtable_rollback", _ctx->tblrollbackType);

    Function *F = Function::Create(_ctx->jitfuncType,
            Function::ExternalLinkage,
            runtime->C.nterms[nterm], M);

    Function::arg_iterator arg_iter=F->arg_begin();
    Value *runtime_ = arg_iter++;
    Value *str = arg_iter++;
    Value *nterm_ = arg_iter++;

    vector<BasicBlock *> failjumpList;
    unordered_map<moz_inst_t *, BasicBlock *> BBMap;
    BasicBlock *entryBB = BasicBlock::Create(Ctx, "entrypoint", F);
    BasicBlock *failBB  = BasicBlock::Create(Ctx, "fail");
    BasicBlock *retBB   = BasicBlock::Create(Ctx, "success");
    BasicBlock *errBB   = BasicBlock::Create(Ctx, "error");
    BasicBlock *unreachableBB = BasicBlock::Create(Ctx, "switch.default");

    moz_inst_t *p = e->begin;
    while (p < e->end) {
        uint8_t opcode = *p;
        unsigned shift = opcode_size(opcode);
        if(opcode == Alt || opcode == Jump) {
            mozaddr_t jump = *((mozaddr_t *)(p+1));
            moz_inst_t *dest = p + shift + jump;
            if(BBMap.find(dest) == BBMap.end()) {
                BasicBlock *label = get_jump_destination(e, dest, failBB);
                if(!label) {
                    F->eraseFromParent();
                    return NULL;
                }
                BBMap[dest] = label;
                if(opcode == Alt) {
                    failjumpList.push_back(label);
                }
            }
        }
        else if(opcode == Call) {
            mozaddr_t next = *((mozaddr_t *)(p + 1 + sizeof(uint16_t)));
            moz_inst_t *dest = p + shift + next;
            if(BBMap.find(dest) == BBMap.end()) {
                BasicBlock *label = get_jump_destination(e, dest, failBB);
                if(!label) {
                    F->eraseFromParent();
                    return NULL;
                }
                BBMap[dest] = label;
            }
        }
        else if(opcode == Lookup) {
            moz_inst_t *pc   = p + 1 + sizeof(uint8_t) + sizeof(uint16_t);
            mozaddr_t skip = *((mozaddr_t *)(pc));
            moz_inst_t *dest = p + shift + skip;
            if(BBMap.find(dest) == BBMap.end()) {
                BasicBlock *label = get_jump_destination(e, dest, failBB);
                if(!label) {
                    F->eraseFromParent();
                    return NULL;
                }
                BBMap[dest] = label;
            }
        }
        else if(opcode == TLookup) {
            moz_inst_t *pc   = p + 1 + sizeof(uint8_t) + sizeof(TAG_t) + sizeof(uint16_t);
            mozaddr_t skip = *((mozaddr_t *)(pc));
            moz_inst_t *dest = p + shift + skip;
            if(BBMap.find(dest) == BBMap.end()) {
                BasicBlock *label = get_jump_destination(e, dest, failBB);
                if(!label) {
                    F->eraseFromParent();
                    return NULL;
                }
                BBMap[dest] = label;
            }
        }
#ifdef MOZVM_USE_JMPTBL
        else if(opcode == TblJump1 || opcode == TblJump2 || opcode == TblJump3) {
            uint16_t tblId = *((uint16_t *)(p+1));
            moz_inst_t *jmpoffset = p + shift;
            int *jumps;
            int size;
            switch(opcode) {
                case TblJump1: {
                    jumps = runtime->C.jumps1[tblId].jumps;
                    size = 2;
                    break;
                }
                case TblJump2: {
                    jumps = runtime->C.jumps2[tblId].jumps;
                    size = 4;
                    break;
                }
                case TblJump3: {
                    jumps = runtime->C.jumps3[tblId].jumps;
                    size = 8;
                    break;
                }
            }
            for(int i = 0; i < size; i++) {
                if(jumps[i] != INT_MAX) {
                    moz_inst_t *dest = jmpoffset + jumps[i];
                    if(BBMap.find(dest) == BBMap.end()) {
                        BasicBlock *label = get_jump_destination(e, dest, failBB);
                        if(!label) {
                            F->eraseFromParent();
                            return NULL;
                        }
                        BBMap[dest] = label;
                    }
                }
            }
        }
#endif /*MOZVM_USE_JMPTBL*/
        p += opcode_size(opcode);
    }

    failjumpList.push_back(errBB);

    builder.SetInsertPoint(entryBB);
    Constant *i8_0  = builder.getInt8(0);
    Constant *i32_0 = builder.getInt32(0);
    Constant *i32_1 = builder.getInt32(1);
    Constant *i32_2 = builder.getInt32(2);
    Constant *i64_0 = builder.getInt64(0);

    Value *ast_   = create_get_element_ptr(builder, runtime_, i64_0, i32_0);
    Value *ast    = builder.CreateLoad(ast_);
    Value *tbl_   = create_get_element_ptr(builder, runtime_, i64_0, i32_1);
    Value *tbl    = builder.CreateLoad(tbl_);
    Value *memo_  = create_get_element_ptr(builder, runtime_, i64_0, i32_2);
    Value *memo   = builder.CreateLoad(memo_);
    Value *head_  = create_get_element_ptr(builder, runtime_, i64_0, builder.getInt32(3));
    Value *tail_  = create_get_element_ptr(builder, runtime_, i64_0, builder.getInt32(4));
    Value *tail   = builder.CreateLoad(tail_);

    Value *sp = create_get_element_ptr(builder, runtime_, i64_0, builder.getInt32(6));
    Value *fp = create_get_element_ptr(builder, runtime_, i64_0, builder.getInt32(7));
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
    Value *cur = create_get_element_ptr(builder, runtime_, i64_0, builder.getInt32(9));
#else
    Value *cur = create_get_element_ptr(builder, runtime_, i64_0, builder.getInt32(8));
#endif

    {
        BlockAddress *addr = BlockAddress::get(F, errBB);
        Value *pos = builder.CreateLoad(cur);
        Value *ast_tx = create_call_inst(builder, f_astsave, ast);
        Value *saved  = create_call_inst(builder, f_tblsave, tbl);
        stack_push_frame(builder, sp, fp, pos, addr, ast_tx, saved);
    }

    BasicBlock *currentBB = entryBB;
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
        else if(currentBB->getTerminator() != nullptr) {
            BasicBlock *newBB = BasicBlock::Create(Ctx, "unreachable", F);
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
                Value *pos = builder.CreateLoad(cur);
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
                moz_inst_t *pc   = p + 1;
                uint16_t nterm   = *((uint16_t *)(pc));
                pc += sizeof(uint16_t);
                mozaddr_t next   = *((mozaddr_t *)(pc));
                moz_inst_t *dest = p + shift + next;
                Constant *ID = builder.getInt16(nterm);

                Value *func;
                if(mozvm_nterm_is_already_compiled(runtime->nterm_entry + nterm)) {
                    func = M->getOrInsertFunction(runtime->C.nterms[nterm], _ctx->jitfuncType);
                }
                else {
                    func = get_callee_function(builder, runtime_, nterm);
                }
                Value *result = create_call_inst(builder, func, runtime_, str, ID);
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
                Value *ifcond = builder.CreateICmpEQ(prev_pos, pos);
                builder.CreateCondBr(ifcond, failBB, next);

                builder.SetInsertPoint(next);
                builder.CreateStore(pos, prev_pos_);
                Value *ast_tx = create_call_inst(builder, f_astsave, ast);
                builder.CreateStore(ast_tx, ast_tx_);
                Value *saved = create_call_inst(builder, f_tblsave, tbl);
                builder.CreateStore(saved, saved_);
                currentBB = next;
                break;
            }
            CASE_(Byte) {
                BasicBlock *succ = BasicBlock::Create(Ctx, "byte.succ", F);
                uint8_t ch = (uint8_t)*(p+1);
                Constant *C = builder.getInt8(ch);

                Value *pos = builder.CreateLoad(cur);
                Value *current = get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *cond = builder.CreateICmpNE(character, C);
                builder.CreateCondBr(cond, failBB, succ);

                builder.SetInsertPoint(succ);
                Value *nextpos = consume(builder, pos);
                builder.CreateStore(nextpos, cur);
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

                Value *pos = builder.CreateLoad(cur);
                Value *current = get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *cond = builder.CreateICmpEQ(character, C);
                builder.CreateCondBr(cond, obody, oend);

                builder.SetInsertPoint(obody);
                Value *nextpos = consume(builder, pos);
                builder.CreateStore(nextpos, cur);
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

                Value *pos = builder.CreateLoad(cur);
                Value *current = get_current(builder, str, pos);
                Value *cond = builder.CreateICmpEQ(current, tail);
                builder.CreateCondBr(cond, failBB, succ);

                builder.SetInsertPoint(succ);
                Value *nextpos = consume(builder, pos);
                builder.CreateStore(nextpos, cur);
                currentBB = succ;
                break;
            }
            CASE_(NAny) {
                BasicBlock *succ = BasicBlock::Create(Ctx, "nany.succ", F);

                Value *pos = builder.CreateLoad(cur);
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
                Value *pos = builder.CreateLoad(cur);
                Value *current = get_current(builder, str, pos);
                Value *result = create_call_inst(builder, f_pstrstwith, current, str, len);
                Value *cond = builder.CreateICmpEQ(result, i32_0);
                builder.CreateCondBr(cond, failBB, succ);

                builder.SetInsertPoint(succ);
                Value *len_ = builder.CreateZExt(len, builder.getInt64Ty());
                Value *nextpos = consume_n(builder, pos, len_);
                builder.CreateStore(nextpos, cur);
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
                Value *pos = builder.CreateLoad(cur);
                Value *current = get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *index = builder.CreateZExt(character, builder.getInt32Ty());
                Value *result = create_call_inst(builder, f_bitsetget, set, index);
                Value *cond = builder.CreateICmpEQ(result, i32_0);
                builder.CreateCondBr(cond, failBB, succ);

                builder.SetInsertPoint(succ);
                Value *nextpos = consume(builder, pos);
                builder.CreateStore(nextpos, cur);
                currentBB = succ;
                break;
            }
            CASE_(NSet) {
                BasicBlock *succ = BasicBlock::Create(Ctx, "nset.succ", F);
                BITSET_t setId = *((BITSET_t *)(p+1));

                Value *set = get_bitset_ptr(builder, runtime_, setId);
                Value *pos = builder.CreateLoad(cur);
                Value *current = get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *index = builder.CreateZExt(character, builder.getInt32Ty());
                Value *result = create_call_inst(builder, f_bitsetget, set, index);
                Value *cond = builder.CreateICmpNE(result, i32_0);
                builder.CreateCondBr(cond, failBB, succ);

                builder.SetInsertPoint(succ);
                currentBB = succ;
                break;
            }
            CASE_(OSet) {
                BasicBlock *obody = BasicBlock::Create(Ctx, "oset.body", F);
                BasicBlock *oend  = BasicBlock::Create(Ctx, "oset.end", F);
                BITSET_t setId = *((BITSET_t *)(p+1));

                Value *set = get_bitset_ptr(builder, runtime_, setId);
                Value *pos = builder.CreateLoad(cur);
                Value *current = get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *index = builder.CreateZExt(character, builder.getInt32Ty());
                Value *result = create_call_inst(builder, f_bitsetget, set, index);
                Value *cond = builder.CreateICmpNE(result, i32_0);
                builder.CreateCondBr(cond, obody, oend);

                builder.SetInsertPoint(obody);
                Value *nextpos = consume(builder, pos);
                builder.CreateStore(nextpos, cur);
                builder.CreateBr(oend);

                builder.SetInsertPoint(oend);
                currentBB = oend;
                break;
            }
            CASE_(RSet) {
                BasicBlock *rcond = BasicBlock::Create(Ctx, "rset.cond", F);
                BasicBlock *rbody = BasicBlock::Create(Ctx, "rset.body", F);
                BasicBlock *rend  = BasicBlock::Create(Ctx, "rset.end",  F);

                BITSET_t setId = *((BITSET_t *)(p+1));
                Value *set = get_bitset_ptr(builder, runtime_, setId);
                Value *firstpos = builder.CreateLoad(cur);
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
                builder.CreateStore(pos, cur);
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
                uint16_t tblId = *((uint16_t *)(p+1));
                moz_inst_t *jmpoffset = p + shift;
                int *jumps = runtime->C.jumps1[tblId].jumps;

                Value *tbl = get_jump_table<1>(builder, runtime_, tblId);
                Value *pos = builder.CreateLoad(cur);
                Value *current = get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *idx = create_call_inst(builder, f_tbljmp1idx, tbl, character);

                SwitchInst *jump = builder.CreateSwitch(idx, unreachableBB, 2);
                for(int i = 0; i < 2; i++) {
                    if(jumps[i] != INT_MAX) {
                        moz_inst_t *dest = jmpoffset + jumps[i];
                        // assert(nterm_has_inst(e, dest));
                        jump->addCase(builder.getInt32(i), BBMap[dest]);
                    }
                    else {
                        jump->addCase(builder.getInt32(i), unreachableBB);
                    }
                }
                break;
            }
            CASE_(TblJump2) {
                uint16_t tblId = *((uint16_t *)(p+1));
                moz_inst_t *jmpoffset = p + shift;
                int *jumps = runtime->C.jumps2[tblId].jumps;

                Value *tbl = get_jump_table<2>(builder, runtime_, tblId);
                Value *pos = builder.CreateLoad(cur);
                Value *current = get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *idx = create_call_inst(builder, f_tbljmp2idx, tbl, character);

                SwitchInst *jump = builder.CreateSwitch(idx, unreachableBB, 4);
                for(int i = 0; i < 4; i++) {
                    if(jumps[i] != INT_MAX) {
                        moz_inst_t *dest = jmpoffset + jumps[i];
                        // assert(nterm_has_inst(e, dest));
                        jump->addCase(builder.getInt32(i), BBMap[dest]);
                    }
                    else {
                        jump->addCase(builder.getInt32(i), unreachableBB);
                    }
                }
                break;
            }
            CASE_(TblJump3) {
                uint16_t tblId = *((uint16_t *)(p+1));
                moz_inst_t *jmpoffset = p + shift;
                int *jumps = runtime->C.jumps3[tblId].jumps;

                Value *tbl = get_jump_table<3>(builder, runtime_, tblId);
                Value *pos = builder.CreateLoad(cur);
                Value *current = get_current(builder, str, pos);
                Value *character = builder.CreateLoad(current);
                Value *idx = create_call_inst(builder, f_tbljmp3idx, tbl, character);

                SwitchInst *jump = builder.CreateSwitch(idx, unreachableBB, 8);
                for(int i = 0; i < 8; i++) {
                    if(jumps[i] != INT_MAX) {
                        moz_inst_t *dest = jmpoffset + jumps[i];
                        // assert(nterm_has_inst(e, dest));
                        jump->addCase(builder.getInt32(i), BBMap[dest]);
                    }
                    else {
                        jump->addCase(builder.getInt32(i), unreachableBB);
                    }
                }
                break;
            }
            CASE_(Lookup) {
                BasicBlock *hit  = BasicBlock::Create(Ctx, "lookup.hit",  F);
                BasicBlock *succ = BasicBlock::Create(Ctx, "lookup.succ",  F);
                BasicBlock *miss = BasicBlock::Create(Ctx, "lookup.miss", F);

                moz_inst_t *pc   = p + 1;
                uint8_t state    = (uint8_t)*(pc);
                pc += sizeof(uint8_t);
                uint16_t memoId  = *((uint16_t *)(pc));
                pc += sizeof(uint16_t);
                mozaddr_t skip   = *((mozaddr_t *)(pc));
                moz_inst_t *dest = p + shift + skip;
                Constant *state_    = builder.getInt8(state);
                Constant *memoId_   = builder.getInt32(memoId);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
                asm volatile("int3");
#endif
                Value *pos     = builder.CreateLoad(cur);
                Value *entry   = create_call_inst(builder, f_memoget, memo, pos, memoId_, state_);
                Value *hitcond = builder.CreateICmpNE(entry, nullentry);
                builder.CreateCondBr(hitcond, hit, miss);

                builder.SetInsertPoint(hit);
                Value *result_  = create_get_element_ptr(builder, entry, i64_0, i32_1);
                Value *result   = builder.CreateLoad(result_);
                Value *failed   = builder.CreatePtrToInt(result, builder.getInt64Ty());
                Value *failcond = builder.CreateICmpEQ(failed, memo_entry_failed);
                builder.CreateCondBr(failcond, failBB, succ);

                builder.SetInsertPoint(succ);
                Value *consumed_ = create_get_element_ptr(builder, entry, i64_0, i32_2);
                Value *consumed  = builder.CreateLoad(consumed_);
                Value *newpos    = consume_n(builder, pos, consumed);
                builder.CreateStore(newpos, cur);
                builder.CreateBr(BBMap[dest]);

                builder.SetInsertPoint(miss);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
#endif
                currentBB = miss;
                break;
            }
            CASE_(Memo) {
                moz_inst_t *pc   = p + 1;
                uint8_t state    = (uint8_t)*(pc);
                pc += sizeof(uint8_t);
                uint16_t memoId  = *((uint16_t *)(pc));
                Constant *state_    = builder.getInt32(state);
                Constant *memoId_   = builder.getInt32(memoId);

                Value *startpos;
                Value *next;
                Value *ast_tx;
                Value *saved;
                stack_pop_frame(builder, sp, fp, &startpos, &next, &ast_tx, &saved);
                Value *endpos  = builder.CreateLoad(cur);
                Value *length  = get_length(builder, startpos, endpos);
                Value *length_ = builder.CreateTrunc(length, builder.getInt32Ty());
                create_call_inst(builder, f_memoset,
                        memo, startpos, memoId_, nullnode, length_, state_);
                break;
            }
            CASE_(MemoFail) {
                moz_inst_t *pc   = p + 1;
                // uint8_t state    = (uint8_t)*(pc);
                pc += sizeof(uint8_t);
                uint16_t memoId  = *((uint16_t *)(pc));
                // Constant *state_    = builder.getInt8(state);
                Constant *memoId_   = builder.getInt32(memoId);

                Value *pos = builder.CreateLoad(cur);
                create_call_inst(builder, f_memofail, memo, pos, memoId_);
                builder.CreateBr(failBB);
                break;
            }
            CASE_(TPush) {
                create_call_inst(builder, f_astpush, ast);
                break;
            }
            CASE_(TPop) {
                TAG_t tagId = *((TAG_t *)(p+1));

                Value *_tagId = get_tag_id(builder, runtime_, tagId);
                create_call_inst(builder, f_astpop, ast, _tagId);
                break;
            }
            CASE_(TLeftFold) {
                moz_inst_t *pc   = p + 1;
                uint8_t shift    = (uint8_t)*(pc);
                pc += sizeof(uint8_t);
                TAG_t tagId      = *((TAG_t *)(pc));
                Constant *shift_ = builder.getInt64(shift);

                Value *_tagId  = get_tag_id(builder, runtime_, tagId);
                Value *pos     = builder.CreateLoad(cur);
                Value *swappos = consume_n(builder, pos, shift_);
                create_call_inst(builder, f_astswap, ast, swappos, _tagId);
                break;
            }
            CASE_(TNew) {
                uint8_t shift    = (uint8_t)*(p+1);
                Constant *shift_ = builder.getInt64(shift);

                Value *pos    = builder.CreateLoad(cur);
                Value *newpos = consume_n(builder, pos, shift_);
                create_call_inst(builder, f_astnew, ast, newpos);
                break;
            }
            CASE_(TCapture) {
                uint8_t shift    = (uint8_t)*(p+1);
                Constant *shift_ = builder.getInt64(shift);

                Value *pos        = builder.CreateLoad(cur);
                Value *capturepos = consume_n(builder, pos, shift_);
                create_call_inst(builder, f_astcapture, ast, capturepos);
                break;
            }
            CASE_(TTag) {
                TAG_t tagId = *((TAG_t *)(p+1));

                Value *tag = get_tag_ptr(builder, runtime_, tagId);
                create_call_inst(builder, f_asttag, ast, tag);
                break;
            }
            CASE_(TReplace) {
                STRING_t strId = *((STRING_t *)(p+1));

                Value *str = get_string_ptr(builder, runtime_, strId);
                create_call_inst(builder, f_astreplace, ast, str);
                break;
            }
            CASE_(TStart) {
                Value *ast_tx = create_call_inst(builder, f_astsave, ast);
                stack_push(builder, sp, ast_tx);
                break;
            }
            CASE_(TCommit) {
                TAG_t tagId = *((TAG_t *)(p+1));

                Value *_tagId = get_tag_id(builder, runtime_, tagId);
                Value *tx     = stack_pop(builder, sp);
                create_call_inst(builder, f_astcommit, ast, _tagId, tx);
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

                moz_inst_t *pc   = p + 1;
                uint8_t state    = (uint8_t)*(pc);
                pc += sizeof(uint8_t);
                TAG_t tagId = *((TAG_t *)(pc));
                pc += sizeof(TAG_t);
                uint16_t memoId  = *((uint16_t *)(pc));
                pc += sizeof(uint16_t);
                mozaddr_t skip   = *((mozaddr_t *)(pc));
                moz_inst_t *dest = p + shift + skip;
                Constant *state_    = builder.getInt8(state);
                Constant *memoId_   = builder.getInt32(memoId);

                Value *_tagId = get_tag_id(builder, runtime_, tagId);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
                asm volatile("int3");
#endif
                Value *pos     = builder.CreateLoad(cur);
                Value *entry   = create_call_inst(builder, f_memoget, memo, pos, memoId_, state_);
                Value *hitcond = builder.CreateICmpNE(entry, nullentry);
                builder.CreateCondBr(hitcond, hit, miss);

                builder.SetInsertPoint(hit);
                Value *result_  = create_get_element_ptr(builder, entry, i64_0, i32_1);
                Value *result   = builder.CreateLoad(result_);
                Value *failed   = builder.CreatePtrToInt(result, builder.getInt64Ty());
                Value *failcond = builder.CreateICmpEQ(failed, memo_entry_failed);
                builder.CreateCondBr(failcond, failBB, succ);

                builder.SetInsertPoint(succ);
                Value *consumed_ = create_get_element_ptr(builder, entry, i64_0, i32_2);
                Value *consumed  = builder.CreateLoad(consumed_);
                Value *newpos    = consume_n(builder, pos, consumed);
                builder.CreateStore(newpos, cur);
                create_call_inst(builder, f_astlink, ast, _tagId, result);
                builder.CreateBr(BBMap[dest]);

                builder.SetInsertPoint(miss);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
#endif
                currentBB = miss;
                break;
            }
            CASE_(TMemo) {
                moz_inst_t *pc   = p + 1;
                uint8_t state    = (uint8_t)*(pc);
                pc += sizeof(uint8_t);
                uint16_t memoId  = *((uint16_t *)(pc));
                Constant *state_    = builder.getInt32(state);
                Constant *memoId_   = builder.getInt32(memoId);

                Value *startpos;
                Value *next;
                Value *ast_tx;
                Value *saved;
                stack_pop_frame(builder, sp, fp, &startpos, &next, &ast_tx, &saved);
                Value *endpos  = builder.CreateLoad(cur);
                Value *length  = get_length(builder, startpos, endpos);
                Value *length_ = builder.CreateTrunc(length, builder.getInt32Ty());
                Value *node    = create_call_inst(builder, f_astlastnode, ast);
                create_call_inst(builder, f_memoset,
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
        Value *ifcond = builder.CreateICmpULT(pos_, pos);
        builder.CreateCondBr(ifcond, posback, rollback);

        builder.SetInsertPoint(posback);
        Value *head = builder.CreateLoad(head_);
        Value *selectcond = builder.CreateICmpULT(head, pos);
        Value *newhead = builder.CreateSelect(selectcond, pos, head);
        builder.CreateStore(newhead, head_);
        builder.CreateStore(pos_, cur);
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
        builder.CreateRet(i8_0);
    }

    errBB->insertInto(F);
    builder.SetInsertPoint(errBB);
    builder.CreateRet(builder.getInt8(1));

    unreachableBB->insertInto(F);
    builder.SetInsertPoint(unreachableBB);
    builder.CreateUnreachable();

    // M->dump();
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
