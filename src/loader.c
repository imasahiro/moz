#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>

// #define VERBOSE_DEBUG 1
// #define LOADER_DEBUG 2

#include "loader.h"
#if defined(VERBOSE_DEBUG) && !defined(LOADER_DEBUG)
#define LOADER_DEBUG 2
#endif

#if defined(MOZVM_PROFILE) && !defined(LOADER_DEBUG)
#define LOADER_DEBUG 1
#endif

#ifdef LOADER_DEBUG
#define MOZVM_DUMP_OPCODE 1
#endif
#include "instruction.h"
#include "pstring.h"

#ifdef MOZVM_ENABLE_JIT
#include "jit.h"
#endif

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MOZVM_OPCODE_SIZE 1
#include "vm_inst.h"

// #define LOADER_DEBUG 1

#ifdef MOZVM_USE_DIRECT_THREADING
#error FIXME. Need to implement First, TblJump1, TblJump2, TblJump3
#endif

#ifdef LOADER_DEBUG
static void mozvm_loader_dump(mozvm_loader_t *L, int print);
static void dump_set(bitset_t *set, char *buf);
#endif

#include "input_source.h"

typedef struct input_stream {
    size_t pos;
    size_t end;
    char *data;
} input_stream_t;

static char *peek(input_stream_t *is)
{
    return is->data + is->pos;
}

static void skip(input_stream_t *is, size_t shift)
{
    is->pos += shift;
}

static uint8_t read8(input_stream_t *is)
{
    return is->data[is->pos++];
}

static uint16_t read16(input_stream_t *is)
{
    uint16_t d1 = read8(is);
    uint16_t d2 = read8(is);
    return d1 << 8 | d2;
}

static unsigned read24(input_stream_t *is)
{
    unsigned d1 = read8(is);
    unsigned d2 = read8(is);
    unsigned d3 = read8(is);
    return d1 << 16 | d2 << 8 | d3;
}

static uint32_t read32(input_stream_t *is)
{
    uint32_t d1 = read16(is);
    uint32_t d2 = read16(is);
    return d1 << 16 | d2;
}

static int get_next(input_stream_t *is, int *has_jump)
{
    assert(has_jump > 0);
    *has_jump = 0;
    return read24(is);
}

DEF_ARRAY_OP_NOPOINTER(uint8_t);

mozvm_loader_t *mozvm_loader_init(mozvm_loader_t *L, unsigned inst_size)
{
    L->jmptbl_id = 0;
#ifdef MOZVM_USE_JMPTBL
    L->jmptbl1_id = 0;
    L->jmptbl2_id = 0;
    L->jmptbl3_id = 0;
#endif
    L->table = (unsigned *) VM_MALLOC(sizeof(unsigned) * inst_size);
    ARRAY_init(uint8_t, &L->buf, 4);
    return L;
}

moz_inst_t *mozvm_loader_freeze(mozvm_loader_t *L)
{
    unsigned i;
    moz_inst_t *inst = ARRAY_n(L->buf, 0);
    VM_FREE(L->table);
    L->table = NULL;
    for (i = 0; i < L->R->C.prod_size; i++) {
        moz_production_t *e = &L->R->C.prods[i];
        uintptr_t begin = (uintptr_t) e->begin;
        uintptr_t end   = (uintptr_t) e->end;
        e->begin = inst + begin;
        e->end   = inst + end;
#ifdef MOZVM_ENABLE_JIT
        e->compiled_code = mozvm_jit_call_prod;
#endif
    }
    return inst;
}

void mozvm_loader_dispose(mozvm_loader_t *L)
{
    ARRAY_dispose(uint8_t, &L->buf);
    if (L->input) {
        VM_FREE(L->input);
    }
}

void moz_loader_print_stats(mozvm_loader_t *L)
{
#ifdef MOZVM_PROFILE
    unsigned bytecode_size = ARRAY_size(L->buf);
    fprintf(stderr, "instruction size    %u %s\n",
            L->inst_size > 1024 * 10 ? L->inst_size / 1024 : L->inst_size,
            L->inst_size > 1024 * 10 ? "KB" : "B");
    fprintf(stderr, "bytecode    size    %u %s\n",
            bytecode_size > 1024 * 10 ? bytecode_size / 1024 : bytecode_size,
            bytecode_size > 1024 * 10 ? "KB" : "B");
#ifdef MOZVM_PROFILE_INST
    mozvm_loader_dump(L, 1);
#endif
#endif
}

static void mozvm_loader_write8(mozvm_loader_t *L, uint8_t v)
{
    ARRAY_add(uint8_t, &L->buf, v);
}

static void mozvm_loader_write16(mozvm_loader_t *L, uint16_t v)
{
    uint8_t *buf;
    unsigned n = sizeof(uint16_t)/sizeof(uint8_t);
    ARRAY_ensureSize(uint8_t, &L->buf, n);
    buf = L->buf.list + ARRAY_size(L->buf);
    *(uint16_t *)buf = v;
    ARRAY_size(L->buf) += n;
}

static void mozvm_loader_write32(mozvm_loader_t *L, uint32_t v)
{
    uint8_t *buf;
    unsigned n = sizeof(uint32_t)/sizeof(uint8_t);
    ARRAY_ensureSize(uint8_t, &L->buf, n);
    buf = L->buf.list + ARRAY_size(L->buf);
    *(uint32_t *)buf = v;
    ARRAY_size(L->buf) += n;
}

static void mozvm_loader_write64(mozvm_loader_t *L, uint64_t v)
{
    uint8_t *buf;
    unsigned n = sizeof(uint64_t)/sizeof(uint8_t);
    ARRAY_ensureSize(uint8_t, &L->buf, n);
    buf = L->buf.list + ARRAY_size(L->buf);
    *(uint64_t *)buf = v;
    ARRAY_size(L->buf) += n;
}

static void mozvm_loader_write_opcode(mozvm_loader_t *L, enum MozOpcode op)
{
#ifdef MOZVM_USE_DIRECT_THREADING
    if (sizeof(int) == sizeof(void *)) {
        mozvm_loader_write32(L, op);
    }
    else {
        mozvm_loader_write64(L, op);
    }
#else
    mozvm_loader_write8(L, (uint8_t)op);
#endif
}

static void mozvm_loader_write_addr(mozvm_loader_t *L, int addr)
{
#ifdef MOZVM_USE_INT16_ADDR
    mozvm_loader_write16(L, addr);
#else
    mozvm_loader_write32(L, addr);
#endif
}

static void mozvm_loader_write_id(mozvm_loader_t *L, int small, uint16_t id, void *ptr)
{
    if (small) {
        mozvm_loader_write16(L, id);
    }
    else {
        if (sizeof(long) == sizeof(int)) {
            mozvm_loader_write32(L, (long)ptr);
        }
        else if (sizeof(long) == sizeof(int64_t)) {
            mozvm_loader_write64(L, (long)ptr);
        }
        else {
            assert(0 && "not supported");
        }
    }
}

#ifdef MOZVM_USE_JMPTBL
static jump_table1_t *alloc_jump_table1(moz_runtime_t *r, uint16_t tblId)
{
    jump_table1_t *t;
    unsigned size = sizeof(jump_table1_t) * (tblId + 1);
    r->C.jumps1 = (jump_table1_t *) VM_REALLOC(r->C.jumps1, size);
    t = r->C.jumps1 + tblId;
    memset(t, 0, sizeof(*t));
    return t;
}

static jump_table2_t *alloc_jump_table2(moz_runtime_t *r, uint16_t tblId)
{
    jump_table2_t *t;
    unsigned size = sizeof(jump_table2_t) * (tblId + 1);
    r->C.jumps2 = (jump_table2_t *) VM_REALLOC(r->C.jumps2, size);
    t = r->C.jumps2 + tblId;
    memset(t, 0, sizeof(*t));
    return t;
}

static jump_table3_t *alloc_jump_table3(moz_runtime_t *r, uint16_t tblId)
{
    jump_table3_t *t;
    unsigned size = sizeof(jump_table3_t) * (tblId + 1);
    r->C.jumps3 = (jump_table3_t *) VM_REALLOC(r->C.jumps3, size);
    t = r->C.jumps3 + tblId;
    memset(t, 0, sizeof(*t));
    return t;
}
#endif

static int *alloc_jump_table(moz_runtime_t *r, uint16_t tblId)
{
    unsigned size = sizeof(int) * MOZ_JMPTABLE_SIZE * (tblId + 1);
    r->C.jumps = (int *) VM_REALLOC(r->C.jumps, size);
    return r->C.jumps + MOZ_JMPTABLE_SIZE * tblId;
}

static uint16_t mozvm_loader_load_inst(mozvm_loader_t *L, input_stream_t *is, int opt)
{
    uint8_t opcode = read8(is);
    int has_jump = opcode & 0x80;
    uint16_t prod = 0;
    opcode = opcode & 0x7f;
    if (opt &&
            (opcode == Nop
#ifdef MOZVM_USE_JMPTBL
             || opcode == First
#endif
#ifdef USE_SKIPJUMP
             || opcode == Skip
#endif
#ifndef MOZVM_EMIT_OP_LABEL
             || opcode == Label
#endif
            )) {/* skip */}
    else {
        mozvm_loader_write_opcode(L, (enum MozOpcode)opcode);
    }
#define CASE_(OP) case OP:
    switch (opcode) {
    CASE_(Nop);
    CASE_(Fail);
    CASE_(Succ) {
        break;
    }
    CASE_(Alt) {
        int failjump = read24(is);
        mozvm_loader_write_addr(L, failjump);
        break;
    }
    CASE_(Jump) {
        int jump = get_next(is, &has_jump);
        mozvm_loader_write_addr(L, jump);
        break;
    }
    CASE_(Call) {
        int next  = read24(is);
        int prod = read16(is);
        int jump  = get_next(is, &has_jump);
#ifdef MOZVM_USE_PROD
        mozvm_loader_write16(L, prod);
#endif
        mozvm_loader_write_addr(L, next);
        mozvm_loader_write_addr(L, jump);
        break;
        (void)prod;
    }
    CASE_(Ret) {
        mozvm_loader_write_addr(L, 0);
        break;
    }
    CASE_(Pos);
    CASE_(Back) {
        break;
    }
    CASE_(Skip) {
#ifdef USE_SKIPJUMP
        int jump;
        if (0 && opt && has_jump) {
            mozvm_loader_write_opcode(L, SkipJump);
            jump = get_next(is, &has_jump);
            mozvm_loader_write_addr(L, jump);
        }
        else {
            mozvm_loader_write_opcode(L, Skip);
        }
#endif
        break;
    }
    CASE_(Byte);
    CASE_(NByte);
    CASE_(OByte);
    CASE_(RByte) {
        uint8_t ch = read8(is);
        mozvm_loader_write8(L, ch);
        break;
    }
    CASE_(Any);
    CASE_(NAny);
    CASE_(OAny);
    CASE_(RAny) {
        break;
    }
    CASE_(Str);
    CASE_(NStr);
    CASE_(OStr);
    CASE_(RStr) {
        uint16_t strId = read16(is);
        const char *impl = L->R->C.strs[strId];
        mozvm_loader_write_id(L, MOZVM_SMALL_STRING_INST, strId, (void *)impl);
        break;
    }
    CASE_(Set);
    CASE_(NSet);
    CASE_(OSet);
    CASE_(RSet) {
        uint16_t setId = read16(is);
        bitset_t *impl = &L->R->C.sets[setId];
        mozvm_loader_write_id(L, MOZVM_SMALL_BITSET_INST, setId, (void *)impl);
        break;
    }
    CASE_(Consume) {
        int8_t shift = read8(is);
        mozvm_loader_write8(L, shift);
        break;
    }
    CASE_(First) {
        int i, j = 0, target_size = 0;
        int table[257] = {};
        int target[257] = {};
        uint16_t tblId;
        for (i = 0; i < 257; i++) {
            table[i] = (int)read24(is);
            for (j = 0; j < target_size; j++) {
                if (table[i] == target[j]) {
                    break;
                }
            }
            if (j == target_size) {
                target[target_size++] = table[i];
            }
        }
#ifdef MOZVM_USE_JMPTBL
        if (opt && target_size <= 1) {
            tblId = L->jmptbl1_id++;
            jump_table1_t *impl = alloc_jump_table1(L->R, tblId);
            jump_table1_init(impl, target, table);
            mozvm_loader_write_opcode(L, TblJump1);
            mozvm_loader_write16(L, tblId);
        }
        else if (opt && target_size <= 4) {
            for (; target_size < 4; target_size++) {
                target[target_size] = 0;
            }
            tblId = L->jmptbl2_id++;
            jump_table2_t *impl = alloc_jump_table2(L->R, tblId);
            jump_table2_init(impl, target, table);
            mozvm_loader_write_opcode(L, TblJump2);
            mozvm_loader_write16(L, tblId);
        }
        else if (opt && target_size <= 8) {
            for (; target_size < 8; target_size++) {
                target[target_size] = 0;
            }
            tblId = L->jmptbl3_id++;
            jump_table3_t *impl = alloc_jump_table3(L->R, tblId);
            jump_table3_init(impl, target, table);
            mozvm_loader_write_opcode(L, TblJump3);
            mozvm_loader_write16(L, tblId);
        }
        else
#endif
        {
            int *impl;
            tblId = L->jmptbl_id++;
            impl = alloc_jump_table(L->R, tblId);
            memcpy(impl, table, sizeof(int) * MOZ_JMPTABLE_SIZE);
#ifdef MOZVM_USE_JMPTBL
            mozvm_loader_write_opcode(L, First);
#endif
            mozvm_loader_write16(L, tblId);
#if 0
            fprintf(stderr, "tblId=%d %p\n", tblId, impl);
            fprintf(stderr, "[\n");
            for (i = 0; i < MOZ_JMPTABLE_SIZE; i++) {
                fprintf(stderr, "%3d ,", impl[i]);
                if (i % 16 == 15) {
                    fprintf(stderr, "\n");
                }
            }
            fprintf(stderr, "]\n");
#endif
        }
        break;
    }
    CASE_(Lookup) {
        int state = read8(is);
        uint32_t memoId = read32(is);
        int skip = read24(is);
        mozvm_loader_write8(L, (int8_t)state);
        mozvm_loader_write16(L, memoId);
        mozvm_loader_write_addr(L, skip);
        break;
    }
    CASE_(Memo) {
        int state = read8(is);
        uint32_t memoId = read32(is);
        mozvm_loader_write8(L, state);
        mozvm_loader_write16(L, memoId);
        break;
    }
    CASE_(MemoFail) {
        int state = read8(is);
        uint32_t memoId = read32(is);
        mozvm_loader_write8(L, (int8_t)state);
        mozvm_loader_write16(L, memoId);
        break;
    }
    CASE_(TPush) {
        break;
    }
    CASE_(TPop) {
        uint16_t tagId = read16(is);
        tag_t *impl = L->R->C.tags[tagId];
        mozvm_loader_write_id(L, MOZVM_SMALL_TAG_INST, tagId, (void *)impl);
        break;
    }
    CASE_(TLeftFold) {
        int8_t shift = read8(is);
        uint16_t tagId = read16(is);
        tag_t *impl = L->R->C.tags[tagId];
        mozvm_loader_write8(L, shift);
        mozvm_loader_write_id(L, MOZVM_SMALL_TAG_INST, tagId, (void *)impl);
        break;
    }
    CASE_(TNew) {
        int8_t shift = read8(is);
        mozvm_loader_write8(L, shift);
        break;
    }
    CASE_(TCapture) {
        int8_t shift = read8(is);
        mozvm_loader_write8(L, shift);
        break;
    }
    CASE_(TTag) {
        uint16_t tagId = read16(is);
        tag_t *impl = L->R->C.tags[tagId];
        mozvm_loader_write_id(L, MOZVM_SMALL_TAG_INST, tagId, (void *)impl);
        break;
    }
    CASE_(TReplace) {
        uint16_t strId = read16(is);
        const char *impl = L->R->C.strs[strId];
        mozvm_loader_write_id(L, MOZVM_SMALL_STRING_INST, strId, (void *)impl);
        break;
    }
    CASE_(TStart) {
        break;
    }
    CASE_(TAbort) {
        asm volatile("int3");
        break;
    }
    CASE_(TCommit) {
        uint16_t tagId = read16(is);
        tag_t *impl = L->R->C.tags[tagId];
        mozvm_loader_write_id(L, MOZVM_SMALL_TAG_INST, tagId, (void *)impl);
        break;
    }
    CASE_(TLookup) {
        int state = read8(is);
        uint32_t memoId = read32(is);
        int skip = read24(is);
        uint16_t tagId = read16(is);
        tag_t *impl = L->R->C.tags[tagId];

        mozvm_loader_write8(L, state);
        mozvm_loader_write_id(L, MOZVM_SMALL_TAG_INST, tagId, (void *)impl);
        mozvm_loader_write16(L, memoId);
        mozvm_loader_write_addr(L, skip);
        break;
    }
    CASE_(TMemo) {
        int state = read8(is);
        uint32_t memoId = read32(is);
        mozvm_loader_write8(L, state);
        mozvm_loader_write16(L, memoId);
        break;
    }
    CASE_(SOpen)
    CASE_(SClose) {
        break;
    }
    CASE_(SIsDef) {
        uint16_t tblId = read16(is);
        uint16_t strId = read16(is);
        tag_t *impl1 = L->R->C.tables[tblId];
        const char *impl2 = L->R->C.strs[strId];
        mozvm_loader_write_id(L, MOZVM_SMALL_TAG_INST, tblId, (void *)impl1);
        mozvm_loader_write_id(L, MOZVM_SMALL_STRING_INST, strId, (void *)impl2);
        break;
    }
    CASE_(SMask);
    CASE_(SDef);
    CASE_(SExists);
    CASE_(SMatch);
    CASE_(SIs);
    CASE_(SIsa) {
        uint16_t tblId = read16(is);
        tag_t *impl = L->R->C.tables[tblId];
        mozvm_loader_write_id(L, MOZVM_SMALL_TAG_INST, tblId, (void *)impl);
        break;
    }
    CASE_(SDefNum) {
        asm volatile("int3");
    }
    CASE_(SCount) {
        asm volatile("int3");
    }
    CASE_(Exit) {
        uint8_t status = read8(is);
        mozvm_loader_write8(L, status);
        break;
        (void)status;
    }
    CASE_(Label) {
        uint16_t label = read16(is);
#ifdef MOZVM_EMIT_OP_LABEL
        mozvm_loader_write16(L, label);
#endif
        prod = label;
        break;
    }
    }
#undef CASE_
    if (has_jump) {
        int jump = get_next(is, &has_jump);
        mozvm_loader_write_opcode(L, Jump);
        mozvm_loader_write_addr(L, jump);
    }
    return prod;
}

static long get_opcode(mozvm_loader_t *L, unsigned idx)
{
#ifdef MOZVM_USE_DIRECT_THREADING
    return *(long *)(L->buf.list + idx);
#else
    return (long)ARRAY_get(uint8_t, &L->buf, idx);
#endif
}

static void set_opcode(mozvm_loader_t *L, unsigned idx, long opcode)
{
#ifdef MOZVM_USE_DIRECT_THREADING
    *(long *)(L->buf.list + idx) = opcode;
#else
    ARRAY_set(uint8_t, &L->buf, idx, opcode);
#endif
}

static void mozvm_loader_load(mozvm_loader_t *L, input_stream_t *is, int opt)
{
    int i = 0, j = 0;
    uint16_t prod_id = 0;
#ifdef MOZVM_USE_DIRECT_THREADING
    const long *addr = (const long *)moz_runtime_parse(L->R, NULL, NULL);
#endif
    while (is->pos < is->end) {
        long begin = 0;
        uint8_t opcode = *peek(is);
        uint16_t prod;
        if (opcode == Label) {
            begin = (long)ARRAY_size(L->buf);
        }
        L->inst_size++;
        L->table[i++] = ARRAY_size(L->buf);
        prod = mozvm_loader_load_inst(L, is, opt);
        if (opcode == Label) {
            prod_id = prod;
            L->R->C.prods[prod_id].begin = (moz_inst_t *)begin;
        }
        L->R->C.prods[prod_id].end = (moz_inst_t *)(long)ARRAY_size(L->buf);
    }

    while (j < (int)ARRAY_size(L->buf)) {
        uint8_t opcode = get_opcode(L, j);
        unsigned shift = opcode_size(opcode);
        mozaddr_t *ref;
        int i, *table = NULL;
        uint8_t *p;
#ifdef MOZVM_USE_JMPTBL
        uint16_t tblId = 0;
        int *jumps = NULL;
        int table_size = 0;
#endif
        // fprintf(stderr, "%03d %-9s %-2d\n", j, opcode2str(opcode), shift);
#define GET_JUMP_ADDR(BUF, IDX) ((mozaddr_t *)((BUF).list + (IDX)))
        switch (opcode) {
        case Jump:
            ref = GET_JUMP_ADDR(L->buf, j + shift - sizeof(mozaddr_t));
            // L0: Jump L3  |  L0: Ret
            // ...          |
            // L3: Ret      |
            if (1 && opcode_size(Jump) == opcode_size(Ret)) {
                if (get_opcode(L, L->table[*ref]) == Ret) {
                    set_opcode(L, j, Ret);
                }
            }
            *ref = L->table[*ref] - (j + shift);
            break;
        case Call:
            // rewrite next
            ref = GET_JUMP_ADDR(L->buf, j + shift - 2 * sizeof(mozaddr_t));
            *ref = L->table[*ref] - (j + shift);
            /* fallthrough */
        case Alt:
        case Lookup:
        case TLookup:
#ifdef USE_SKIPJUMP
        case SkipJump:
#endif
            ref = GET_JUMP_ADDR(L->buf, j + shift - sizeof(mozaddr_t));
            *ref = L->table[*ref] - (j + shift);
            break;
        case First:
            p = L->buf.list + j + shift - sizeof(JMPTBL_t);
            table = JMPTBL_GET_IMPL(L->R, *(JMPTBL_t *)p);
            for (i = 0; i < MOZ_JMPTABLE_SIZE; i++) {
                table[i] = L->table[table[i]] - (j + shift);
            }
            break;
#ifdef MOZVM_USE_JMPTBL
        case TblJump1:
            tblId = *(uint16_t *)(L->buf.list + j + shift - sizeof(uint16_t));
            jumps = L->R->C.jumps1[tblId].jumps;
            table_size = 2;
            goto L_encode_jumps;
        case TblJump2:
            tblId = *(uint16_t *)(L->buf.list + j + shift - sizeof(uint16_t));
            jumps = L->R->C.jumps2[tblId].jumps;
            table_size = 4;
            goto L_encode_jumps;
        case TblJump3:
            tblId = *(uint16_t *)(L->buf.list + j + shift - sizeof(uint16_t));
            jumps = L->R->C.jumps3[tblId].jumps;
            table_size = 8;
L_encode_jumps:
            for (i = 0; i < table_size; i++) {
                if (jumps[i] != 0) {
                    jumps[i] = L->table[jumps[i]] - (j + shift);
                    assert(jumps[i] != INT_MAX);
                }
                else {
                    jumps[i] = INT_MAX;
                }
            }
            break;
#endif
        default:
            break;
        }
#undef GET_JUMP_ADDR
        j += shift;
    }

#ifdef MOZVM_USE_DIRECT_THREADING
    j = 0;
    while (j < ARRAY_size(L->buf)) {
        uint8_t opcode = get_opcode(L, j);;
        unsigned shift = opcode_size(opcode);
        set_opcode(L, j, addr[opcode]);
        j += shift;
    }
#endif
}

static int checkFileType(input_stream_t *is)
{
    return read8(is) == 'N' && read8(is) == 'E' && read8(is) == 'Z';
}

#define MOZ_SUPPORTED_NEZ_VERSION 0
static int checkVersion(input_stream_t *is)
{
    return read8(is) >= MOZ_SUPPORTED_NEZ_VERSION;
}

int mozvm_loader_load_input_file(mozvm_loader_t *L, const char *file)
{
    L->input = load_file(file, &L->input_size, 32);
    return L->input != NULL;
}

int mozvm_loader_load_input_text(mozvm_loader_t *L, const char *text, unsigned len)
{
    L->input = (char *)VM_CALLOC(1, len + 1);
    memcpy(L->input, text, len);
    return 1;
}

moz_inst_t *mozvm_loader_load_file(mozvm_loader_t *L, const char *file, int opt)
{
    unsigned i, inst_size, memo_size, jmptbl_size, prod_size;
    mozvm_constant_t *bc = NULL;
    input_stream_t is;
    moz_inst_t *inst = NULL;

    is.pos = 0;
    is.data = load_file(file, &is.end, 0);
    if (!checkFileType(&is)) {
        fprintf(stderr, "verify error: not bytecode file\n");
        exit(EXIT_FAILURE);
    }
    if (!checkVersion(&is)) {
        fprintf(stderr, "verify error: version miss match\n");
        exit(EXIT_FAILURE);
    }

    inst_size = (unsigned) read16(&is);
    memo_size = (unsigned) read16(&is);
    jmptbl_size = (unsigned) read16(&is);
    prod_size  = (unsigned) read16(&is);
    (void)jmptbl_size;

    mozvm_loader_init(L, inst_size);
    L->R = moz_runtime_init(memo_size);

#if defined(MOZVM_PROFILE) && defined(MOZVM_MEMORY_PROFILE)
    mozvm_mm_snapshot(MOZVM_MM_PROF_EVENT_RUNTIME_INIT);
#endif

    bc = &L->R->C;
    memset(bc, 0, sizeof(mozvm_constant_t));
    bc->inst_size = inst_size;
    bc->memo_size = memo_size;
    bc->prod_size = prod_size;

    if (bc->prod_size > 0) {
        unsigned size = sizeof(moz_production_t) * bc->prod_size;
        bc->prods = (moz_production_t *)VM_MALLOC(size);
        for (i = 0; i < bc->prod_size; i++) {
            uint16_t len = read16(&is);
            char *str = peek(&is);
            skip(&is, len + 1);
            bc->prods[i].name = pstring_alloc(str, (unsigned)len);
#if VERBOSE_DEBUG
            fprintf(stderr, "prod%d %s\n", i, bc->prods[i]);
#endif
        }
    }

    bc->set_size = read16(&is);
    if (bc->set_size > 0) {
        bc->sets = (bitset_t *) VM_MALLOC(sizeof(bitset_t) * bc->set_size);
#define INT_BIT (sizeof(int) * CHAR_BIT)
        for (i = 0; i < bc->set_size; i++) {
            unsigned j, k;
#if VERBOSE_DEBUG
            char buf[512] = {};
#endif
            bitset_t *set = &bc->sets[i];
            bitset_init(set);
            for (j = 0; j < 256/INT_BIT; j++) {
                unsigned v = read32(&is);
                for (k = 0; k < INT_BIT; k++) {
                    unsigned mask = 1U << k;
                    if ((v & mask) == mask)
                        bitset_set(set, k + INT_BIT * j);
                }
            }
#if VERBOSE_DEBUG
            dump_set(set, buf);
            fprintf(stderr, "set: %s\n", buf);
#endif
        }
#undef N
    }
    bc->str_size = read16(&is);
    if (bc->str_size > 0) {
        bc->strs = (const char **)VM_MALLOC(sizeof(const char *) * bc->str_size);
        for (i = 0; i < bc->str_size; i++) {
            uint16_t len = read16(&is);
            char *str = peek(&is);
            skip(&is, len + 1);
            bc->strs[i] = pstring_alloc(str, (unsigned)len);
#if VERBOSE_DEBUG
            fprintf(stderr, "str%d %s\n", i, bc->strs[i]);
#endif
        }
    }
    bc->tag_size = read16(&is);
    if (bc->tag_size > 0) {
        bc->tags = (const char **)VM_MALLOC(sizeof(const char *) * bc->tag_size);
        for (i = 0; i < bc->tag_size; i++) {
            uint16_t len = read16(&is);
            char *str = peek(&is);
            skip(&is, len + 1);
            bc->tags[i] = pstring_alloc(str, (unsigned)len);
#if VERBOSE_DEBUG
            fprintf(stderr, "tag%d %s\n", i, bc->tags[i]);
#endif
        }
    }
    bc->table_size = read16(&is);
    if (bc->table_size > 0) {
        bc->tables = (const char **)VM_MALLOC(sizeof(const char *) * bc->table_size);
        for (i = 0; i < bc->table_size; i++) {
            uint16_t len = read16(&is);
            char *str = peek(&is);
            skip(&is, len + 1);
            bc->tables[i] = pstring_alloc(str, (unsigned)len);
#if VERBOSE_DEBUG
            fprintf(stderr, "tbl%d %s\n", i, bc->tables[i]);
#endif
        }
    }

    mozvm_loader_load(L, &is, opt);
#ifdef MOZVM_PROFILE_INST
    L->R->C.profile = (long *)VM_CALLOC(1, sizeof(long) * ARRAY_size(L->buf));
#endif
    inst = mozvm_loader_freeze(L);
#ifdef LOADER_DEBUG
    mozvm_loader_dump(L, LOADER_DEBUG > 1);
#endif
    VM_FREE(is.data);

#if defined(MOZVM_PROFILE) && defined(MOZVM_MEMORY_PROFILE)
    mozvm_mm_snapshot(MOZVM_MM_PROF_EVENT_BYTECODE_LOAD);
#endif
    return inst;
}

#ifdef LOADER_DEBUG
static char *write_char(char *p, unsigned char ch)
{
    switch (ch) {
    case '\\':
        *p++ = '\\';
        break;
    case '\b':
        *p++ = '\\';
        *p++ = 'b';
        break;
    case '\f':
        *p++ = '\\';
        *p++ = 'f';
        break;
    case '\n':
        *p++ = '\\';
        *p++ = 'n';
        break;
    case '\r':
        *p++ = '\\';
        *p++ = 'r';
        break;
    case '\t':
        *p++ = '\\';
        *p++ = 't';
        break;
    default:
        if (32 <= ch && ch <= 126) {
            *p++ = ch;
        }
        else {
            *p++ = '\\';
            *p++ = "0123456789abcdef"[ch / 16];
            *p++ = "0123456789abcdef"[ch % 16];
        }
    }
    return p;
}

static void dump_set(bitset_t *set, char *buf)
{
    unsigned i, j;
    *buf++ = '[';
    for (i = 0; i < 256; i++) {
        if (bitset_get(set, i)) {
            buf = write_char(buf, i);
            for (j = i + 1; j < 256; j++) {
                if (!bitset_get(set, j)) {
                    if (j == i + 1) {}
                    else {
                        *buf++ = '-';
                        buf = write_char(buf, j - 1);
                        i = j - 1;
                    }
                    break;
                }
            }
            if (j == 256) {
                *buf++ = '-';
                buf = write_char(buf, j - 1);
                break;
            }
        }
    }
    *buf++ = ']';
    *buf++ = '\0';
}

#if 1
#define OP_PRINT(FMT, ...) if (print) { fprintf(stderr, FMT, __VA_ARGS__); }
#define OP_PRINT_END()     if (print) { fprintf(stderr, "\n"); }
#define OP_PRINT_STR(STR)  if (print) { fprintf(stderr, STR); }
#else
#define OP_PRINT(FMT, ...)
#define OP_PRINT_END()
#endif

static char *write_char(char *p, unsigned char ch);
static void mozvm_loader_dump(mozvm_loader_t *L, int print)
{
    int idx = 0, j = 0;
    uint8_t opcode = 0;
    moz_inst_t *head = ARRAY_n(L->buf, 0);
    for (j = 0; j < L->R->C.prod_size; j++) {
        moz_production_t *e = &L->R->C.prods[j];
        moz_inst_t *p;
        OP_PRINT("prod%d %s\n", j, e->name);
        unsigned shift = 0;
        int *table = NULL;
        int table_size = 0;
        for (p = e->begin; p < e->end; p += shift, idx++) {
            opcode = *p;
            shift = opcode_size(opcode);
#ifdef MOZVM_PROFILE_INST
            if (L->R->C.profile) {
                OP_PRINT("%8ld, ", L->R->C.profile[j]);
            }
#endif
            OP_PRINT("%4ld %4d %s ", (long)(p - head), idx, opcode2str(opcode));
            switch (opcode) {
#define CASE_(OP) case OP:
            CASE_(Nop);
            CASE_(Fail);
            CASE_(Succ) {
                break;
            }
            CASE_(Alt);
            CASE_(Jump) {
                OP_PRINT("%ld", (long)(p + shift - head + *(mozaddr_t *)(p + 1)));
                break;
            }
            CASE_(Call) {
                int next;
                int jump;
                moz_inst_t *pc = p;
#ifdef MOZVM_USE_PROD
                int prod = *(int16_t *)(p + 1);
                OP_PRINT("%s ", L->R->C.prods[prod].name);
                pc += sizeof(int16_t);
#endif
                next = *(mozaddr_t *)(pc + 1);
                jump = *(mozaddr_t *)(pc + 1 + sizeof(mozaddr_t));
                OP_PRINT("next=%ld ", (long)(pc + shift - head + next));
                OP_PRINT("jump=%ld" , (long)(pc + shift - head + jump));
                break;
            }
            CASE_(Ret);
            CASE_(Pos);
            CASE_(Back);
            CASE_(Skip) {
                break;
            }
            CASE_(Byte);
            CASE_(NByte);
            CASE_(OByte);
            CASE_(RByte) {
                char buf[10] = {};
                char *s = write_char(buf, *(p + 1));
                s[0] = '\0';
                OP_PRINT("%d '%s'", *(p + 1), buf);
                break;
            }
            CASE_(Any);
            CASE_(NAny);
            CASE_(OAny);
            CASE_(RAny) {
                break;
            }
            CASE_(Str);
            CASE_(NStr);
            CASE_(OStr);
            CASE_(RStr) {
                STRING_t strId = *(STRING_t *)(p + 1);
                const char *impl = STRING_GET_IMPL(L->R, strId);
                OP_PRINT("'%s'", impl);
                break;
            }
            CASE_(Set);
            CASE_(NSet);
            CASE_(OSet);
            CASE_(RSet) {
                BITSET_t setId = *(BITSET_t *)(p + 1);
                bitset_t *impl = BITSET_GET_IMPL(L->R, setId);
                char buf[1024];
                dump_set(impl, buf);
                OP_PRINT("%p %s", impl, buf);
                break;
            }
            CASE_(Consume) {
                int8_t shift = *(p + 1);
                OP_PRINT("%d", shift);
                break;
            }
            CASE_(First) {
                {
                    JMPTBL_t tblId = *(JMPTBL_t *)(p + 1);
                    int *impl = JMPTBL_GET_IMPL(L->R, tblId);
                    int i, target[257] = {};
                    table = target;
                    table_size = 0;
                    for (i = 0; i < 257; i++) {
                        int k;
                        for (k = 0; k < table_size; k++) {
                            if (impl[i] == target[k]) {
                                break;
                            }
                        }
                        if (j == table_size) {
                            target[table_size++] = impl[i];
                        }
                    }
                }
L_dump_table:
                {
                    int i;
                    OP_PRINT_STR("[");
                    for (i = 0; i < table_size; i++) {
                        if (table[i] != INT_MAX) {
                            if (i != 0) {
                                OP_PRINT_STR(", ");
                            }
                            OP_PRINT("%ld", (long)(p + shift + table[i] - head));
                        }
                    }
                    OP_PRINT_STR("]");
                }
                break;
            }
            CASE_(TblJump1) {
                uint16_t tblId = *(uint16_t *)(p + 1);
                jump_table1_t *t = L->R->C.jumps1 + tblId;
                table = t->jumps;
                table_size = 2;
                goto L_dump_table;
            }
            CASE_(TblJump2) {
                uint16_t tblId = *(uint16_t *)(p + 1);
                jump_table2_t *t = L->R->C.jumps2 + tblId;
                table = t->jumps;
                table_size = 4;
                goto L_dump_table;
            }
            CASE_(TblJump3) {
                uint16_t tblId = *(uint16_t *)(p + 1);
                jump_table2_t *t = L->R->C.jumps2 + tblId;
                table = t->jumps;
                table_size = 8;
                goto L_dump_table;
            }
            CASE_(Lookup) {
                int state = *(int8_t *)(p + 1);
                uint16_t memoId = *(uint16_t *)(p + 2);
                int skip = *(int *)(p + 4);
                OP_PRINT("%d %d %d", state, memoId, skip);
                break;
            }
            CASE_(Memo);
            CASE_(MemoFail) {
                int8_t state = *(int8_t *)(p + 1);
                uint16_t memoId = *(uint16_t *)(p + 2);
                OP_PRINT("%d %d", state, memoId);
                break;
            }
            CASE_(TPush) {
                break;
            }
            CASE_(TNew);
            CASE_(TCapture) {
                int8_t shift = *(int8_t *)(p + 1);
                OP_PRINT("%d", shift);
                break;
            }
            CASE_(TLeftFold);
            CASE_(TPop);
            CASE_(TTag);
            CASE_(TCommit) {
                TAG_t tagId = *(TAG_t *)(p + 1);
                tag_t *impl = TAG_GET_IMPL(L->R, tagId);
                OP_PRINT("%s", impl);
                break;
            }
            CASE_(TReplace) {
                asm volatile("int3");
                break;
            }
            CASE_(TStart) {
                break;
            }
            CASE_(TAbort) {
                asm volatile("int3");
                break;
            }
            CASE_(TLookup) {
                int8_t state = *(int8_t *)(p + 1);
                TAG_t tagId = *(TAG_t *)(p + 2);
                uint16_t memoId = *(uint16_t *)(p + 2 + sizeof(TAG_t));
                mozaddr_t skip = *(mozaddr_t *)(p + 4 + sizeof(TAG_t));
                tag_t *impl = TAG_GET_IMPL(L->R, tagId);
                OP_PRINT("%d %d %d %p", state, memoId, skip, impl);
                break;
            }
            CASE_(TMemo) {
                int state = *(int8_t *)(p + 1);
                uint16_t memoId = *(uint16_t *)(p + 2);
                OP_PRINT("%d %d", state, memoId);
                break;
            }
            CASE_(SOpen);
            CASE_(SClose) {
                break;
            }
            CASE_(SIsDef) {
                TAG_t    tagId = *(TAG_t *)(p + 1);
                STRING_t strId = *(STRING_t *)(p + 1 + sizeof(TAG_t));
                tag_t      *impl1 = TBL_GET_IMPL(L->R, tagId);
                const char *impl2 = STRING_GET_IMPL(L->R, strId);
                OP_PRINT("%p %s", impl1, impl2);
                break;
            }

            CASE_(SMask);
            CASE_(SDef);
            CASE_(SExists);
            CASE_(SMatch);
            CASE_(SIs);
            CASE_(SIsa) {
                TAG_t tagId = *(TAG_t *)(p + 1);
                tag_t *impl = TBL_GET_IMPL(L->R, tagId);
                OP_PRINT("%p", impl);
                break;
            }
            CASE_(SDefNum) {
                asm volatile("int3");
            }
            CASE_(SCount) {
                asm volatile("int3");
            }
            CASE_(Exit) {
                // OP_PRINT("%d", *(p + 1));
                break;
            }
            CASE_(Label) {
                break;
            }
#undef CASE_
            }
            OP_PRINT_END()
        }
    }
}
#endif /* LOADER_DEBUG */

#ifdef __cplusplus
}
#endif
