#include <assert.h>

#ifndef MOZ_RC_H
#define MOZ_RC_H

#define MOZ_RC_FIELD      __refc
#define MOZ_RC_HEADER     long MOZ_RC_FIELD
#define MOZ_RC_COUNT(O)   (O)->MOZ_RC_FIELD
#define MOZ_RC_INIT(O)    MOZ_RC_COUNT(O) = 0
#define MOZ_RC_RETAIN(O)  (MOZ_RC_COUNT(O))++
#define MOZ_RC_RELEASE(O, SWEEP) do {\
    assert(MOZ_RC_COUNT(O) > 0);\
    (MOZ_RC_COUNT(O))--; \
    if (MOZ_RC_COUNT(O) == 0) { \
        SWEEP(O); \
    } \
} while (0)

#define MOZ_RC_ASSIGN(v, o, SWEEP) do {\
    struct moz_rc_obj_t { \
        MOZ_RC_HEADER; \
    } *__tmp = (struct moz_rc_obj_t *) o; \
    MOZ_RC_RETAIN(__tmp); \
    MOZ_RC_RELEASE(v, SWEEP); \
    *((struct moz_rc_obj_t **)&v) = (__tmp); \
} while (0)

#define MOZ_RC_INIT_FIELD(field, o) do {\
    struct moz_rc_obj_t { \
        MOZ_RC_HEADER; \
    } *__tmp = (struct moz_rc_obj_t *) o; \
    assert(field == NULL); \
    MOZ_RC_RETAIN(__tmp); \
    *((struct moz_rc_obj_t **)&field) = (__tmp); \
} while (0)

#endif /* end of include guard */
