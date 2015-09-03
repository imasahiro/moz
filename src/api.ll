; ModuleID = 'api.c'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.moz_runtime_t = type { %struct.AstMachine*, %struct.symtable_t*, %struct.memo*, i8*, i8*, i8*, i64*, %struct.mozvm_constant_t, [1 x i64] }
%struct.AstMachine = type { %struct.ARRAY_AstLog_t, %struct.Node*, %struct.Node*, i8* }
%struct.ARRAY_AstLog_t = type { i32, i32, %struct.AstLog* }
%struct.AstLog = type { i32, i32, %union.ast_log_entry, %union.ast_log_index }
%union.ast_log_entry = type { i64 }
%union.ast_log_index = type { i64 }
%struct.Node = type { i64, i8*, i8*, i8*, i32, %union.NodeEntry }
%union.NodeEntry = type { %struct.node_small_array }
%struct.node_small_array = type { i32, [2 x %struct.Node*] }
%struct.symtable_t = type { i32, %struct.ARRAY_entry_t_t }
%struct.ARRAY_entry_t_t = type { i32, i32, %struct.symtable_entry_t* }
%struct.symtable_entry_t = type { i32, i32, i8*, %struct.moz_token }
%struct.moz_token = type { i8*, i32 }
%struct.memo = type opaque
%struct.mozvm_constant_t = type { %struct.bitset_t*, i8**, i8**, i32*, %struct.jump_table1_t*, %struct.jump_table2_t*, %struct.jump_table3_t*, i8**, i16, i16, i16, i16, i16, i32, i32, i32 }
%struct.bitset_t = type { [4 x i64] }
%struct.jump_table1_t = type { [1 x %struct.bitset_t], [2 x i32] }
%struct.jump_table2_t = type { [2 x %struct.bitset_t], [4 x i32] }
%struct.jump_table3_t = type { [3 x %struct.bitset_t], [8 x i32] }
%struct.MemoEntry = type { i64, %union.anon, i32, i32 }
%union.anon = type { %struct.Node* }

; Function Attrs: nounwind readonly uwtable
define i8* @call_String_Get_Impl(%struct.moz_runtime_t* nocapture readonly %runtime, i16 zeroext %ID) #0 {
entry:
  %idxprom = zext i16 %ID to i64
  %strs = getelementptr inbounds %struct.moz_runtime_t* %runtime, i64 0, i32 7, i32 2
  %0 = load i8*** %strs, align 8, !tbaa !1
  %arrayidx = getelementptr inbounds i8** %0, i64 %idxprom
  %1 = load i8** %arrayidx, align 8, !tbaa !9
  ret i8* %1
}

; Function Attrs: nounwind readonly uwtable
define i8* @call_Tag_Get_Impl(%struct.moz_runtime_t* nocapture readonly %runtime, i16 zeroext %ID) #0 {
entry:
  %idxprom = zext i16 %ID to i64
  %tags = getelementptr inbounds %struct.moz_runtime_t* %runtime, i64 0, i32 7, i32 1
  %0 = load i8*** %tags, align 8, !tbaa !10
  %arrayidx = getelementptr inbounds i8** %0, i64 %idxprom
  %1 = load i8** %arrayidx, align 8, !tbaa !9
  ret i8* %1
}

; Function Attrs: nounwind readonly uwtable
define %struct.bitset_t* @call_Bitset_Get_Impl(%struct.moz_runtime_t* nocapture readonly %runtime, i16 zeroext %ID) #0 {
entry:
  %idxprom = zext i16 %ID to i64
  %sets = getelementptr inbounds %struct.moz_runtime_t* %runtime, i64 0, i32 7, i32 0
  %0 = load %struct.bitset_t** %sets, align 8, !tbaa !11
  %arrayidx = getelementptr inbounds %struct.bitset_t* %0, i64 %idxprom
  ret %struct.bitset_t* %arrayidx
}

; Function Attrs: nounwind readonly uwtable
define i32* @call_JmpTbl_Get_Impl(%struct.moz_runtime_t* nocapture readonly %runtime, i16 zeroext %ID) #0 {
entry:
  %jumps = getelementptr inbounds %struct.moz_runtime_t* %runtime, i64 0, i32 7, i32 3
  %0 = load i32** %jumps, align 8, !tbaa !12
  %conv = zext i16 %ID to i64
  %mul = shl nuw nsw i64 %conv, 8
  %add.ptr = getelementptr inbounds i32* %0, i64 %mul
  ret i32* %add.ptr
}

; Function Attrs: nounwind uwtable
define %struct.bitset_t* @call_bitset_init(%struct.bitset_t* %set) #1 {
entry:
  %set6.i = bitcast %struct.bitset_t* %set to i8*
  tail call void @llvm.memset.p0i8.i64(i8* %set6.i, i8 0, i64 32, i32 8, i1 false) #4
  ret %struct.bitset_t* %set
}

; Function Attrs: nounwind uwtable
define void @call_bitset_set(%struct.bitset_t* nocapture %set, i32 %index) #1 {
entry:
  %0 = and i32 %index, 63
  %rem.i = zext i32 %0 to i64
  %shl.i = shl i64 1, %rem.i
  %div3.i = lshr i32 %index, 6
  %div.i = zext i32 %div3.i to i64
  %arrayidx.i = getelementptr inbounds %struct.bitset_t* %set, i64 0, i32 0, i64 %div.i
  %1 = load i64* %arrayidx.i, align 8, !tbaa !13
  %or.i = or i64 %1, %shl.i
  store i64 %or.i, i64* %arrayidx.i, align 8, !tbaa !13
  ret void
}

; Function Attrs: nounwind readonly uwtable
define i32 @call_bitset_get(%struct.bitset_t* nocapture readonly %set, i32 %index) #0 {
entry:
  %0 = and i32 %index, 63
  %rem.i = zext i32 %0 to i64
  %shl.i = shl i64 1, %rem.i
  %div4.i = lshr i32 %index, 6
  %div.i = zext i32 %div4.i to i64
  %arrayidx.i = getelementptr inbounds %struct.bitset_t* %set, i64 0, i32 0, i64 %div.i
  %1 = load i64* %arrayidx.i, align 8, !tbaa !13
  %and.i = and i64 %1, %shl.i
  %cmp.i = icmp ne i64 %and.i, 0
  %conv2.i = zext i1 %cmp.i to i32
  ret i32 %conv2.i
}

; Function Attrs: nounwind readonly uwtable
define i64 @call_ast_save_tx(%struct.AstMachine* nocapture readonly %ast) #0 {
entry:
  %size.i = getelementptr inbounds %struct.AstMachine* %ast, i64 0, i32 0, i32 0
  %0 = load i32* %size.i, align 4, !tbaa !15
  %conv.i = zext i32 %0 to i64
  ret i64 %conv.i
}

; Function Attrs: nounwind uwtable
define void @call_ast_rollback_tx(%struct.AstMachine* %ast, i64 %tx) #1 {
entry:
  tail call void @ast_rollback_tx(%struct.AstMachine* %ast, i64 %tx) #4
  ret void
}

declare void @ast_rollback_tx(%struct.AstMachine*, i64) #2

; Function Attrs: nounwind uwtable
define void @call_ast_commit_tx(%struct.AstMachine* %ast, i32 %index, i64 %tx) #1 {
entry:
  tail call void @ast_commit_tx(%struct.AstMachine* %ast, i32 %index, i64 %tx) #4
  ret void
}

declare void @ast_commit_tx(%struct.AstMachine*, i32, i64) #2

; Function Attrs: nounwind uwtable
define void @call_ast_log_replace(%struct.AstMachine* %ast, i8* %str) #1 {
entry:
  tail call void @ast_log_replace(%struct.AstMachine* %ast, i8* %str) #4
  ret void
}

declare void @ast_log_replace(%struct.AstMachine*, i8*) #2

; Function Attrs: nounwind uwtable
define void @call_ast_log_capture(%struct.AstMachine* %ast, i8* %pos) #1 {
entry:
  tail call void @ast_log_capture(%struct.AstMachine* %ast, i8* %pos) #4
  ret void
}

declare void @ast_log_capture(%struct.AstMachine*, i8*) #2

; Function Attrs: nounwind uwtable
define void @call_ast_log_new(%struct.AstMachine* %ast, i8* %pos) #1 {
entry:
  tail call void @ast_log_new(%struct.AstMachine* %ast, i8* %pos) #4
  ret void
}

declare void @ast_log_new(%struct.AstMachine*, i8*) #2

; Function Attrs: nounwind uwtable
define void @call_ast_log_pop(%struct.AstMachine* %ast, i32 %index) #1 {
entry:
  tail call void @ast_log_pop(%struct.AstMachine* %ast, i32 %index) #4
  ret void
}

declare void @ast_log_pop(%struct.AstMachine*, i32) #2

; Function Attrs: nounwind uwtable
define void @acall_st_log_push(%struct.AstMachine* %ast) #1 {
entry:
  tail call void @ast_log_push(%struct.AstMachine* %ast) #4
  ret void
}

declare void @ast_log_push(%struct.AstMachine*) #2

; Function Attrs: nounwind uwtable
define void @call_ast_log_swap(%struct.AstMachine* %ast, i8* %pos) #1 {
entry:
  tail call void @ast_log_swap(%struct.AstMachine* %ast, i8* %pos) #4
  ret void
}

declare void @ast_log_swap(%struct.AstMachine*, i8*) #2

; Function Attrs: nounwind uwtable
define void @call_ast_log_tag(%struct.AstMachine* %ast, i8* %tag) #1 {
entry:
  tail call void @ast_log_tag(%struct.AstMachine* %ast, i8* %tag) #4
  ret void
}

declare void @ast_log_tag(%struct.AstMachine*, i8*) #2

; Function Attrs: nounwind uwtable
define void @call_ast_log_link(%struct.AstMachine* %ast, i32 %index, %struct.Node* %result) #1 {
entry:
  tail call void @ast_log_link(%struct.AstMachine* %ast, i32 %index, %struct.Node* %result) #4
  ret void
}

declare void @ast_log_link(%struct.AstMachine*, i32, %struct.Node*) #2

; Function Attrs: nounwind readonly uwtable
define %struct.Node* @call_ast_get_last_linked_node(%struct.AstMachine* nocapture readonly %ast) #0 {
entry:
  %last_linked.i = getelementptr inbounds %struct.AstMachine* %ast, i64 0, i32 1
  %0 = load %struct.Node** %last_linked.i, align 8, !tbaa !18
  ret %struct.Node* %0
}

; Function Attrs: nounwind uwtable
define %struct.Node* @call_ast_get_parsed_node(%struct.AstMachine* %ast) #1 {
entry:
  %call = tail call %struct.Node* @ast_get_parsed_node(%struct.AstMachine* %ast) #4
  ret %struct.Node* %call
}

declare %struct.Node* @ast_get_parsed_node(%struct.AstMachine*) #2

; Function Attrs: nounwind uwtable
define i32 @call_memo_set(%struct.memo* %memo, i8* %pos, i32 %memoId, %struct.Node* %n, i32 %consumed, i32 %state) #1 {
entry:
  %call = tail call i32 @memo_set(%struct.memo* %memo, i8* %pos, i32 %memoId, %struct.Node* %n, i32 %consumed, i32 %state) #4
  ret i32 %call
}

declare i32 @memo_set(%struct.memo*, i8*, i32, %struct.Node*, i32, i32) #2

; Function Attrs: nounwind uwtable
define i32 @call_memo_fail(%struct.memo* %memo, i8* %pos, i32 %memoId) #1 {
entry:
  %call = tail call i32 @memo_fail(%struct.memo* %memo, i8* %pos, i32 %memoId) #4
  ret i32 %call
}

declare i32 @memo_fail(%struct.memo*, i8*, i32) #2

; Function Attrs: nounwind readnone uwtable
define noalias %struct.MemoEntry* @memo_get(%struct.memo* nocapture readnone %memo, i8* nocapture readnone %pos, i32 %memoId, i8 zeroext %state) #3 {
entry:
  ret %struct.MemoEntry* undef
}

; Function Attrs: nounwind uwtable
define void @call_symtable_add_symbol_mask(%struct.symtable_t* %tbl, i8* %tableName) #1 {
entry:
  tail call void @symtable_add_symbol_mask(%struct.symtable_t* %tbl, i8* %tableName) #4
  ret void
}

declare void @symtable_add_symbol_mask(%struct.symtable_t*, i8*) #2

; Function Attrs: nounwind uwtable
define void @call_symtable_add_symbol(%struct.symtable_t* %tbl, i8* %tableName, %struct.moz_token* %captured) #1 {
entry:
  tail call void @symtable_add_symbol(%struct.symtable_t* %tbl, i8* %tableName, %struct.moz_token* %captured) #4
  ret void
}

declare void @symtable_add_symbol(%struct.symtable_t*, i8*, %struct.moz_token*) #2

; Function Attrs: nounwind uwtable
define i32 @call_symtable_has_symbol(%struct.symtable_t* %tbl, i8* %tableName) #1 {
entry:
  %call = tail call i32 @symtable_has_symbol(%struct.symtable_t* %tbl, i8* %tableName) #4
  ret i32 %call
}

declare i32 @symtable_has_symbol(%struct.symtable_t*, i8*) #2

; Function Attrs: nounwind uwtable
define i32 @call_symtable_get_symbol(%struct.symtable_t* %tbl, i8* %tableName, %struct.moz_token* %t) #1 {
entry:
  %call = tail call i32 @symtable_get_symbol(%struct.symtable_t* %tbl, i8* %tableName, %struct.moz_token* %t) #4
  ret i32 %call
}

declare i32 @symtable_get_symbol(%struct.symtable_t*, i8*, %struct.moz_token*) #2

; Function Attrs: nounwind uwtable
define i32 @call_symtable_contains(%struct.symtable_t* %tbl, i8* %tableName, %struct.moz_token* %t) #1 {
entry:
  %call = tail call i32 @symtable_contains(%struct.symtable_t* %tbl, i8* %tableName, %struct.moz_token* %t) #4
  ret i32 %call
}

declare i32 @symtable_contains(%struct.symtable_t*, i8*, %struct.moz_token*) #2

; Function Attrs: nounwind readonly uwtable
define i64 @call_symtable_savepoint(%struct.symtable_t* nocapture readonly %tbl) #0 {
entry:
  %size.i = getelementptr inbounds %struct.symtable_t* %tbl, i64 0, i32 1, i32 0
  %0 = load i32* %size.i, align 4, !tbaa !19
  %conv.i = zext i32 %0 to i64
  ret i64 %conv.i
}

; Function Attrs: nounwind uwtable
define void @call_symtable_rollback(%struct.symtable_t* %tbl, i64 %saved) #1 {
entry:
  tail call void @symtable_rollback(%struct.symtable_t* %tbl, i64 %saved) #4
  ret void
}

declare void @symtable_rollback(%struct.symtable_t*, i64) #2

; Function Attrs: nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture, i8, i64, i32, i1) #4

attributes #0 = { nounwind readonly uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind readnone uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { nounwind }

!llvm.ident = !{!0}

!0 = metadata !{metadata !"clang version 3.5.1 (tags/RELEASE_351/final)"}
!1 = metadata !{metadata !2, metadata !3, i64 72}
!2 = metadata !{metadata !"moz_runtime_t", metadata !3, i64 0, metadata !3, i64 8, metadata !3, i64 16, metadata !3, i64 24, metadata !3, i64 32, metadata !3, i64 40, metadata !3, i64 48, metadata !6, i64 56, metadata !4, i64 144}
!3 = metadata !{metadata !"any pointer", metadata !4, i64 0}
!4 = metadata !{metadata !"omnipotent char", metadata !5, i64 0}
!5 = metadata !{metadata !"Simple C/C++ TBAA"}
!6 = metadata !{metadata !"mozvm_constant_t", metadata !3, i64 0, metadata !3, i64 8, metadata !3, i64 16, metadata !3, i64 24, metadata !3, i64 32, metadata !3, i64 40, metadata !3, i64 48, metadata !3, i64 56, metadata !7, i64 64, metadata !7, i64 66, metadata !7, i64 68, metadata !7, i64 70, metadata !7, i64 72, metadata !8, i64 76, metadata !8, i64 80, metadata !8, i64 84}
!7 = metadata !{metadata !"short", metadata !4, i64 0}
!8 = metadata !{metadata !"int", metadata !4, i64 0}
!9 = metadata !{metadata !3, metadata !3, i64 0}
!10 = metadata !{metadata !2, metadata !3, i64 64}
!11 = metadata !{metadata !2, metadata !3, i64 56}
!12 = metadata !{metadata !2, metadata !3, i64 80}
!13 = metadata !{metadata !14, metadata !14, i64 0}
!14 = metadata !{metadata !"long", metadata !4, i64 0}
!15 = metadata !{metadata !16, metadata !8, i64 0}
!16 = metadata !{metadata !"AstMachine", metadata !17, i64 0, metadata !3, i64 16, metadata !3, i64 24, metadata !3, i64 32}
!17 = metadata !{metadata !"ARRAY_AstLog_t", metadata !8, i64 0, metadata !8, i64 4, metadata !3, i64 8}
!18 = metadata !{metadata !16, metadata !3, i64 16}
!19 = metadata !{metadata !20, metadata !8, i64 8}
!20 = metadata !{metadata !"symtable_t", metadata !8, i64 0, metadata !21, i64 8}
!21 = metadata !{metadata !"ARRAY_entry_t_t", metadata !8, i64 0, metadata !8, i64 4, metadata !3, i64 8}
