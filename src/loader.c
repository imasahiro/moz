#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include "bitset.c"
#include "pstring.h"
#include "instruction.h"

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

static char *load_file(const char *path, size_t *size)
{
    size_t len;
    size_t readed;
    char *data;
    FILE *fp = fopen(path, "rb");
    assert(fp != 0);

    fseek(fp, 0, SEEK_END);
    len = (size_t) ftell(fp);
    fseek(fp, 0, SEEK_SET);
    data = (char *) calloc(1, len + 1);
    readed = fread(data, 1, len, fp);
    assert(len == readed);
    fclose(fp);
    *size = len;
    return data;
    (void)readed;
}

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

static uint32_t read32(input_stream_t *is)
{
    uint32_t d1 = read16(is);
    uint32_t d2 = read16(is);
    return d1 << 16 | d2;
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

static void loadInst(input_stream_t *is)
{
    uint8_t opcode = read8(is);
    int has_jump = opcode & 0x80;
    opcode = opcode & 0x7f;
#define CASE_(OP) case OP:
    fprintf(stderr, "***%s %d\n", opcode2str(opcode), has_jump > 0);
    switch (opcode) {
    CASE_(Nop) {
        asm volatile("int3");
    }
    CASE_(Fail) {
        asm volatile("int3");
    }
    CASE_(Alt) {
        int failjump = read32(is);
        fprintf(stderr, "alt %d\n", failjump);
        break;
    }
    CASE_(Succ) {
        asm volatile("int3");
    }
    CASE_(Jump) {
        asm volatile("int3");
    }
    CASE_(Call) {
        int label = read32(is);
        int next  = 0;
        assert(has_jump != 0);
        next = read32(is);
        fprintf(stderr, "call %d %d\n", label, next);
        break;
    }
    CASE_(Ret) {
        fprintf(stderr, "ret\n");
        break;
    }
    CASE_(Pos) {
        asm volatile("int3");
    }
    CASE_(Back) {
        asm volatile("int3");
    }
    CASE_(Skip) {
        int next = read32(is);
        fprintf(stderr, "skip %d\n", next);
        break;
    }
    CASE_(Byte) {
        asm volatile("int3");
    }
    CASE_(Any) {
        fprintf(stderr, "any\n");
        break;
    }
    CASE_(Str) {
        asm volatile("int3");
    }
    CASE_(Set) {
        asm volatile("int3");
    }
    CASE_(NByte) {
        asm volatile("int3");
    }
    CASE_(NAny) {
        fprintf(stderr, "nany\n");
        break;
    }
    CASE_(NStr) {
        asm volatile("int3");
    }
    CASE_(NSet) {
        asm volatile("int3");
    }
    CASE_(OByte) {
        asm volatile("int3");
    }
    CASE_(OAny) {
        asm volatile("int3");
    }
    CASE_(OStr) {
        asm volatile("int3");
    }
    CASE_(OSet) {
        asm volatile("int3");
    }
    CASE_(RByte) {
        asm volatile("int3");
    }
    CASE_(RAny) {
        asm volatile("int3");
    }
    CASE_(RStr) {
        asm volatile("int3");
    }
    CASE_(RSet) {
        asm volatile("int3");
    }
    CASE_(Consume) {
        asm volatile("int3");
    }
    CASE_(First) {
        asm volatile("int3");
    }
    CASE_(Lookup) {
        asm volatile("int3");
    }
    CASE_(Memo) {
        asm volatile("int3");
    }
    CASE_(MemoFail) {
        asm volatile("int3");
    }
    CASE_(TPush) {
        asm volatile("int3");
    }
    CASE_(TPop) {
        asm volatile("int3");
    }
    CASE_(TLeftFold) {
        asm volatile("int3");
    }
    CASE_(TNew) {
        asm volatile("int3");
    }
    CASE_(TCapture) {
        asm volatile("int3");
    }
    CASE_(TTag) {
        asm volatile("int3");
    }
    CASE_(TReplace) {
        asm volatile("int3");
    }
    CASE_(TStart) {
        asm volatile("int3");
    }
    CASE_(TCommit) {
        asm volatile("int3");
    }
    CASE_(TAbort) {
        asm volatile("int3");
    }
    CASE_(TLookup) {
        asm volatile("int3");
    }
    CASE_(TMemo) {
        asm volatile("int3");
    }
    CASE_(SOpen) {
        asm volatile("int3");
    }
    CASE_(SClose) {
        asm volatile("int3");
    }
    CASE_(SMask) {
        asm volatile("int3");
    }
    CASE_(SDef) {
        asm volatile("int3");
    }
    CASE_(SIsDef) {
        asm volatile("int3");
    }
    CASE_(SExists) {
        asm volatile("int3");
    }
    CASE_(SMatch) {
        asm volatile("int3");
    }
    CASE_(SIs) {
        asm volatile("int3");
    }
    CASE_(SIsa) {
        asm volatile("int3");
    }
    CASE_(SDefNum) {
        asm volatile("int3");
    }
    CASE_(SCount) {
        asm volatile("int3");
    }
    CASE_(Exit) {
        asm volatile("int3");
    }
    CASE_(Label) {
        uint16_t len = read16(is);
        asm volatile("int3");
        char *label = peek(is); skip(is, len);
        (void)len; (void)label;
        break;
    }
    }
#undef CASE_
}

typedef struct moz_bytecode_t {
    /* header */
    unsigned inst_size;
    unsigned prod_size;
    unsigned memo_size;
    /* const data */
    uint16_t nterm_size;
    const char **nterms;
    uint16_t set_size;
    bitset_t *sets;
    uint16_t str_size;
    const char **strs;
    uint16_t tag_size;
    const char **tags;
    uint16_t table_size;
    void *table;
    // uint16_t sym_size;
    // void *syms;
} moz_bytecode_t;

int main(int argc, char const* argv[])
{
    unsigned i;
    input_stream_t is;
    moz_bytecode_t bc = {};
    is.pos = 0;
    is.data = load_file(argv[1], &is.end);
    assert(checkFileType(&is));
    assert(checkVersion(&is));
    bc.inst_size = (unsigned) read16(&is);
    bc.prod_size = (unsigned) read16(&is);
    bc.memo_size = (unsigned) read16(&is);

    bc.nterm_size = read16(&is);
    if (bc.nterm_size > 0) {
        bc.nterms = (const char **)malloc(sizeof(const char *) * bc.nterm_size);
        for (i = 0; i < bc.nterm_size; i++) {
            uint16_t len = read16(&is);
            char *str = peek(&is);
            skip(&is, len + 1);
            bc.nterms[i] = pstring_alloc(str, (unsigned)len);
            fprintf(stderr, "nterm%d %s\n", i, bc.nterms[i]);
        }
    }

    bc.set_size = read16(&is);
    if (bc.set_size > 0) {
        bc.sets = (bitset_t *) malloc(sizeof(bitset_t) * bc.set_size);
#define INT_BIT (sizeof(int) * CHAR_BIT)
#define N (256 / INT_BIT)
        for (i = 0; i < bc.set_size; i++) {
            unsigned j;
            bitset_t *set = &bc.sets[i];
            bitset_init(set);
            for (j = 0; j < N; j+= 2) {
                unsigned low  = read32(&is);
                unsigned high = read32(&is);
                for (int i = 0; i < INT_BIT; i++) {
                    unsigned mask = 1U << i;
                    if ((low & mask) == mask)
                        bitset_set(set, i);
                    if ((high & mask) == mask)
                        bitset_set(set, i + INT_BIT);
                }
            }
            if (0) {
                // asm volatile("int3");
                fprintf(stderr, "%d\n", bitset_get(set, '0'));
            }
        }
#undef N
    }
    bc.str_size = read16(&is);
    if (bc.str_size > 0) {
        bc.strs = (const char **)malloc(sizeof(const char *) * bc.str_size);
        // assert(0 && "we do not have any specification about set field");
    }
    bc.tag_size = read16(&is);
    if (bc.tag_size > 0) {
        bc.tags = (const char **)malloc(sizeof(const char *) * bc.tag_size);
        for (i = 0; i < bc.tag_size; i++) {
            uint16_t len = read16(&is);
            char *str = peek(&is);
            skip(&is, len + 1);
            bc.tags[i] = pstring_alloc(str, (unsigned)len);
            fprintf(stderr, "tag%d %s\n", i, bc.tags[i]);
        }
    }
    bc.table_size = read16(&is);
    if (bc.table_size > 0) {
        bc.table = peek(&is);
        assert(0 && "we do not have any specification about set field");
    }
    // bc.sym_size = read16(&is);
    // if (bc.sym_size > 0) {
    //     bc.syms = peek(&is);
    //     // assert(0 && "we do not have any specification about set field");
    // }

#define PRINT_FIELD(O, FIELD) \
    fprintf(stderr, "O->" #FIELD " = %d\n", (O).FIELD)
    PRINT_FIELD(bc, inst_size);
    PRINT_FIELD(bc, prod_size);
    PRINT_FIELD(bc, memo_size);
    PRINT_FIELD(bc, nterm_size);
    PRINT_FIELD(bc, set_size);
    PRINT_FIELD(bc, str_size);
    PRINT_FIELD(bc, tag_size);
    PRINT_FIELD(bc, table_size);
    // PRINT_FIELD(bc, sym_size);
    i = 0;
    while (is.pos + i < is.end) {
        fprintf(stderr, "%02x ", (uint8_t)(is.data[is.pos + i]));
        if (i != 0 && i % 16 == 15) {
            fprintf(stderr, "\n");
        }
        i++;
    }
    fprintf(stderr, "\n");
    while (is.pos < is.end) {
        loadInst(&is);
    }
    return 0;
}
