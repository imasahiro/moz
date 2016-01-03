#ifndef MOZIR_H
#define MOZIR_H

#define FOR_EACH_IR(OP) \
    OP(ILabel) \
    OP(IExit) \
    OP(IJump) \
    OP(ITableJump) \
    OP(IInvoke) \
    OP(IPLoad) \
    OP(IPStore) \
    OP(IRet) \
    OP(IFail) \
    OP(IAny) \
    OP(INAny) \
    OP(IByte) \
    OP(IStr) \
    OP(ISet) \
    OP(IUByte) \
    OP(IUSet) \
    OP(IRByte) \
    OP(IRStr) \
    OP(IRSet) \
    OP(IRUByte) \
    OP(IRUSet) \
    OP(IOByte) \
    OP(IOStr) \
    OP(IOSet) \
    OP(IOUByte) \
    OP(IOUSet) \
    OP(ILookup) \
    OP(IMemo) \
    OP(IMemoFail) \
    OP(ITStart) \
    OP(ITCommit) \
    OP(ITAbort) \
    OP(ITPush) \
    OP(ITPop) \
    OP(ITFoldL) \
    OP(ITNew) \
    OP(ITCapture) \
    OP(ITTag) \
    OP(ITReplace) \
    OP(ITLookup) \
    OP(ITMemo) \
    OP(ISOpen) \
    OP(ISClose) \
    OP(ISMask) \
    OP(ISDef) \
    OP(ISIsDef) \
    OP(ISExists) \
    OP(ISMatch) \
    OP(ISIs) \
    OP(ISIsa)

typedef enum ir_type {
#define DEFINE_ENUM(NAME) NAME,
    FOR_EACH_IR(DEFINE_ENUM)
#undef DEFINE_ENUM
    MAX_IR
} ir_type_t;

#define VMIR_BASE IR_t base

typedef struct IR {
    unsigned id;
    ir_type_t type;
    struct block_t *parent;
    struct block_t *fail;
} IR_t;

typedef struct ILabel {
    VMIR_BASE;
} ILabel_t;

typedef struct IExit {
    VMIR_BASE;
    uint8_t exitStatus;
} IExit_t;

typedef struct IJump {
    VMIR_BASE;
    union target {
        mozaddr_t jumpAddr;
        struct block_t *target;
    } v;
} IJump_t;

typedef struct ITableJump {
    VMIR_BASE;
    uint16_t tblId;
} ITableJump_t;

typedef struct IInvoke {
    VMIR_BASE;
    union val {
        decl_t *decl;
        mozaddr_t addr;
    } v;
} IInvoke_t;

typedef struct IPLoad {
    VMIR_BASE;
} IPLoad_t;

typedef struct IPStore {
    VMIR_BASE;
} IPStore_t;

typedef struct IRet {
    VMIR_BASE;
} IRet_t;

typedef struct IFail {
    VMIR_BASE;
} IFail_t;

typedef struct IAny {
    VMIR_BASE;
} IAny_t;

typedef struct INAny {
    VMIR_BASE;
} INAny_t;

typedef struct IByte {
    VMIR_BASE;
    uint8_t byte;
} IByte_t;

typedef struct IStr {
    VMIR_BASE;
    STRING_t strId;
} IStr_t;

typedef struct ISet {
    VMIR_BASE;
    BITSET_t setId;
} ISet_t;

typedef struct IUByte {
    VMIR_BASE;
    unsigned byte;
} IUByte_t;

typedef struct IUSet {
    VMIR_BASE;
    BITSET_t setId;
} IUSet_t;

typedef struct IRByte {
    VMIR_BASE;
    uint8_t byte;
} IRByte_t;

typedef struct IRStr {
    VMIR_BASE;
    STRING_t strId;
} IRStr_t;

typedef struct IRSet {
    VMIR_BASE;
    BITSET_t setId;
} IRSet_t;

typedef struct IRUByte {
    VMIR_BASE;
    unsigned byte;
} IRUByte_t;

typedef struct IRUSet {
    VMIR_BASE;
    BITSET_t setId;
} IRUSet_t;

typedef struct IOByte {
    VMIR_BASE;
    uint8_t byte;
} IOByte_t;

typedef struct IOStr {
    VMIR_BASE;
    STRING_t strId;
} IOStr_t;

typedef struct IOSet {
    VMIR_BASE;
    BITSET_t setId;
} IOSet_t;

typedef struct IOUByte {
    VMIR_BASE;
    unsigned byte;
} IOUByte_t;

typedef struct IOUSet {
    VMIR_BASE;
    BITSET_t setId;
} IOUSet_t;

typedef struct ILookup {
    VMIR_BASE;
    uint8_t state;
    uint16_t memoId;
    mozaddr_t skip;
} ILookup_t;

typedef struct IMemo {
    VMIR_BASE;
    uint8_t state;
    uint16_t memoId;
} IMemo_t;

typedef struct IMemoFail {
    VMIR_BASE;
    uint8_t state;
    uint16_t memoId;
} IMemoFail_t;

typedef struct ITStart {
    VMIR_BASE;
} ITStart_t;

typedef struct ITCommit {
    VMIR_BASE;
    TAG_t tagId;
} ITCommit_t;

typedef struct ITAbort {
    VMIR_BASE;
} ITAbort_t;

typedef struct ITPush {
    VMIR_BASE;
} ITPush_t;

typedef struct ITPop {
    VMIR_BASE;
    TAG_t tagId;
} ITPop_t;

typedef struct ITFoldL {
    VMIR_BASE;
    int8_t shift;
    TAG_t tagId;
} ITFoldL_t;

typedef struct ITNew {
    VMIR_BASE;
    int8_t shift;
} ITNew_t;

typedef struct ITCapture {
    VMIR_BASE;
    int8_t shift;
} ITCapture_t;

typedef struct ITTag {
    VMIR_BASE;
    TAG_t tagId;
} ITTag_t;

typedef struct ITReplace {
    VMIR_BASE;
    STRING_t strId;
} ITReplace_t;

typedef struct ITLookup {
    VMIR_BASE;
    uint8_t state;
    TAG_t tagId;
    uint16_t memoId;
    mozaddr_t skip;
} ITLookup_t;

typedef struct ITMemo {
    VMIR_BASE;
    uint8_t state;
    uint16_t memoId;
} ITMemo_t;

typedef struct ISOpen {
    VMIR_BASE;
} ISOpen_t;

typedef struct ISClose {
    VMIR_BASE;
} ISClose_t;

typedef struct ISMask {
    VMIR_BASE;
    TAG_t tagId;
} ISMask_t;

typedef struct ISDef {
    VMIR_BASE;
    TAG_t tagId;
} ISDef_t;

typedef struct ISIsDef {
    VMIR_BASE;
    TAG_t tagId;
    STRING_t strId;
} ISIsDef_t;

typedef struct ISExists {
    VMIR_BASE;
    TAG_t tagId;
} ISExists_t;

typedef struct ISMatch {
    VMIR_BASE;
    TAG_t tagId;
} ISMatch_t;

typedef struct ISIs {
    VMIR_BASE;
    TAG_t tagId;
} ISIs_t;

typedef struct ISIsa {
    VMIR_BASE;
    TAG_t tagId;
} ISIsa_t;

#endif /* end of include guard */
