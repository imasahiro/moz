#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>

#include "pstring.h"
#define MOZVM_DUMP_OPCODE 1
#include "instruction.h"
#include "mozvm.h"

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#ifdef __cplusplus
extern "C" {
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

static uint16_t read24(input_stream_t *is)
{
    uint16_t d1 = read8(is);
    uint16_t d2 = read8(is);
    uint16_t d3 = read8(is);
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

static void loadInst(input_stream_t *is, int id)
{
    uint8_t opcode = read8(is);
    int has_jump = opcode & 0x80;
    opcode = opcode & 0x7f;
#define CASE_(OP) case OP:
    if (0) {
        fprintf(stderr, "---%s %d\n", opcode2str(opcode), has_jump > 0);
    }
    switch (opcode) {
    CASE_(Nop) {
        asm volatile("int3");
    }
    CASE_(Fail) {
        fprintf(stderr, "%d fail\n", id);
        break;
    }
    CASE_(Alt) {
        int failjump = read24(is);
        fprintf(stderr, "%d alt %d\n", id, failjump);
        break;
    }
    CASE_(Succ) {
        fprintf(stderr, "%d fail\n", id);
        break;
    }
    CASE_(Jump) {
        asm volatile("int3");
    }
    CASE_(Call) {
        int jump  = read24(is);
        int nterm = 0;
        int next  = 0;
        nterm = read16(is);
        next  = get_next(is, &has_jump);
        fprintf(stderr, "%d call jmp=%d nterm=%d next=%d\n", id, jump, nterm, next);
        break;
    }
    CASE_(Ret) {
        fprintf(stderr, "%d ret\n", id);
        break;
    }
    CASE_(Pos) {
        fprintf(stderr, "%d pos\n", id);
        break;
    }
    CASE_(Back) {
        fprintf(stderr, "%d back\n", id);
        break;
    }
    CASE_(Skip) {
        int next = get_next(is, &has_jump);
        fprintf(stderr, "%d skip %d\n", id, next);
        break;
    }
    CASE_(Byte) {
        uint8_t ch = read8(is);
        fprintf(stderr, "%d byte '%c'\n", id, ch);
        break;
    }
    CASE_(Any) {
        fprintf(stderr, "%d any\n", id);
        break;
    }
    CASE_(Str) {
        asm volatile("int3");
    }
    CASE_(Set) {
        uint16_t set = read16(is);
        fprintf(stderr, "%d set %d\n", id, set);
        break;
    }
    CASE_(NByte) {
        uint8_t ch = read8(is);
        fprintf(stderr, "%d nbyte '%c'\n", id, ch);
        break;
    }
    CASE_(NAny) {
        asm volatile("int3");
        fprintf(stderr, "%d nany\n", id);
        break;
    }
    CASE_(NStr) {
        asm volatile("int3");
    }
    CASE_(NSet) {
        uint16_t set = read16(is);
        fprintf(stderr, "%d nset %d\n", id, set);
        break;
    }
    CASE_(OByte) {
        uint8_t ch = read8(is);
        fprintf(stderr, "%d obyte '%c'\n", id, ch);
        break;
    }
    CASE_(OAny) {
        asm volatile("int3");
    }
    CASE_(OStr) {
        asm volatile("int3");
    }
    CASE_(OSet) {
        uint16_t set = read16(is);
        fprintf(stderr, "%d oset %d\n", id, set);
        break;
    }
    CASE_(RByte) {
        uint8_t ch = read8(is);
        fprintf(stderr, "%d rbyte '%c'\n", id, ch);
        break;
    }
    CASE_(RAny) {
        fprintf(stderr, "%d rany\n", id);
    }
    CASE_(RStr) {
        asm volatile("int3");
    }
    CASE_(RSet) {
        uint16_t set = read16(is);
        // dump_set(set, buf);
        fprintf(stderr, "%d rset %d\n", id, set);
        break;
    }
    CASE_(Consume) {
        asm volatile("int3");
    }
    CASE_(First) {
        int i, table[257] = {};
        for (i = 0; i < 257; i++) {
            table[i] = read24(is);
        }
        fprintf(stderr, "%d first\n", id);
        break;
    }
    CASE_(Lookup) {
        int state = read8(is);
        uint32_t memoId = read32(is);
        int skip = read24(is);
        fprintf(stderr, "%d lookup %d %d %d\n", id, state, memoId, skip);
        break;
    }
    CASE_(Memo) {
        int state = read8(is);
        uint32_t memoId = read32(is);
        fprintf(stderr, "%d memo %d %d\n", id, state, memoId);
        break;
    }
    CASE_(MemoFail) {
        int state = read8(is);
        uint32_t memoId = read32(is);
        fprintf(stderr, "%d memofail %d %d\n", id, state, memoId);
        break;
    }
    CASE_(TPush) {
        asm volatile("int3");
        fprintf(stderr, "%d tpush\n", id);
        break;
    }
    CASE_(TPop) {
        int index = read8(is);
        asm volatile("int3");
        fprintf(stderr, "%d tpop %d\n", id, index);
        break;
    }
    CASE_(TLeftFold) {
        uint8_t shift = read8(is);
        fprintf(stderr, "%d tleftfold %d\n", id, shift);
        break;
    }
    CASE_(TNew) {
        uint8_t shift = read8(is);
        fprintf(stderr, "%d tnew %d\n", id, shift);
        break;
    }
    CASE_(TCapture) {
        uint8_t shift = read8(is);
        fprintf(stderr, "%d tcap %d\n", id, shift);
        break;
    }
    CASE_(TTag) {
        uint16_t tagId = read16(is);
        fprintf(stderr, "%d ttag %d\n", id, tagId);
        break;
    }
    CASE_(TReplace) {
        asm volatile("int3");
        break;
    }
    CASE_(TStart) {
        fprintf(stderr, "%d tstart\n", id);
        break;
    }
    CASE_(TCommit) {
        int index = read8(is);
        fprintf(stderr, "%d tcommit %d\n", id, index);
        break;
    }
    CASE_(TAbort) {
        asm volatile("int3");
        break;
    }
    CASE_(TLookup) {
        int state = read8(is);
        uint32_t memoId = read32(is);
        int skip = read24(is);
        int index = read8(is);
        fprintf(stderr, "%d tlookup %d %d %d %d\n", id, state, memoId, skip, index);
        break;
    }
    CASE_(TMemo) {
        int state = read8(is);
        uint32_t memoId = read32(is);
        fprintf(stderr, "%d tlookup %d %d\n", id, state, memoId);
        break;
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
        uint8_t status = read8(is);
        fprintf(stderr, "%d exit %d\n", id, status);
    }
    CASE_(Label) {
        uint16_t label = read16(is);
        fprintf(stderr, "%d label %d\n", id, label);
        (void)label;
        break;
    }
    }
#undef CASE_
    if (has_jump) {
        int jump = get_next(is, &has_jump);
        fprintf(stderr, "- jump %d\n", jump);
    }
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
    bc.memo_size = (unsigned) read16(&is);
    bc.jumptable_size = (unsigned) read16(&is);

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
            unsigned j, k;
            char buf[512] = {};
            bitset_t *set = &bc.sets[i];
            bitset_init(set);
            for (j = 0; j < 256/INT_BIT; j++) {
                unsigned v = read32(&is);
                for (k = 0; k < INT_BIT; k++) {
                    unsigned mask = 1U << k;
                    if ((v & mask) == mask)
                        bitset_set(set, k + INT_BIT * j);
                }
            }
            dump_set(set, buf);
            fprintf(stderr, "set: %s\n", buf);
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
    PRINT_FIELD(bc, memo_size);
    PRINT_FIELD(bc, jumptable_size);
    PRINT_FIELD(bc, nterm_size);
    PRINT_FIELD(bc, set_size);
    PRINT_FIELD(bc, str_size);
    PRINT_FIELD(bc, tag_size);
    PRINT_FIELD(bc, table_size);
    if (0) {
        i = 0;
        while (is.pos + i < is.end) {
            fprintf(stderr, "%02x ", (uint8_t)(is.data[is.pos + i]));
            if (i != 0 && i % 16 == 15) {
                fprintf(stderr, "\n");
            }
            i++;
        }
        fprintf(stderr, "\n");
    }
    i = 0;
    while (is.pos < is.end) {
        loadInst(&is, i++);
    }
    return 0;
}


#ifdef __cplusplus
}
#endif
