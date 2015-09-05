#ifndef API_IR
#define API_IR "; ModuleID = 'api.h'\n\
\n\
%struct.moz_runtime_t = type { %struct.AstMachine*, %struct.symtable_t*, %struct.memo*, i8*, i8*, i8*, i64*, i64*, %struct.mozvm_nterm_entry_t*, i8*, %struct.mozvm_constant_t, [1 x i64] }\n\
%struct.AstMachine = type { %struct.ARRAY_AstLog_t, %struct.Node*, %struct.Node*, i8* }\n\
%struct.ARRAY_AstLog_t = type { i32, i32, %struct.AstLog* }\n\
%struct.AstLog = type { i32, i32, %union.ast_log_entry, %union.ast_log_index }\n\
%union.ast_log_entry = type { i64 }\n\
%union.ast_log_index = type { i64 }\n\
%struct.Node = type { i64, i8*, i8*, i8*, i32, %union.NodeEntry }\n\
%union.NodeEntry = type { %struct.node_small_array }\n\
%struct.node_small_array = type { i32, [2 x %struct.Node*] }\n\
%struct.symtable_t = type { i32, %struct.ARRAY_entry_t_t }\n\
%struct.ARRAY_entry_t_t = type { i32, i32, %struct.symtable_entry_t* }\n\
%struct.symtable_entry_t = type { i32, i32, i8*, %struct.moz_token }\n\
%struct.moz_token = type { i8*, i32 }\n\
%struct.memo = type opaque\n\
%struct.mozvm_nterm_entry_t = type { i8*, i8*, i32, i8 (%struct.moz_runtime_t*, i8*, i32*)* }\n\
%struct.mozvm_constant_t = type { %struct.bitset_t*, i8**, i8**, i8**, i32*, %struct.jump_table1_t*, %struct.jump_table2_t*, %struct.jump_table3_t*, i8**, i16, i16, i16, i16, i16, i32, i32, i32 }\n\
%struct.bitset_t = type { [4 x i64] }\n\
%struct.jump_table1_t = type { [1 x %struct.bitset_t], [2 x i32] }\n\
%struct.jump_table2_t = type { [2 x %struct.bitset_t], [4 x i32] }\n\
%struct.jump_table3_t = type { [3 x %struct.bitset_t], [8 x i32] }\n\
%struct.MemoEntry = type { i64, %union.anon, i32, i32 }\n\
%union.anon = type { %struct.Node* }\n\
\n\
; Function Attrs: nounwind readonly uwtable\n\
define i8* @string_Get_Impl(%struct.moz_runtime_t* nocapture readonly %runtime, i16 zeroext %ID) #0 {\n\
  %1 = zext i16 %ID to i64\n\
  %2 = getelementptr inbounds %struct.moz_runtime_t* %runtime, i64 0, i32 10, i32 2\n\
  %3 = load i8*** %2, align 8, !tbaa !1\n\
  %4 = getelementptr inbounds i8** %3, i64 %1\n\
  %5 = load i8** %4, align 8, !tbaa !9\n\
  ret i8* %5\n\
}\n\
\n\
; Function Attrs: nounwind readonly uwtable\n\
define i8* @tag_Get_Impl(%struct.moz_runtime_t* nocapture readonly %runtime, i16 zeroext %ID) #0 {\n\
  %1 = zext i16 %ID to i64\n\
  %2 = getelementptr inbounds %struct.moz_runtime_t* %runtime, i64 0, i32 10, i32 1\n\
  %3 = load i8*** %2, align 8, !tbaa !10\n\
  %4 = getelementptr inbounds i8** %3, i64 %1\n\
  %5 = load i8** %4, align 8, !tbaa !9\n\
  ret i8* %5\n\
}\n\
\n\
; Function Attrs: nounwind readonly uwtable\n\
define %struct.bitset_t* @bitset_Get_Impl(%struct.moz_runtime_t* nocapture readonly %runtime, i16 zeroext %ID) #0 {\n\
  %1 = zext i16 %ID to i64\n\
  %2 = getelementptr inbounds %struct.moz_runtime_t* %runtime, i64 0, i32 10, i32 0\n\
  %3 = load %struct.bitset_t** %2, align 8, !tbaa !11\n\
  %4 = getelementptr inbounds %struct.bitset_t* %3, i64 %1\n\
  ret %struct.bitset_t* %4\n\
}\n\
\n\
; Function Attrs: nounwind readonly uwtable\n\
define i32* @jmpTbl_Get_Impl(%struct.moz_runtime_t* nocapture readonly %runtime, i16 zeroext %ID) #0 {\n\
  %1 = getelementptr inbounds %struct.moz_runtime_t* %runtime, i64 0, i32 10, i32 4\n\
  %2 = load i32** %1, align 8, !tbaa !12\n\
  %3 = zext i16 %ID to i64\n\
  %4 = shl nuw nsw i64 %3, 8\n\
  %5 = getelementptr inbounds i32* %2, i64 %4\n\
  ret i32* %5\n\
}\n\
\n\
; Function Attrs: nounwind readonly uwtable\n\
define %struct.AstMachine* @ast_Machine_Get(%struct.moz_runtime_t* nocapture readonly %runtime) #0 {\n\
  %1 = getelementptr inbounds %struct.moz_runtime_t* %runtime, i64 0, i32 0\n\
  %2 = load %struct.AstMachine** %1, align 8, !tbaa !13\n\
  ret %struct.AstMachine* %2\n\
}\n\
\n\
; Function Attrs: nounwind readonly uwtable\n\
define %struct.symtable_t* @symtable_Get(%struct.moz_runtime_t* nocapture readonly %runtime) #0 {\n\
  %1 = getelementptr inbounds %struct.moz_runtime_t* %runtime, i64 0, i32 1\n\
  %2 = load %struct.symtable_t** %1, align 8, !tbaa !14\n\
  ret %struct.symtable_t* %2\n\
}\n\
\n\
; Function Attrs: nounwind readonly uwtable\n\
define %struct.memo* @memo_Get(%struct.moz_runtime_t* nocapture readonly %runtime) #0 {\n\
  %1 = getelementptr inbounds %struct.moz_runtime_t* %runtime, i64 0, i32 2\n\
  %2 = load %struct.memo** %1, align 8, !tbaa !15\n\
  ret %struct.memo* %2\n\
}\n\
\n\
; Function Attrs: nounwind readonly uwtable\n\
define i32 @call_bitset_get(%struct.bitset_t* nocapture readonly %set, i32 %index) #0 {\n\
  %1 = and i32 %index, 63\n\
  %2 = zext i32 %1 to i64\n\
  %3 = shl i64 1, %2\n\
  %div.i = lshr i32 %index, 6\n\
  %4 = zext i32 %div.i to i64\n\
  %5 = getelementptr inbounds %struct.bitset_t* %set, i64 0, i32 0, i64 %4\n\
  %6 = load i64* %5, align 8, !tbaa !16\n\
  %7 = and i64 %6, %3\n\
  %8 = icmp ne i64 %7, 0\n\
  %9 = zext i1 %8 to i32\n\
  ret i32 %9\n\
}\n\
\n\
; Function Attrs: nounwind readonly uwtable\n\
define i64 @call_ast_save_tx(%struct.AstMachine* nocapture readonly %ast) #0 {\n\
  %ast.idx = getelementptr %struct.AstMachine* %ast, i64 0, i32 0, i32 0\n\
  %ast.idx.val = load i32* %ast.idx, align 4, !tbaa !18\n\
  %1 = zext i32 %ast.idx.val to i64\n\
  ret i64 %1\n\
}\n\
\n\
declare void @ast_rollback_tx(%struct.AstMachine*, i64) #1\n\
\n\
declare void @ast_commit_tx(%struct.AstMachine*, i32, i64) #1\n\
\n\
declare void @ast_log_replace(%struct.AstMachine*, i8*) #1\n\
\n\
declare void @ast_log_capture(%struct.AstMachine*, i8*) #1\n\
\n\
declare void @ast_log_new(%struct.AstMachine*, i8*) #1\n\
\n\
declare void @ast_log_pop(%struct.AstMachine*, i32) #1\n\
\n\
declare void @ast_log_push(%struct.AstMachine*) #1\n\
\n\
declare void @ast_log_swap(%struct.AstMachine*, i8*) #1\n\
\n\
declare void @ast_log_tag(%struct.AstMachine*, i8*) #1\n\
\n\
declare void @ast_log_link(%struct.AstMachine*, i32, %struct.Node*) #1\n\
\n\
; Function Attrs: nounwind readonly uwtable\n\
define %struct.Node* @call_ast_get_last_linked_node(%struct.AstMachine* nocapture readonly %ast) #0 {\n\
  %ast.idx = getelementptr %struct.AstMachine* %ast, i64 0, i32 1\n\
  %ast.idx.val = load %struct.Node** %ast.idx, align 8, !tbaa !21\n\
  ret %struct.Node* %ast.idx.val\n\
}\n\
\n\
declare %struct.Node* @ast_get_parsed_node(%struct.AstMachine*) #1\n\
\n\
declare i32 @memo_set(%struct.memo*, i8*, i32, %struct.Node*, i32, i32) #1\n\
\n\
declare i32 @memo_fail(%struct.memo*, i8*, i32) #1\n\
\n\
declare %struct.MemoEntry* @memo_get(%struct.memo*, i8*, i32, i8 zeroext) #1\n\
\n\
declare void @symtable_add_symbol_mask(%struct.symtable_t*, i8*) #1\n\
\n\
declare void @symtable_add_symbol(%struct.symtable_t*, i8*, %struct.moz_token*) #1\n\
\n\
declare i32 @symtable_has_symbol(%struct.symtable_t*, i8*) #1\n\
\n\
declare i32 @symtable_get_symbol(%struct.symtable_t*, i8*, %struct.moz_token*) #1\n\
\n\
declare i32 @symtable_contains(%struct.symtable_t*, i8*, %struct.moz_token*) #1\n\
\n\
; Function Attrs: nounwind readonly uwtable\n\
define i64 @call_symtable_savepoint(%struct.symtable_t* nocapture readonly %tbl) #0 {\n\
  %tbl.idx = getelementptr %struct.symtable_t* %tbl, i64 0, i32 1, i32 0\n\
  %tbl.idx.val = load i32* %tbl.idx, align 4, !tbaa !22\n\
  %1 = zext i32 %tbl.idx.val to i64\n\
  ret i64 %1\n\
}\n\
\n\
declare void @symtable_rollback(%struct.symtable_t*, i64) #1\n\
\n\
attributes #0 = { nounwind readonly uwtable \"less-precise-fpmad\"=\"false\" \"no-frame-pointer-elim\"=\"false\" \"no-infs-fp-math\"=\"false\" \"no-nans-fp-math\"=\"false\" \"stack-protector-buffer-size\"=\"8\" \"unsafe-fp-math\"=\"false\" \"use-soft-float\"=\"false\" }\n\
attributes #1 = { \"less-precise-fpmad\"=\"false\" \"no-frame-pointer-elim\"=\"false\" \"no-infs-fp-math\"=\"false\" \"no-nans-fp-math\"=\"false\" \"stack-protector-buffer-size\"=\"8\" \"unsafe-fp-math\"=\"false\" \"use-soft-float\"=\"false\" }\n\
\n\
!llvm.ident = !{!0}\n\
\n\
!0 = !{!\"Ubuntu clang version 3.6.0-2ubuntu1~trusty1 (tags/RELEASE_360/final) (based on LLVM 3.6.0)\"}\n\
!1 = !{!2, !3, i64 96}\n\
!2 = !{!\"moz_runtime_t\", !3, i64 0, !3, i64 8, !3, i64 16, !3, i64 24, !3, i64 32, !3, i64 40, !3, i64 48, !3, i64 56, !3, i64 64, !3, i64 72, !6, i64 80, !4, i64 176}\n\
!3 = !{!\"any pointer\", !4, i64 0}\n\
!4 = !{!\"omnipotent char\", !5, i64 0}\n\
!5 = !{!\"Simple C/C++ TBAA\"}\n\
!6 = !{!\"mozvm_constant_t\", !3, i64 0, !3, i64 8, !3, i64 16, !3, i64 24, !3, i64 32, !3, i64 40, !3, i64 48, !3, i64 56, !3, i64 64, !7, i64 72, !7, i64 74, !7, i64 76, !7, i64 78, !7, i64 80, !8, i64 84, !8, i64 88, !8, i64 92}\n\
!7 = !{!\"short\", !4, i64 0}\n\
!8 = !{!\"int\", !4, i64 0}\n\
!9 = !{!3, !3, i64 0}\n\
!10 = !{!2, !3, i64 88}\n\
!11 = !{!2, !3, i64 80}\n\
!12 = !{!2, !3, i64 112}\n\
!13 = !{!2, !3, i64 0}\n\
!14 = !{!2, !3, i64 8}\n\
!15 = !{!2, !3, i64 16}\n\
!16 = !{!17, !17, i64 0}\n\
!17 = !{!\"long\", !4, i64 0}\n\
!18 = !{!19, !8, i64 0}\n\
!19 = !{!\"AstMachine\", !20, i64 0, !3, i64 16, !3, i64 24, !3, i64 32}\n\
!20 = !{!\"ARRAY_AstLog_t\", !8, i64 0, !8, i64 4, !3, i64 8}\n\
!21 = !{!19, !3, i64 16}\n\
!22 = !{!23, !8, i64 8}\n\
!23 = !{!\"symtable_t\", !8, i64 0, !24, i64 8}\n\
!24 = !{!\"ARRAY_entry_t_t\", !8, i64 0, !8, i64 4, !3, i64 8}"
#endif
