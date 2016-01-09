#include <memory.h>
#include "karray.h"

#ifndef MOZ_BUFFER_H
#define MOZ_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

DEF_ARRAY_T_OP_NOPOINTER(uint8_t);

struct moz_buffer_t {
    ARRAY(uint8_t) buf;
    unsigned pos;
};

typedef struct moz_buffer_reader_t {
    struct moz_buffer_t buf;
} moz_buffer_reader_t;

typedef struct moz_buffer_writer_t {
    struct moz_buffer_t buf;
} moz_buffer_writer_t;

// static inline uint16_t __swap_byte_order16(uint16_t value)
// {
//     uint16_t b0 = value << 8;
//     uint16_t b1 = value >> 8;
//     return b0 | b1;
// }
//
// static inline uint32_t __swap_byte_order32(uint32_t value)
// {
//     uint32_t b0 = value & 0x000000ff;
//     uint32_t b1 = value & 0x0000ff00;
//     uint32_t b2 = value & 0x00ff0000;
//     uint32_t b3 = value & 0xff000000;
//     return (b0 << 24) | (b1 << 8) | (b2 >> 8) | (b3 >> 24);
// }
//
// static inline uint64_t __swap_byte_order64(uint64_t value)
// {
//     uint64_t b0 = __swap_byte_order32((uint32_t)(value));
//     uint32_t b1 = __swap_byte_order32((uint32_t)(value >> 32));
//     return (b0 << 32) | b1;
// }

static inline int moz_buffer_reader_has_next(moz_buffer_reader_t *R)
{
    return R->buf.pos < R->buf.buf.size;
}

static inline uint8_t moz_buffer_reader_read8(moz_buffer_reader_t *R)
{
    return R->buf.buf.list[R->buf.pos++];
}

static inline uint16_t moz_buffer_reader_read16(moz_buffer_reader_t *R)
{
    uint16_t d1 = moz_buffer_reader_read8(R);
    uint16_t d2 = moz_buffer_reader_read8(R);
    if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {
        return d2 << 8 | d1;
    } else {
        return d1 << 8 | d2;
    }
}

static inline uint32_t moz_buffer_reader_read32(moz_buffer_reader_t *R)
{
    uint32_t d1 = moz_buffer_reader_read16(R);
    uint32_t d2 = moz_buffer_reader_read16(R);
    if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {
        return d2 << 16 | d1;
    } else {
        return d1 << 16 | d2;
    }
}

static inline uint64_t moz_buffer_reader_read64(moz_buffer_reader_t *R)
{
    uint64_t d1 = moz_buffer_reader_read32(R);
    uint64_t d2 = moz_buffer_reader_read32(R);
    if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {
        return d2 << 32 | d1;
    } else {
        return d1 << 32 | d2;
    }
}

static inline void moz_buffer_reader_init(moz_buffer_reader_t *R, const char *buf, unsigned size)
{
    R->buf.buf.list = (uint8_t *)buf;
    R->buf.buf.size = size;
    R->buf.buf.capacity = -1;
    R->buf.pos = 0;
}

static inline void moz_buffer_reader_init_from_writer(moz_buffer_reader_t *R, moz_buffer_writer_t *W)
{
    R->buf.buf.list = ARRAY_BEGIN(W->buf.buf);
    R->buf.buf.size = ARRAY_size(W->buf.buf);
    R->buf.buf.capacity = W->buf.buf.capacity;
    R->buf.pos = 0;
}

static inline void moz_buffer_writer_write8(moz_buffer_writer_t *W, uint8_t v)
{
    ARRAY_add(uint8_t, &W->buf.buf, v);
}

static inline void moz_buffer_writer_write16(moz_buffer_writer_t *W, uint16_t v)
{
    uint8_t *buf;
    unsigned n = sizeof(uint16_t)/sizeof(uint8_t);
    ARRAY_ensureSize(uint8_t, &W->buf.buf, n);
    buf = W->buf.buf.list + ARRAY_size(W->buf.buf);
    *(uint16_t *)buf = v;
    ARRAY_size(W->buf.buf) += n;
}

static inline void moz_buffer_writer_write32(moz_buffer_writer_t *W, uint32_t v)
{
    uint8_t *buf;
    unsigned n = sizeof(uint32_t)/sizeof(uint8_t);
    ARRAY_ensureSize(uint8_t, &W->buf.buf, n);
    buf = W->buf.buf.list + ARRAY_size(W->buf.buf);
    *(uint32_t *)buf = v;
    ARRAY_size(W->buf.buf) += n;
}

static inline void moz_buffer_writer_write64(moz_buffer_writer_t *W, uint64_t v)
{
    uint8_t *buf;
    unsigned n = sizeof(uint64_t)/sizeof(uint8_t);
    ARRAY_ensureSize(uint8_t, &W->buf.buf, n);
    buf = W->buf.buf.list + ARRAY_size(W->buf.buf);
    *(uint64_t *)buf = v;
    ARRAY_size(W->buf.buf) += n;
}

static inline void moz_buffer_writer_init(moz_buffer_writer_t *W, unsigned capacity)
{
    ARRAY_init(uint8_t, &W->buf.buf, capacity);
}

static inline unsigned moz_buffer_writer_length(moz_buffer_writer_t *W)
{
    return ARRAY_size(W->buf.buf);
}


static inline void moz_buffer_writer_dispose(moz_buffer_writer_t *W)
{
    ARRAY_dispose(uint8_t, &W->buf.buf);
    memset(W, 0, sizeof(moz_buffer_writer_t));
}

#ifdef __cplusplus
}
#endif

#endif /* end of include guard */
