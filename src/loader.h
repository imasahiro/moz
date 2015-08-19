#include "mozvm.h"
#include "karray.h"
#ifndef LOADER_H
#define LOADER_H

DEF_ARRAY_STRUCT(uint8_t);
DEF_ARRAY_T(uint8_t);

struct mozvm_loader_t {
    unsigned jmptbl_id;
    moz_runtime_t *R;
    unsigned *table;
    ARRAY(uint8_t) buf;
};

typedef struct mozvm_loader_t mozvm_loader_t;
mozvm_loader_t *mozvm_loader_init(mozvm_loader_t *L, unsigned inst_size);
moz_inst_t *mozvm_loader_freeze(mozvm_loader_t *L);
void mozvm_loader_dispose(mozvm_loader_t *L);
moz_inst_t *mozvm_loader_load_file(mozvm_loader_t *L, const char *file);

#endif /* end of include guard */
