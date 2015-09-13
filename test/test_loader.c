#include "loader.h"
#include <assert.h>

int main(int argc, char const* argv[])
{
    mozvm_loader_t L = {};
    moz_inst_t *inst = mozvm_loader_load_file(&L, argv[1], 1);
    assert(inst != NULL);
    moz_runtime_dispose(L.R);
    mozvm_loader_dispose(&L);
    return 0;
}
