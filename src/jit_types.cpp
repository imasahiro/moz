#include "llvm/IR/TypeBuilder.h"

namespace llvm {
template<typename R, typename A1, typename A2, typename A3, typename A4,
         typename A5, typename A6, bool cross>
class TypeBuilder<R(A1, A2, A3, A4, A5, A6), cross> {
public:
  static FunctionType *get(LLVMContext &Context) {
    Type *params[] = {
      TypeBuilder<A1, cross>::get(Context),
      TypeBuilder<A2, cross>::get(Context),
      TypeBuilder<A3, cross>::get(Context),
      TypeBuilder<A4, cross>::get(Context),
      TypeBuilder<A5, cross>::get(Context),
      TypeBuilder<A6, cross>::get(Context),
    };
    return FunctionType::get(TypeBuilder<R, cross>::get(Context),
                             params, false);
  }
};

static Type *NodeType = NULL;
static Type *MemoType = NULL;
static Type *SymTblType = NULL;
static Type *BitSetType = NULL;
static Type *AstMachineType = NULL;
static Type *MemoEntryType = NULL;

static Type *JmpTbl1Type = NULL;
static Type *JmpTbl2Type = NULL;
static Type *JmpTbl3Type = NULL;

static Type *MozRuntimeType = NULL;
static Type *MozConstsType = NULL;
static Type *NTermEntryType = NULL;

template<bool cross>
class TypeBuilder<Node, cross> {
public:
  static Type *get(LLVMContext& C) {
      if (NodeType == NULL) {
          NodeType = StructType::create(C, "Node");
      }
      return NodeType;
  }
};

template<bool cross>
class TypeBuilder<symtable_t, cross> {
public:
  static Type *get(LLVMContext& C) {
      if (SymTblType == NULL) {
          StructType *entryTy = StructType::create(C, "entry_t");
          Type  *EntryAryType = StructType::create("ARRAY_entry_t",
                  TypeBuilder<unsigned, cross>::get(C),
                  TypeBuilder<unsigned, cross>::get(C),
                  entryTy->getPointerTo(),
                  NULL
                  );

          SymTblType = StructType::create("symtable_t",
                  TypeBuilder<unsigned, cross>::get(C),
                  EntryAryType,
                  NULL
                  );
      }
      return SymTblType;
  }
};

template<bool cross>
class TypeBuilder<memo_t, cross> {
public:
  static Type *get(LLVMContext& C) {
      if (MemoType == NULL) {
          MemoType = StructType::create(C, "memo_t");
      }
      return MemoType;
  }
};

template<bool cross>
class TypeBuilder<bitset_t, cross> {
public:
  static Type *get(LLVMContext& C) {
      if (BitSetType == NULL) {
          Type *ElmTy = TypeBuilder<bitset_entry_t, cross>::get(C);
          ArrayType *ATy = ArrayType::get(ElmTy, 256/BITS);
          BitSetType = StructType::create("bitset_t", ATy, NULL);
      }
      return BitSetType;
  }
};

template<bool cross>
class TypeBuilder<MemoEntry_t, cross> {
public:
  static Type *get(LLVMContext& C) {
      if (MemoEntryType == NULL) {
          MemoEntryType = StructType::create("MemoEntry_t",
                  TypeBuilder<uintptr_t, cross>::get(C),
                  TypeBuilder<Node *, cross>::get(C),
                  TypeBuilder<unsigned, cross>::get(C),
                  TypeBuilder<unsigned, cross>::get(C),
                  NULL
                  );
      }
      return MemoEntryType;
  }
};

template<bool cross>
class TypeBuilder<AstMachine, cross> {
public:
  static Type *get(LLVMContext& C) {
      if (AstMachineType == NULL) {
          StructType *logTy = StructType::create(C, "AstLog");
          Type  *AstLogType = StructType::create("ARRAY_AstLog_t",
                  TypeBuilder<unsigned, cross>::get(C),
                  TypeBuilder<unsigned, cross>::get(C),
                  logTy->getPointerTo(),
                  NULL
                  );
          AstMachineType = StructType::create("AstMachine",
                  AstLogType,
                  TypeBuilder<Node *, cross>::get(C),
                  TypeBuilder<Node *, cross>::get(C),
                  TypeBuilder<char *, cross>::get(C),
                  NULL
                  );
      }
      return AstMachineType;
  }
};

template<bool cross>
class TypeBuilder<jump_table1_t, cross> {
public:
  static Type *get(LLVMContext& C) {
      if (JmpTbl1Type == NULL) {
          Type *BitSetType = TypeBuilder<bitset_t, cross>::get(C);
          Type *IntType    = TypeBuilder<int, cross>::get(C);
          ArrayType *A1 = ArrayType::get(BitSetType, 1);
          ArrayType *A2 = ArrayType::get(BitSetType, 2);
          JmpTbl1Type = StructType::create("jump_table1_t",
                  A1, A2, NULL);
      }
      return JmpTbl1Type;
  }
};

template<bool cross>
class TypeBuilder<jump_table2_t, cross> {
public:
  static Type *get(LLVMContext& C) {
      if (JmpTbl2Type == NULL) {
          Type *BitSetType = TypeBuilder<bitset_t, cross>::get(C);
          Type *IntType    = TypeBuilder<int, cross>::get(C);
          ArrayType *A1 = ArrayType::get(BitSetType, 2);
          ArrayType *A2 = ArrayType::get(BitSetType, 4);
          JmpTbl2Type = StructType::create("jump_table2_t",
                  A1, A2, NULL);
      }
      return JmpTbl2Type;
  }
};

template<bool cross>
class TypeBuilder<jump_table3_t, cross> {
public:
  static Type *get(LLVMContext& C) {
      if (JmpTbl3Type == NULL) {
          Type *BitSetType = TypeBuilder<bitset_t, cross>::get(C);
          Type *IntType    = TypeBuilder<int, cross>::get(C);
          ArrayType *A1 = ArrayType::get(BitSetType, 3);
          ArrayType *A2 = ArrayType::get(BitSetType, 8);
          JmpTbl3Type = StructType::create("jump_table3_t",
                  A1, A2, NULL);
      }
      return JmpTbl3Type;
  }
};

template<bool cross>
class TypeBuilder<mozvm_constant_t, cross> {
public:
  static Type *get(LLVMContext& C) {
      if (MozConstsType == NULL) {
          MozConstsType = StructType::create("mozvm_constant_t",
            TypeBuilder<bitset_t, cross>::get(C)->getPointerTo(),
            TypeBuilder<char **, cross>::get(C), // tags
            TypeBuilder<char **, cross>::get(C), // strs
            TypeBuilder<char **, cross>::get(C), // tables
            TypeBuilder<int   *, cross>::get(C),   // jumps
#ifdef MOZVM_USE_JMPTBL
            TypeBuilder<jump_table1_t  *, cross>::get(C),
            TypeBuilder<jump_table2_t  *, cross>::get(C),
            TypeBuilder<jump_table3_t  *, cross>::get(C),
#endif
            TypeBuilder<uint16_t, cross>::get(C), // nterms
            TypeBuilder<uint16_t, cross>::get(C), // set_size
            TypeBuilder<uint16_t, cross>::get(C), // str_size
            TypeBuilder<uint16_t, cross>::get(C), // tag_size
            TypeBuilder<uint16_t, cross>::get(C), // table_size
            TypeBuilder<uint16_t, cross>::get(C), // nterm_size

            TypeBuilder<unsigned, cross>::get(C), // inst_size
            TypeBuilder<unsigned, cross>::get(C), // memo_size
            TypeBuilder<unsigned, cross>::get(C), // input_size
#ifdef MOZVM_PROFILE_INST
            TypeBuilder<long *, cross>::get(C), // profile
#endif
            NULL
            );
      }
      return MozConstsType;
  }
};

template<bool cross>
class TypeBuilder<mozvm_nterm_entry_t, cross> {
public:
  static Type *get(LLVMContext& C) {
      if (NTermEntryType == NULL) {
          NTermEntryType = StructType::create(C, "mozvm_nterm_entry_t");
      }
      return NTermEntryType;
  }
  static void setBody(LLVMContext& C) {
      if (StructType *STy = dyn_cast<StructType>(NTermEntryType)) {
          assert(MozRuntimeType != NULL);
          STy->setBody(
            TypeBuilder<moz_inst_t *, cross>::get(C), // begin
            TypeBuilder<moz_inst_t *, cross>::get(C), // end
            TypeBuilder<unsigned, cross>::get(C),     // call_counter
            TypeBuilder<moz_jit_func_t, cross>::get(C), //compiled_cod
            NULL
            );
      }
  }
};

template<bool cross>
class TypeBuilder<moz_runtime_t, cross> {
public:
  static Type *get(LLVMContext& C) {
      if (MozRuntimeType == NULL) {
          MozRuntimeType = StructType::create("moz_runtime_t",
                  TypeBuilder<AstMachine, cross>::get(C)->getPointerTo(),
                  TypeBuilder<symtable_t, cross>::get(C)->getPointerTo(),
                  TypeBuilder<memo_t,     cross>::get(C)->getPointerTo(),
                  TypeBuilder<mozpos_t,   cross>::get(C),
                  TypeBuilder<char *,     cross>::get(C), // tail
                  TypeBuilder<char *,     cross>::get(C), // input
                  TypeBuilder<long *,     cross>::get(C), // sp
                  TypeBuilder<long *,     cross>::get(C), // fp
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
                  TypeBuilder<long *,     cross>::get(C), // memo points
#endif
                  TypeBuilder<mozpos_t,   cross>::get(C), // cur
                  TypeBuilder<void *,   cross>::get(C), // jitctx
                  TypeBuilder<mozvm_nterm_entry_t *, cross>::get(C),
                  TypeBuilder<mozvm_constant_t, cross>::get(C),
                  ArrayType::get(TypeBuilder<long, cross>::get(C), 1),
                  NULL);
          TypeBuilder<mozvm_nterm_entry_t, cross>::setBody(C);
      }
      return MozRuntimeType;
  }
};
} /* namespace llvm */

template<typename FuncType>
static llvm::FunctionType *GetFuncType(FuncType &_f)
{
    return llvm::TypeBuilder<FuncType, false>::get(llvm::getGlobalContext());
}

template<typename T>
static llvm::Type *GetType()
{
    return llvm::TypeBuilder<T, false>::get(llvm::getGlobalContext());
}

#define REGISTER_FUNC(M, FUNC) RegisterFunc(M, #FUNC, FUNC)
template<typename FuncType>
static llvm::Constant *RegisterFunc(llvm::Module *M, const char *name, FuncType &F)
{
    llvm::FunctionType *FTy = GetFuncType(F);
    return M->getOrInsertFunction(name, FTy);
}

