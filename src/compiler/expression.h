#include "core/karray.h"
#include "core/bitset.h"
#include "core/reference_count.h"

#ifndef MOZ_COMPILER_EXPRESSION_H
#define MOZ_COMPILER_EXPRESSION_H

#ifdef __cplusplus
extern "C" {
#endif

#define FOR_EACH_BASE_AST(OP) \
    OP(Empty, Expr, Expr) \
    OP(Invoke, Name, Invoke) \
    OP(Any, Expr, Expr) \
    OP(Byte, Byte, Expr) \
    OP(Str, Str, Expr) \
    OP(Set, Set, Expr) \
    OP(And, Unary, Unary) \
    OP(Choice, List, Choice) \
    OP(Fail, Expr, Expr) \
    OP(Not, Unary, Not) \
    OP(Option, Unary, Option) \
    OP(Sequence, List, Sequence) \
    OP(Repetition, List, Repetition) \
    OP(Repetition1, List, Repetition1) \
    OP(Tcapture, Expr, Expr) \
    OP(Tdetree, Expr, Expr) \
    OP(Tlfold, Expr, Expr) \
    OP(Tlink, NameUnary, NameUnary) \
    OP(Tnew, Unary, Unary) \
    OP(Treplace, Expr, Expr) \
    OP(Ttag, Name, Expr) \
    OP(Xblock, Unary, Unary) \
    OP(Xexists, Name, Expr) \
    OP(Xif, Expr, Expr) \
    OP(Xis, Name, Expr) \
    OP(Xisa, Name, Expr) \
    OP(Xon, Expr, Expr) \
    OP(Xmatch, Expr, Expr) \
    OP(Xlocal, NameUnary,  NameUnary) \
    OP(Xsymbol, NameUnary, NameUnary)

#define FOR_EACH_EXTRA_AST(OP) \
    OP(Pattern, Unary, Unary) \
    OP(RByte, Byte, Expr) \
    OP(RStr, Str, Expr) \
    OP(RSet, Set, Expr) \
    OP(RUByte, Byte, Expr) \
    OP(RUSet, Set, Expr)

typedef enum expr_type {
#define DEFINE_ENUM(NAME, DUMP, OPT) NAME,
    FOR_EACH_BASE_AST(DEFINE_ENUM)
    FOR_EACH_EXTRA_AST(DEFINE_ENUM)
#undef DEFINE_ENUM
    MAX_TYPE
} expr_type_t;

typedef struct name {
    unsigned len;
    const char *str;
} name_t;

typedef struct decl {
    MOZ_RC_HEADER;
    name_t name;
    struct expr *body;
} decl_t;

typedef struct expr {
    MOZ_RC_HEADER;
    expr_type_t type;
} expr_t;

typedef decl_t *decl_ptr_t;
typedef expr_t *expr_ptr_t;

DEF_ARRAY_STRUCT0(decl_ptr_t, unsigned);
DEF_ARRAY_T(decl_ptr_t);
DEF_ARRAY_OP_NOPOINTER(decl_ptr_t);

DEF_ARRAY_STRUCT0(expr_ptr_t, unsigned);
DEF_ARRAY_T(expr_ptr_t);
DEF_ARRAY_OP_NOPOINTER(expr_ptr_t);

DEF_ARRAY_STRUCT0(uint8_t, unsigned);
DEF_ARRAY_T(uint8_t);
DEF_ARRAY_OP_NOPOINTER(uint8_t);

typedef struct Unary_t {
    expr_t base;
    expr_t *expr;
} Unary_t;

typedef struct Name_t {
    expr_t base;
    name_t name;
} Name_t;

typedef struct NameUnary_t {
    expr_t base;
    name_t name;
    expr_t *expr;
} NameUnary_t;

typedef struct List_t {
    expr_t base;
    ARRAY(expr_ptr_t) list;
} List_t;

typedef struct Invoke_t {
    expr_t base;
    name_t name;
    decl_t *decl;
} Invoke_t;

typedef struct Any_t {
    expr_t base;
} Any_t;

typedef struct Byte_t {
    expr_t base;
    uint8_t byte;
} Byte_t;

typedef struct Str_t {
    expr_t base;
    ARRAY(uint8_t) list;
} Str_t;

typedef struct Set_t {
    expr_t base;
    bitset_t set;
} Set_t;

typedef struct RByte_t {
    expr_t base;
    uint8_t byte;
} RByte_t;

typedef struct RStr_t {
    expr_t base;
    ARRAY(uint8_t) list;
} RStr_t;

typedef struct RSet_t {
    expr_t base;
    bitset_t set;
} RSet_t;

typedef struct Pattern_t {
    expr_t base;
} Pattern_t;

typedef struct And_t {
    expr_t base;
    expr_t *expr;
} And_t;

typedef struct Choice_t {
    expr_t base;
    ARRAY(expr_ptr_t) list;
} Choice_t;

typedef struct Empty_t {
    expr_t base;
} Empty_t;

typedef struct Fail_t {
    expr_t base;
} Fail_t;

typedef struct Not_t {
    expr_t base;
    expr_t *expr;
} Not_t;

typedef struct Option_t {
    expr_t base;
    expr_t *expr;
} Option_t;

typedef struct Sequence_t {
    expr_t base;
    ARRAY(expr_ptr_t) list;
} Sequence_t;

typedef struct Repetition_t {
    expr_t base;
    ARRAY(expr_ptr_t) list;
} Repetition_t;

typedef struct Repetition1_t {
    expr_t base;
    ARRAY(expr_ptr_t) list;
} Repetition1_t;

typedef struct Tcapture_t {
    expr_t base;
} Tcapture_t;

typedef struct Tdetree_t {
    expr_t base;
} Tdetree_t;

typedef struct Tlfold_t {
    expr_t base;
} Tlfold_t;

typedef struct Tlink_t {
    expr_t base;
    name_t name;
    expr_t *expr;
} Tlink_t;

typedef struct Tnew_t {
    expr_t base;
    expr_t *expr;
} Tnew_t;

typedef struct Treplace_t {
    expr_t base;
} Treplace_t;

typedef struct Ttag_t {
    expr_t base;
    name_t name;
} Ttag_t;

typedef struct Xblock_t {
    expr_t base;
    expr_t *expr;
} Xblock_t;

typedef struct Xexists_t {
    expr_t base;
    name_t name;
} Xexists_t;

typedef struct Xif_t {
    expr_t base;
} Xif_t;

typedef struct Xis_t {
    expr_t base;
    name_t name;
} Xis_t;

typedef struct Xisa_t {
    expr_t base;
    name_t name;
} Xisa_t;

typedef struct Xlocal_t {
    expr_t base;
    name_t name;
    expr_t *expr;
} Xlocal_t;

typedef struct Xmatch_t {
    expr_t base;
} Xmatch_t;

typedef struct Xon_t {
    expr_t base;
} Xon_t;

typedef struct Xsymbol_t {
    expr_t base;
    name_t name;
    expr_t *expr;
} Xsymbol_t;

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
