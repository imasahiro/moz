#include <stdio.h>

#include "mozvm.h"
#include "core/pstring.h"

#ifdef MOZVM_ENABLE_JIT
#include "jit.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

moz_runtime_t *moz_runtime_init(unsigned memo)
{
    moz_runtime_t *r;
    unsigned size = sizeof(*r) + sizeof(long) * (MOZ_DEFAULT_STACK_SIZE - 1);
    r = (moz_runtime_t *)VM_CALLOC(1, size);
    r->ast = AstMachine_init(MOZ_AST_MACHINE_DEFAULT_LOG_SIZE, NULL);
    r->table = symtable_init();
    r->memo = memo_init(MOZ_MEMO_DEFAULT_WINDOW_SIZE, memo);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
    r->memo_points = (MemoPoint *)VM_CALLOC(1, sizeof(MemoPoint) * memo);
#endif
    r->head = 0;
    r->input = r->tail = NULL;
    memset(&r->stack_[0], 0xaa, sizeof(long) * MOZ_DEFAULT_STACK_SIZE);
    memset(&r->stack_[MOZ_DEFAULT_STACK_SIZE - 0xf], 0xbb, sizeof(long) * 0xf);
    r->stack = &r->stack_[0] + 0xf;
    r->fp = r->stack;

    r->C.memo_size = memo;
#ifdef MOZVM_ENABLE_JIT
    r->cur = 0;
    mozvm_jit_init(r);
#endif
#ifdef MOZVM_MEMORY_USE_MSGC
    NodeManager_add_gc_root(r->ast, ast_trace);
    NodeManager_add_gc_root(r->memo, memo_trace);
#endif
    return r;
}

void moz_runtime_reset1(moz_runtime_t *r)
{
    unsigned memo = r->C.memo_size;
    AstMachine_dispose(r->ast);
    symtable_dispose(r->table);
    memo_dispose(r->memo);
#ifdef MOZVM_ENABLE_JIT
    mozvm_jit_reset(r);
#endif

    r->ast = AstMachine_init(MOZ_AST_MACHINE_DEFAULT_LOG_SIZE, NULL);
    r->table = symtable_init();
    r->memo = memo_init(MOZ_MEMO_DEFAULT_WINDOW_SIZE, memo);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
    memset(r->memo_points, 0, sizeof(MemoPoint) * memo);
#endif
    r->stack = &r->stack_[0] + 0xf;
    r->fp = r->stack;
}
void moz_runtime_reset2(moz_runtime_t *r)
{
#ifdef MOZVM_MEMORY_USE_MSGC
    NodeManager_add_gc_root(r->ast, ast_trace);
    NodeManager_add_gc_root(r->memo, memo_trace);
#endif
}


void moz_runtime_dispose(moz_runtime_t *r)
{
    unsigned i;
    AstMachine_dispose(r->ast);
    symtable_dispose(r->table);
    memo_dispose(r->memo);
#ifdef MOZVM_USE_DYNAMIC_DEACTIVATION
    VM_FREE(r->memo_points);
#endif
    if (r->C.jumps) {
        VM_FREE(r->C.jumps);
    }
#ifdef MOZVM_USE_JMPTBL
    if (r->C.jumps1) {
        VM_FREE(r->C.jumps1);
    }
    if (r->C.jumps2) {
        VM_FREE(r->C.jumps2);
    }
    if (r->C.jumps3) {
        VM_FREE(r->C.jumps3);
    }
#endif
#ifdef MOZVM_PROFILE_INST
    if (r->C.profile) {
        VM_FREE(r->C.profile);
    }
#endif

#ifdef MOZVM_ENABLE_JIT
    mozvm_jit_dispose(r);
#endif
    if (r->C.set_size) {
        VM_FREE(r->C.sets);
    }
    if (r->C.table_size) {
        for (i = 0; i < r->C.table_size; i++) {
            pstring_delete((const char *)r->C.tables[i]);
        }
        VM_FREE(r->C.tables);
    }
    if (r->C.tag_size) {
        for (i = 0; i < r->C.tag_size; i++) {
            pstring_delete((const char *)r->C.tags[i]);
        }
        VM_FREE(r->C.tags);
    }
    if (r->C.str_size) {
        for (i = 0; i < r->C.str_size; i++) {
            pstring_delete((const char *)r->C.strs[i]);
        }
        VM_FREE(r->C.strs);
    }
    if (r->C.prod_size) {
        for (i = 0; i < r->C.prod_size; i++) {
            pstring_delete((const char *)r->C.prods[i].name);
        }
        VM_FREE(r->C.prods);
    }
    VM_FREE(r);
}

#ifdef __cplusplus
}
#endif
