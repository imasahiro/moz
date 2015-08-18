#ifndef KHASH_H
#define KHASH_H

#ifdef KHASH_USE_HASH6432SHIFT
// This 64-bit-to-32-bit hash was copied from
// http://www.concentric.net/~Ttwang/tech/inthash.htm .
static unsigned hash6432shift(uintptr_t key)
{
    key = (~key) + (key << 18); // key = (key << 18) - key - 1;
    key = key ^ (key >> 31);
    key = key * 21; // key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 11);
    key = key + (key << 6);
    key = key ^ (key >> 22);
    return (unsigned)key;
}
#endif

#ifdef KHASH_USE_FNV1A
// FNV1A hash
static unsigned fnv1a_string(const uint8_t *s, uint32_t len, uint32_t hash)
{
    const uint8_t *e = s + len;
    while(s < e) {
        hash = (*s++ ^ hash) * 0x01000193;
    }
    return hash;
}

static unsigned fnv1a(const char *p, uint32_t len)
{
    const uint8_t *str = (const uint8_t *) p;
    uint32_t hash = 0x811c9dc5;
#define UNROLL 4
    while(len >= UNROLL) {
      hash = fnv1a_string(str, UNROLL, hash);
      str += UNROLL;
      len -= UNROLL;
    }
    return fnv1a_string(str, len, hash);
}
#endif

#endif /* end of include guard */
