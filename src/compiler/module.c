#include "module.h"
#include "core/karray.h"
#include "core/buffer.h"
#include "ir.h"
#include "expression.h"
#include "block.h"
#define MOZVM_MOZVM2_DUMP 1

#define dump_opcode(out, R, opcode)  fprintf(out, "%s ", IR_TYPE_NAME[opcode]);
#define dump_mozaddr_t(out, R, addr) fprintf(out, "%05d ", addr);
#define dump_int8_t(out, R, i8)      fprintf(out, "0x%x ", i8);
#define dump_uint8_t(out, R, u8)     fprintf(out, "0x%x ", u8);
#define dump_uint16_t(out, R, u16)   fprintf(out, "0x%x ", u16);
#define dump_BITSET_t(out, R, setId) fprintf(out, "set(%d) ", setId);
#define dump_STRING_t(out, R, strId) fprintf(out, "'%s' ", R->C.strs[strId]);
#define dump_TAG_t(out, R, tagId)    fprintf(out, "'%s' ", R->C.tags[tagId]);
#include "vm2_inst.h"
#include "linker.c"

#ifdef __cplusplus
extern "C" {
#endif

DEF_ARRAY_OP_NOPOINTER(decl_ptr_t);
DEF_ARRAY_OP_NOPOINTER(expr_ptr_t);

typedef struct moz_bytecode_writer_t {
    moz_compiler_t *compiler;
    moz_buffer_writer_t writer;
    mozlinker_t linker;
} moz_bytecode_writer_t;

typedef struct moz_vm2_module_t {
    moz_module_t base;
    uint8_t *compiled_code;
    uint8_t *compiled_code_end;
} moz_vm2_module_t;

int moz_vm2_runtime_parse(moz_runtime_t *runtime,
        const moz_inst_t *PC,
        char *head, char *tail);

static int moz_vm2_module_parse(moz_module_t *_M, char *input, size_t input_size, Node **node)
{
    moz_vm2_module_t *M = (moz_vm2_module_t *) _M;
    int result = moz_vm2_runtime_parse(M->base.runtime,
            M->compiled_code,
            input, input + input_size);
    *node = ast_get_parsed_node(M->base.runtime->ast);
    return result;
}

static void moz_vm2_module_dispose(moz_module_t *_M)
{
    moz_vm2_module_t *M = (moz_vm2_module_t *) _M;
    if (M->compiled_code) {
        VM_FREE(M->compiled_code);
        M->compiled_code = NULL;
    }
    moz_runtime_dispose(M->base.runtime);
    VM_FREE(M);
}

static void moz_vm2_module_dump(moz_module_t *_M)
{
    moz_vm2_module_t *M = (moz_vm2_module_t *) _M;
    uint8_t *inst = M->compiled_code;
    while (inst != M->compiled_code_end) {
        fprintf(stderr, "%05ld ", inst - M->compiled_code);
        inst = mozvm2_dump(stderr, M->base.runtime, (moz_inst_t *)inst);
        fprintf(stderr, "\n");
    }
}

static moz_vm2_module_t *moz_vm2_module_new(moz_compiler_t *C, uint8_t *code_begin, unsigned code_size)
{
    moz_vm2_module_t *M = VM_CALLOC(1, sizeof(*M));
    M->base.parse = moz_vm2_module_parse;
    M->base.dump = moz_vm2_module_dump;
    M->base.dispose = moz_vm2_module_dispose;

    M->base.runtime = moz_runtime_init(0);
    if (ARRAY_size(C->sets)) {
        M->base.runtime->C.sets = (bitset_t *) VM_MALLOC(sizeof(bitset_t) * ARRAY_size(C->sets));
        memcpy(M->base.runtime->C.sets, C->sets.list, sizeof(bitset_t) * ARRAY_size(C->sets));
    }
    if (ARRAY_size(C->strs)) {
        unsigned i = 0;
        pstring_t **x, **e;

        M->base.runtime->C.strs = (const char **) VM_MALLOC(sizeof(const char *) * ARRAY_size(C->strs));
        FOR_EACH_ARRAY(C->strs, x, e) {
            M->base.runtime->C.strs[i++] = (*x)->str;
        }
    }
    if (ARRAY_size(C->tags)) {
        unsigned i = 0;
        pstring_t **x, **e;

        M->base.runtime->C.tags = (const char **) VM_MALLOC(sizeof(const char *) * ARRAY_size(C->tags));
        FOR_EACH_ARRAY(C->tags, x, e) {
            M->base.runtime->C.tags[i++] = (*x)->str;
        }
    }
    M->compiled_code = code_begin;
    M->compiled_code_end = M->compiled_code + code_size;
    return M;
}

#define TODO(E) do { \
    moz_inst_dump(W->compiler, E); \
    asm volatile("int3"); \
} while (0)

static void moz_ILabel_encode(moz_bytecode_writer_t *W, ILabel_t *ir)
{
    /* do nothing */
}

static void moz_IExit_encode(moz_bytecode_writer_t *W, IExit_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_IJump_encode(moz_bytecode_writer_t *W, IJump_t *ir)
{
    mozlinker_add_label(&W->linker, &W->writer, ir->v.target, ir->base.id);
}

static void moz_ITableJump_encode(moz_bytecode_writer_t *W, ITableJump_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_IInvoke_encode(moz_bytecode_writer_t *W, IInvoke_t *ir)
{
    mozlinker_add_label(&W->linker, &W->writer, ir->base.fail, ir->base.id);
    mozlinker_add_label(&W->linker, &W->writer, ir->v.decl->inst, ir->base.id);
}

static void moz_IPLoad_encode(moz_bytecode_writer_t *W, IPLoad_t *ir)
{
    /* do nothing */
}

static void moz_IPStore_encode(moz_bytecode_writer_t *W, IPStore_t *ir)
{
    /* do nothing */
}

static void moz_IRet_encode(moz_bytecode_writer_t *W, IRet_t *ir)
{
    /* do nothing */
}

static void moz_IFail_encode(moz_bytecode_writer_t *W, IFail_t *ir)
{
    /* do nothing */
}

static void moz_IAny_encode(moz_bytecode_writer_t *W, IAny_t *ir)
{
    mozlinker_add_label(&W->linker, &W->writer, ir->base.fail, ir->base.id);
}

static void moz_IByte_encode(moz_bytecode_writer_t *W, IByte_t *ir)
{
    mozlinker_add_label(&W->linker, &W->writer, ir->base.fail, ir->base.id);
    moz_buffer_writer_write8(&W->writer, ir->byte);
}

static void moz_IStr_encode(moz_bytecode_writer_t *W, IStr_t *ir)
{
    mozlinker_add_label(&W->linker, &W->writer, ir->base.fail, ir->base.id);
    moz_buffer_writer_write16(&W->writer, ir->strId);
}

static void moz_ISet_encode(moz_bytecode_writer_t *W, ISet_t *ir)
{
    mozlinker_add_label(&W->linker, &W->writer, ir->base.fail, ir->base.id);
    moz_buffer_writer_write16(&W->writer, ir->setId);
}

static void moz_IUByte_encode(moz_bytecode_writer_t *W, IUByte_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_IUSet_encode(moz_bytecode_writer_t *W, IUSet_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_INAny_encode(moz_bytecode_writer_t *W, INAny_t *ir)
{
    mozlinker_add_label(&W->linker, &W->writer, ir->base.fail, ir->base.id);
}

static void moz_INByte_encode(moz_bytecode_writer_t *W, INByte_t *ir)
{
    mozlinker_add_label(&W->linker, &W->writer, ir->base.fail, ir->base.id);
    moz_buffer_writer_write8(&W->writer, ir->byte);
}

static void moz_INStr_encode(moz_bytecode_writer_t *W, INStr_t *ir)
{
    mozlinker_add_label(&W->linker, &W->writer, ir->base.fail, ir->base.id);
    moz_buffer_writer_write16(&W->writer, ir->strId);
}

static void moz_INSet_encode(moz_bytecode_writer_t *W, INSet_t *ir)
{
    mozlinker_add_label(&W->linker, &W->writer, ir->base.fail, ir->base.id);
    moz_buffer_writer_write16(&W->writer, ir->setId);
}

static void moz_IRAny_encode(moz_bytecode_writer_t *W, IRAny_t *ir)
{
    /* do nothing */
}

static void moz_IRByte_encode(moz_bytecode_writer_t *W, IRByte_t *ir)
{
    moz_buffer_writer_write8(&W->writer, ir->byte);
}

static void moz_IRStr_encode(moz_bytecode_writer_t *W, IRStr_t *ir)
{
    moz_buffer_writer_write16(&W->writer, ir->strId);
}

static void moz_IRSet_encode(moz_bytecode_writer_t *W, IRSet_t *ir)
{
    moz_buffer_writer_write16(&W->writer, ir->setId);
}

static void moz_IRUByte_encode(moz_bytecode_writer_t *W, IRUByte_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_IRUSet_encode(moz_bytecode_writer_t *W, IRUSet_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_IOByte_encode(moz_bytecode_writer_t *W, IOByte_t *ir)
{
    moz_buffer_writer_write8(&W->writer, ir->byte);
}

static void moz_IOStr_encode(moz_bytecode_writer_t *W, IOStr_t *ir)
{
    moz_buffer_writer_write16(&W->writer, ir->strId);
}

static void moz_IOSet_encode(moz_bytecode_writer_t *W, IOSet_t *ir)
{
    moz_buffer_writer_write16(&W->writer, ir->setId);
}

static void moz_IOUByte_encode(moz_bytecode_writer_t *W, IOUByte_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_IOUSet_encode(moz_bytecode_writer_t *W, IOUSet_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ILookup_encode(moz_bytecode_writer_t *W, ILookup_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_IMemo_encode(moz_bytecode_writer_t *W, IMemo_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_IMemoFail_encode(moz_bytecode_writer_t *W, IMemoFail_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ITStart_encode(moz_bytecode_writer_t *W, ITStart_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ITCommit_encode(moz_bytecode_writer_t *W, ITCommit_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ITAbort_encode(moz_bytecode_writer_t *W, ITAbort_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ITPush_encode(moz_bytecode_writer_t *W, ITPush_t *ir)
{
    /* do nothing */
}

static void moz_ITPop_encode(moz_bytecode_writer_t *W, ITPop_t *ir)
{
    moz_buffer_writer_write16(&W->writer, ir->tagId);
}

static void moz_ITFoldL_encode(moz_bytecode_writer_t *W, ITFoldL_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ITNew_encode(moz_bytecode_writer_t *W, ITNew_t *ir)
{
    moz_buffer_writer_write8(&W->writer, 0);
}

static void moz_ITCapture_encode(moz_bytecode_writer_t *W, ITCapture_t *ir)
{
    moz_buffer_writer_write8(&W->writer, 0);
}

static void moz_ITTag_encode(moz_bytecode_writer_t *W, ITTag_t *ir)
{
    moz_buffer_writer_write16(&W->writer, ir->tagId);
}

static void moz_ITReplace_encode(moz_bytecode_writer_t *W, ITReplace_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ITLookup_encode(moz_bytecode_writer_t *W, ITLookup_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ITMemo_encode(moz_bytecode_writer_t *W, ITMemo_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ISOpen_encode(moz_bytecode_writer_t *W, ISOpen_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ISClose_encode(moz_bytecode_writer_t *W, ISClose_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ISMask_encode(moz_bytecode_writer_t *W, ISMask_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ISDef_encode(moz_bytecode_writer_t *W, ISDef_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ISIsDef_encode(moz_bytecode_writer_t *W, ISIsDef_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ISExists_encode(moz_bytecode_writer_t *W, ISExists_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ISMatch_encode(moz_bytecode_writer_t *W, ISMatch_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ISIs_encode(moz_bytecode_writer_t *W, ISIs_t *ir)
{
    TODO((IR_t *)ir);
}

static void moz_ISIsa_encode(moz_bytecode_writer_t *W, ISIsa_t *ir)
{
    TODO((IR_t *)ir);
}
typedef void (*f_encode)(moz_bytecode_writer_t *W, IR_t *ir);

static unsigned moz_ir_encode(moz_bytecode_writer_t *W, IR_t *ir)
{
    static const f_encode encode[] = {
#define DEFINE_ENCODE(NAME) (f_encode) moz_##NAME##_encode,
        FOR_EACH_IR(DEFINE_ENCODE)
#undef DEFINE_ENCODE
    };
    unsigned pos = moz_buffer_writer_length(&W->writer);
    moz_buffer_writer_write8(&W->writer, ir->type);
    encode[ir->type](W, ir);
    W->linker.address_head[ir->id] = pos;
    W->linker.address_tail[ir->id] = moz_buffer_writer_length(&W->writer);
    fprintf(stderr, "id:%03d addr=(%u, %u)\n",
            ir->id, pos, moz_buffer_writer_length(&W->writer));
    return pos;
}

static uint8_t *buffer_copy(uint8_t *origin, unsigned size)
{
    uint8_t *code = VM_CALLOC(1, size);
    memcpy(code, origin, size);
    return code;
}

moz_module_t *moz_vm2_module_compile(struct moz_compiler_t *C)
{
    moz_bytecode_writer_t W;
    block_t **I, **E;
    moz_buffer_reader_t R;
    mozaddr_t *address_head;
    mozaddr_t *address_tail;
    moz_vm2_module_t *M;
    uint8_t *code;

    W.compiler = C;
    address_head = (mozaddr_t *) alloca(sizeof(mozaddr_t) * moz_ir_max_id());
    address_tail = (mozaddr_t *) alloca(sizeof(mozaddr_t) * moz_ir_max_id());
    memset(address_head, 0, sizeof(mozaddr_t) * moz_ir_max_id());
    memset(address_tail, 0, sizeof(mozaddr_t) * moz_ir_max_id());
    mozlinker_init(&W.linker, address_head, address_tail, moz_ir_max_id());
    moz_buffer_writer_init(&W.writer, 32);

    FOR_EACH_ARRAY(C->blocks, I, E) {
        IR_t **x, **e;
        FOR_EACH_ARRAY((*I)->insts, x, e) {
            moz_ir_encode(&W, *x);
        }
    }
    moz_buffer_reader_init_from_writer(&R, &W.writer);
    code = buffer_copy(moz_buffer_reader_get_raw_buffer(&R),
            moz_buffer_writer_length(&W.writer));
    mozlinker_resolve(&W.linker, code);
    M = moz_vm2_module_new(C, code, moz_buffer_writer_length(&W.writer));
    moz_buffer_writer_dispose(&W.writer);
    mozlinker_dispose(&W.linker);
    return (moz_module_t *) M;
}

#ifdef __cplusplus
}
#endif
