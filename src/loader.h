#ifndef LOADER_H
#define LOADER_H

#include "mozvm.h"
#include "core/karray.h"

#ifdef __cplusplus
extern "C" {
#endif

DEF_ARRAY_STRUCT(uint8_t);
DEF_ARRAY_T(uint8_t);

struct mozvm_loader_t {
    char *input;
    size_t input_size;
    unsigned inst_size;
    unsigned jmptbl_id;
#ifdef MOZVM_USE_JMPTBL
    unsigned jmptbl1_id;
    unsigned jmptbl2_id;
    unsigned jmptbl3_id;
#endif
    moz_runtime_t *R;
    unsigned *table;
    ARRAY(uint8_t) buf;
};

typedef struct mozvm_loader_t mozvm_loader_t;
mozvm_loader_t *mozvm_loader_init(mozvm_loader_t *L, unsigned inst_size);
moz_inst_t *mozvm_loader_freeze(mozvm_loader_t *L);
void mozvm_loader_dispose(mozvm_loader_t *L);
moz_inst_t *mozvm_loader_load_syntax(mozvm_loader_t *L, const uint8_t *memory, unsigned len, int opt);
moz_inst_t *mozvm_loader_load_syntax_file(mozvm_loader_t *L, const char *file, int opt);
int mozvm_loader_load_input_file(mozvm_loader_t *L, const char *file);
int mozvm_loader_load_input_text(mozvm_loader_t *L, const char *text, unsigned len);

void moz_loader_print_stats(mozvm_loader_t *L);

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
