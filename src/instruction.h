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


#ifdef MOZVM_DUMP_OPCODE
static const char *opcode2str(int opcode)
{
    switch (opcode) {
#define CASE_(OP) case OP: return #OP;
    CASE_(Nop);
    CASE_(Fail);
    CASE_(Alt);
    CASE_(Succ);
    CASE_(Jump);
    CASE_(Call);
    CASE_(Ret);
    CASE_(Pos);
    CASE_(Back);
    CASE_(Skip);
    CASE_(Byte);
    CASE_(Any);
    CASE_(Str);
    CASE_(Set);
    CASE_(NByte);
    CASE_(NAny);
    CASE_(NStr);
    CASE_(NSet);
    CASE_(OByte);
    CASE_(OAny);
    CASE_(OStr);
    CASE_(OSet);
    CASE_(RByte);
    CASE_(RAny);
    CASE_(RStr);
    CASE_(RSet);
    CASE_(Consume);
    CASE_(First);
    CASE_(Lookup);
    CASE_(Memo);
    CASE_(MemoFail);
    CASE_(TPush);
    CASE_(TPop);
    CASE_(TLeftFold);
    CASE_(TNew);
    CASE_(TCapture);
    CASE_(TTag);
    CASE_(TReplace);
    CASE_(TStart);
    CASE_(TCommit);
    CASE_(TAbort);
    CASE_(TLookup);
    CASE_(TMemo);
    CASE_(SOpen);
    CASE_(SClose);
    CASE_(SMask);
    CASE_(SDef);
    CASE_(SIsDef);
    CASE_(SExists);
    CASE_(SMatch);
    CASE_(SIs);
    CASE_(SIsa);
    CASE_(SDefNum);
    CASE_(SCount);
    CASE_(Exit);
    CASE_(Label);
#undef CASE_
    }
    return "???";
}
#endif /*MOZVM_DUMP_OPCODE*/
#endif /* end of include guard */
