#ifndef MOZIR_H
#define MOZIR_H

#define VMIR_BASE unsigned id

struct ILabel {
    VMIR_BASE;
} ILabel_t;

struct IExit {
    VMIR_BASE;
    uint8_t exitStatus;
} IExit_t;

struct IJump {
    VMIR_BASE;
    mozaddr_t jumpAddr;
} IJump_t;

struct ITableJump {
    VMIR_BASE;
    uint16_t tblId;
} ITableJump_t;

struct IInvoke {
    VMIR_BASE;
    mozaddr_t productAddr;
} IInvoke_t;

struct IRet {
    VMIR_BASE;
} IRet_t;

struct IAny {
    VMIR_BASE;
} IAny_t;

struct INAny {
    VMIR_BASE;
} INAny_t;

struct IByte {
    VMIR_BASE;
    uint8_t byte;
} IByte_t;

struct IStr {
    VMIR_BASE;
    STRING_t strId;
} IStr_t;

struct ISet {
    VMIR_BASE;
    BITSET_t setId;
} ISet_t;

struct IUByte {
    VMIR_BASE;
    unsigned byte;
} IUByte_t;

struct IUSet {
    VMIR_BASE;
    BITSET_t setId;
} IUSet_t;

struct IRByte {
    VMIR_BASE;
    uint8_t byte;
} IRByte_t;

struct IRStr {
    VMIR_BASE;
    STRING_t strId;
} IRStr_t;

struct IRSet {
    VMIR_BASE;
    BITSET_t setId;
} IRSet_t;

struct IRUByte {
    VMIR_BASE;
    unsigned byte;
} IRUByte_t;

struct IRUSet {
    VMIR_BASE;
    BITSET_t setId;
} IRUSet_t;

struct ILookup {
    VMIR_BASE;
    uint8_t state;
    uint16_t memoId;
    mozaddr_t skip;
} ILookup_t;

struct IMemo {
    VMIR_BASE;
    uint8_t state;
    uint16_t memoId;
} IMemo_t;

struct IMemoFail {
    VMIR_BASE;
    uint8_t state;
    uint16_t memoId;
} IMemoFail_t;

struct ITStart {
    VMIR_BASE;
} ITStart_t;

struct ITCommit {
    VMIR_BASE;
    TAG_t tagId;
} ITCommit_t;

struct ITAbort {
    VMIR_BASE;
} ITAbort_t;

struct ITPush {
    VMIR_BASE;
} ITPush_t;

struct ITPop {
    VMIR_BASE;
    TAG_t tagId;
} ITPop_t;

struct ITFoldL {
    VMIR_BASE;
    int8_t shift;
    TAG_t tagId;
} ITFoldL_t;

struct ITNew {
    VMIR_BASE;
    int8_t shift;
} ITNew_t;

struct ITCapture {
    VMIR_BASE;
    int8_t shift;
} ITCapture_t;

struct ITTag {
    VMIR_BASE;
    TAG_t tagId;
} ITTag_t;

struct ITReplace {
    VMIR_BASE;
    STRING_t strId;
} ITReplace_t;

struct ITLookup {
    VMIR_BASE;
    uint8_t state;
    TAG_t tagId;
    uint16_t memoId;
    mozaddr_t skip;
} ITLookup_t;

struct ITMemo {
    VMIR_BASE;
    uint8_t state;
    uint16_t memoId;
} ITMemo_t;

struct ISOpen {
    VMIR_BASE;
} ISOpen_t;

struct ISClose {
    VMIR_BASE;
} ISClose_t;

struct ISMask {
    VMIR_BASE;
    TAG_t tagId;
} ISMask_t;

struct ISDef {
    VMIR_BASE;
    TAG_t tagId;
} ISDef_t;

struct ISIsDef {
    VMIR_BASE;
    TAG_t tagId;
    STRING_t strId;
} ISIsDef_t;

struct ISExists {
    VMIR_BASE;
    TAG_t tagId;
} ISExists_t;

struct ISMatch {
    VMIR_BASE;
    TAG_t tagId;
} ISMatch_t;

struct ISIs {
    VMIR_BASE;
    TAG_t tagId;
} ISIs_t;

struct ISIsa {
    VMIR_BASE;
    TAG_t tagId;
} ISIsa_t;

#endif /* end of include guard */
