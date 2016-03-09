#include "core/karray.h"
#include "core/bitset.h"
#include "core/pstring.h"
#include "core/reference_count.h"
#include "compiler.h"

#ifndef MOZ_COMPILER_EXPRESSION_H
#define MOZ_COMPILER_EXPRESSION_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MOZ_BUFFER_H
DEF_ARRAY_STRUCT0(uint8_t, unsigned);
DEF_ARRAY_T(uint8_t);
#endif

#define FOR_EACH_BASE_AST(OP) \
    OP(Empty, Expr, Expr, Expr) \
    OP(Invoke, Name, Invoke, Invoke) \
    OP(Any, Expr, Expr, Expr) \
    OP(Byte, Byte, Expr, Expr) \
    OP(Str, Str, Expr, Expr) \
    OP(Set, Set, Expr, Expr) \
    OP(And, Unary, Unary, Unary) \
    OP(Choice, List, Choice, List) \
    OP(Fail, Expr, Expr, Expr) \
    OP(Not, Unary, Not, Unary) \
    OP(Option, Unary, Option, Unary) \
    OP(Sequence, List, Sequence, List) \
    OP(Repetition, List, Repetition, List) \
    OP(Tcapture, Expr, Expr, Expr) \
    OP(Tdetree, Expr, Expr, Expr) \
    OP(Tlfold, Expr, Expr, Expr) \
    OP(Tpush, Expr, Expr, Expr) \
    OP(Tpop, Name, Expr, Expr) \
    OP(Tnew, Expr, Expr, Expr) \
    OP(Treplace, Expr, Expr, Expr) \
    OP(Ttag, Name, Expr, Expr) \
    OP(Xblock, Unary, Unary, Unary) \
    OP(Xexists, Name, Expr, Expr) \
    OP(Xif, Expr, Expr, Expr) \
    OP(Xis, Name, Expr, Expr) \
    OP(Xisa, Name, Expr, Expr) \
    OP(Xon, Expr, Expr, Expr) \
    OP(Xmatch, Expr, Expr, Expr) \
    OP(Xlocal, NameUnary,  NameUnary, NameUnary) \
    OP(Xsymbol, NameUnary, NameUnary, NameUnary)

typedef enum expr_type {
#define DEFINE_ENUM(NAME, DUMP, OPT, SWEEP) NAME,
    FOR_EACH_BASE_AST(DEFINE_ENUM)
#undef DEFINE_ENUM
    DECL,
    MAX_TYPE
} expr_type_t;

typedef struct name {
    unsigned len;
    const char *str;
} name_t;

typedef struct decl {
    MOZ_RC_HEADER;
    expr_type_t type;
    name_t name;
    struct expr *body;
    struct block_t *inst;
} decl_t;

typedef struct expr {
    MOZ_RC_HEADER;
    expr_type_t type;
} expr_t;

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

typedef struct Tcapture_t {
    expr_t base;
} Tcapture_t;

typedef struct Tdetree_t {
    expr_t base;
} Tdetree_t;

typedef struct Tlfold_t {
    expr_t base;
} Tlfold_t;

typedef struct Tpush_t {
    expr_t base;
    name_t name;
    expr_t *expr;
} Tpush_t;

typedef struct Tpop_t {
    expr_t base;
    name_t name;
} Tpop_t;

typedef struct Tnew_t {
    expr_t base;
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

/* API */
void moz_expr_dump(int level, expr_t *e);
decl_t *moz_decl_new(moz_compiler_t *C, const char *name, unsigned len);
void moz_decl_mark_as_top_level(decl_t *decl);

/* Internal API */
void moz_node_to_ast(moz_compiler_t *C, Node *node);
void moz_ast_optimize(moz_compiler_t *C);
void moz_ast_dump(moz_compiler_t *C);
void moz_decl_sweep(decl_t *decl);
void moz_expr_sweep(expr_t *e);

/* Expression factory */
typedef const struct moz_expr_factory_t {
    expr_t *(*_Invoke)(moz_compiler_t *C, const char *str, unsigned len, decl_t *decl);
    expr_t *(*_Any)(moz_compiler_t *C);
    expr_t *(*_Byte)(moz_compiler_t *C, uint8_t byte);
    expr_t *(*_Str)(moz_compiler_t *C, const char *str, unsigned len);
    expr_t *(*_Set)(moz_compiler_t *C, unsigned *data, unsigned len);
    expr_t *(*_And)(moz_compiler_t *C, expr_t *expr);
    expr_t *(*_Choice)(moz_compiler_t *C);
    expr_t *(*_Empty)(moz_compiler_t *C);
    expr_t *(*_Fail)(moz_compiler_t *C);
    expr_t *(*_Not)(moz_compiler_t *C, expr_t *expr);
    expr_t *(*_Option)(moz_compiler_t *C, expr_t *expr);
    expr_t *(*_Sequence)(moz_compiler_t *C);
    expr_t *(*_Repetition)(moz_compiler_t *C);
    expr_t *(*_Tcapture)(moz_compiler_t *C);
    expr_t *(*_Tdetree)(moz_compiler_t *C);
    expr_t *(*_Tlfold)(moz_compiler_t *C);
    expr_t *(*_Tpush)(moz_compiler_t *C);
    expr_t *(*_Tpop)(moz_compiler_t *C, const char *str, unsigned len);
    expr_t *(*_Tnew)(moz_compiler_t *C);
    expr_t *(*_Treplace)(moz_compiler_t *C);
    expr_t *(*_Ttag)(moz_compiler_t *C, const char *str, unsigned len);
    expr_t *(*_Xblock)(moz_compiler_t *C, expr_t *expr);
    expr_t *(*_Xexists)(moz_compiler_t *C, const char *str, unsigned len);
    expr_t *(*_Xif)(moz_compiler_t *C);
    expr_t *(*_Xis)(moz_compiler_t *C, const char *str, unsigned len);
    expr_t *(*_Xisa)(moz_compiler_t *C, const char *str, unsigned len);
    expr_t *(*_Xlocal)(moz_compiler_t *C, const char *str, unsigned len, expr_t *expr);
    expr_t *(*_Xmatch)(moz_compiler_t *C);
    expr_t *(*_Xon)(moz_compiler_t *C);
    expr_t *(*_Xsymbol)(moz_compiler_t *C, const char *str, unsigned len, expr_t *expr);
} moz_expr_factory_t;

moz_expr_factory_t *moz_compiler_get_factory();

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
