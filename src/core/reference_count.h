#include <assert.h>

#ifndef MOZ_RC_H
#define MOZ_RC_H

#define MOZ_RC_FIELD      __refc
#define MOZ_RC_HEADER     long MOZ_RC_FIELD
#define MOZ_RC_INIT(O)    (O)->MOZ_RC_FIELD = 0
#define MOZ_RC_RETAIN(O)  ((O)->MOZ_RC_FIELD)++
#define MOZ_RC_RELEASE(O, SWEEP) do {\
    assert((O)->MOZ_RC_FIELD > 0);\
    ((O)->MOZ_RC_FIELD)--; \
    if ((O)->MOZ_RC_FIELD == 0) { \
        SWEEP((O)); \
    } \
} while (0)

#endif /* end of include guard */
