#include "mozvm.h"

#ifndef __JIT_H__
#define __JIT_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MOZVM_ENABLE_JIT
void mozvm_jit_init(moz_runtime_t *runtime);
void mozvm_jit_reset(moz_runtime_t *runtime);
void mozvm_jit_dispose(moz_runtime_t *runtime);
uint8_t mozvm_jit_call_nterm(moz_runtime_t *runtime, const char *str, uint16_t nterm);
moz_jit_func_t mozvm_jit_compile(moz_runtime_t *runtime, mozvm_nterm_entry_t *e);

static inline int mozvm_nterm_is_already_compiled(mozvm_nterm_entry_t *e)
{
    return (e->compiled_code != mozvm_jit_call_nterm);
}
#endif

#ifdef __cplusplus
}
#endif //__cplusplus
#endif //__JIT_H__
