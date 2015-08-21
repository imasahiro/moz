#include <stdint.h>
#ifndef MOZ_INSTRUCTION_H
#define MOZ_INSTRUCTION_H

enum MozOpcode {
    Nop  = 0,  // Do nothing
    Fail = 1,  // Fail
    Alt  = 2,  // Alt
    Succ = 3,  // Succ
    Jump = 4,  // Jump
    Call = 5,  // Call
    Ret  = 6,  // Ret
    Pos  = 7,  // Pos
    Back = 8,  // Back
    Skip = 9,  // Skip

    Byte     = 10,  // match a byte character
    Any      = 11,  // match any
    Str      = 12,  // match string
    Set      = 13,  // match set
    NByte    = 14,  //
    NAny     = 15,  //
    NStr     = 16,  //
    NSet     = 17,  //
    OByte    = 18,  //
    OAny     = 19,  //
    OStr     = 20,  //
    OSet     = 21,  //
    RByte    = 22,  //
    RAny     = 23,  //
    RStr     = 24,  //
    RSet     = 25,  //

    Consume  = 26,  //
    First    = 27,  //

    Lookup   = 28,  // match a character
    Memo     = 29,  // match a character
    MemoFail = 30,  // match a character

    TPush    = 31,
    TPop     = 32,
    TLeftFold    = 33,
    TNew     = 34,
    TCapture = 35,
    TTag     = 36,
    TReplace = 37,
    TStart   = 38,
    TCommit  = 39,
    TAbort   = 40,

    TLookup   = 41,
    TMemo     = 42,


    SOpen    = 43,
    SClose   = 44,
    SMask    = 45,
    SDef     = 46,
    SIsDef   = 47,
    SExists  = 48,
    SMatch   = 49,
    SIs      = 50,
    SIsa     = 51,
    SDefNum  = 52,
    SCount   = 53,
    Exit     = 54,   // 7-bit only


    Label    = 127,  // 7-bit
};

#define OP_EACH(F)  \
    F(Nop)\
    F(Fail)\
    F(Alt)\
    F(Succ)\
    F(Jump)\
    F(Call)\
    F(Ret)\
    F(Pos)\
    F(Back)\
    F(Skip)\
    F(Byte)\
    F(Any)\
    F(Str)\
    F(Set)\
    F(NByte)\
    F(NAny)\
    F(NStr)\
    F(NSet)\
    F(OByte)\
    F(OAny)\
    F(OStr)\
    F(OSet)\
    F(RByte)\
    F(RAny)\
    F(RStr)\
    F(RSet)\
    F(Consume)\
    F(First)\
    F(Lookup)\
    F(Memo)\
    F(MemoFail)\
    F(TPush)\
    F(TPop)\
    F(TLeftFold)\
    F(TNew)\
    F(TCapture)\
    F(TTag)\
    F(TReplace)\
    F(TStart)\
    F(TCommit)\
    F(TAbort)\
    F(TLookup)\
    F(TMemo)\
    F(SOpen)\
    F(SClose)\
    F(SMask)\
    F(SDef)\
    F(SIsDef)\
    F(SExists)\
    F(SMatch)\
    F(SIs)\
    F(SIsa)\
    F(SDefNum)\
    F(SCount)\
    F(Exit)\
    F(Label)

#ifdef MOZVM_DUMP_OPCODE
static const char *opcode2str(int opcode)
{
    switch (opcode) {
#define CASE_(OP) case OP: return #OP;
        OP_EACH(CASE_)
#undef CASE_
    }
    return "???";
}
#endif /*MOZVM_DUMP_OPCODE*/
#endif /* end of include guard */
