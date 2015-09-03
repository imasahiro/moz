#include "mozvm.h"

#ifndef __JIT_H__
#define __JIT_H__

#ifdef __cplusplus
extern "C" {
#endif

void mozvm_jit_init(moz_runtime_t *runtime);
void mozvm_jit_reset(moz_runtime_t *runtime);
void mozvm_jit_dispose(moz_runtime_t *runtime);
moz_jit_func_t mozvm_jit_compile(moz_runtime_t *runtime, mozvm_nterm_entry_t *e);

#ifdef __cplusplus
}
#endif //__cplusplus
#endif //__JIT_H__
