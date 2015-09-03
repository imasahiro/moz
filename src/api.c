#include "mozvm.h"
#include "bitset.h"
#include "ast.h"
#include "memo.h"
#include "symtable.h"

const char *call_String_Get_Impl(moz_runtime_t *runtime, STRING_t ID) {
	return STRING_GET_IMPL(runtime, ID);
}
tag_t *call_Tag_Get_Impl(moz_runtime_t *runtime, TAG_t ID) {
	return TAG_GET_IMPL(runtime, ID);
}
bitset_t *call_Bitset_Get_Impl(moz_runtime_t *runtime, BITSET_t ID) {
	return BITSET_GET_IMPL(runtime, ID);
}
int *call_JmpTbl_Get_Impl(moz_runtime_t *runtime, JMPTBL_t ID) {
	return JMPTBL_GET_IMPL(runtime, ID);
}

bitset_t *call_bitset_init(bitset_t *set) {
    return bitset_init(set);
}
void call_bitset_set(bitset_t *set, unsigned index) {
	bitset_set(set, index);
}
int call_bitset_get(bitset_t *set, unsigned index) {
    return bitset_get(set, index);
}

long call_ast_save_tx(AstMachine *ast) {
	return ast_save_tx(ast);
}
void call_ast_rollback_tx(AstMachine *ast, long tx) {
	ast_rollback_tx(ast, tx);
}
void call_ast_commit_tx(AstMachine *ast, int index, long tx) {
	ast_commit_tx(ast, index, tx);
}
void call_ast_log_replace(AstMachine *ast, const char *str) {
	ast_log_replace(ast, str);
}
void call_ast_log_capture(AstMachine *ast, mozpos_t pos) {
	ast_log_capture(ast, pos);
}
void call_ast_log_new(AstMachine *ast, mozpos_t pos) {
	ast_log_new(ast, pos);
}
void call_ast_log_pop(AstMachine *ast, int index) {
	ast_log_pop(ast, index);
}
void acall_st_log_push(AstMachine *ast) {
	ast_log_push(ast);
}
void call_ast_log_swap(AstMachine *ast, mozpos_t pos) {
	ast_log_swap(ast, pos);
}
void call_ast_log_tag(AstMachine *ast, const char *tag) {
	ast_log_tag(ast, tag);
}
void call_ast_log_link(AstMachine *ast, int index, Node result) {
	ast_log_link(ast, index, result);
}
Node call_ast_get_last_linked_node(AstMachine *ast) {
	return ast_get_last_linked_node(ast);
}
Node call_ast_get_parsed_node(AstMachine *ast) {
	return ast_get_parsed_node(ast);
}

int call_memo_set(memo_t *memo, mozpos_t pos, uint32_t memoId, Node n, unsigned consumed, int state) {
	return memo_set(memo, pos, memoId, n, consumed, state);
}
int call_memo_fail(memo_t *memo, mozpos_t pos, uint32_t memoId) {
	return memo_fail(memo, pos, memoId);
}
MemoEntry_t *memo_get(memo_t *memo, mozpos_t pos, uint32_t memoId, uint8_t state) {
	return memo_get(memo, pos, memoId, state);
}

void call_symtable_add_symbol_mask(symtable_t *tbl, const char *tableName) {
	symtable_add_symbol_mask(tbl, tableName);
}
void call_symtable_add_symbol(symtable_t *tbl, const char *tableName, token_t *captured) {
	symtable_add_symbol(tbl, tableName, captured);
}
int call_symtable_has_symbol(symtable_t *tbl, const char *tableName) {
	return symtable_has_symbol(tbl, tableName);
}
int call_symtable_get_symbol(symtable_t *tbl, const char *tableName, token_t *t) {
	return symtable_get_symbol(tbl, tableName, t);
}
int call_symtable_contains(symtable_t *tbl, const char *tableName, token_t *t) {
	return symtable_contains(tbl, tableName, t);
}
long call_symtable_savepoint(symtable_t *tbl) {
	return symtable_savepoint(tbl);
}
void call_symtable_rollback(symtable_t *tbl, long saved) {
	symtable_rollback(tbl, saved);
}
