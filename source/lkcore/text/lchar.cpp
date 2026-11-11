#include "lcharconst.h"
#include "lkcore/lstringutil.h"
#include <cctype>

LK_NAMESPACE_BEGIN
namespace lchar {

#define FLAG(x) (1 << (x))

const Properties* Util::properties(uint32_t ucs4) noexcept
{
    if(ucs4 < 0x11000){
        uint32_t id = uc_property_trie[uc_property_trie[ucs4>>5] + (ucs4 & 0x1f)];
        return &uc_properties[id];
    }else{
        uint32_t id = uc_property_trie[uc_property_trie[((ucs4 - 0x11000)>>8) + 0x880] + (ucs4 & 0xff)];
        return &uc_properties[id];
    }
}

const Properties* Util::properties(uint16_t ucs2) noexcept
{
    uint32_t id = uc_property_trie[uc_property_trie[ucs2>>5] + (ucs2 & 0x1f)];
    return &uc_properties[id];
}


GraphemeBreakClass Util::graphemeBreakClass(uint32_t ucs4) noexcept
{
    return static_cast<GraphemeBreakClass>(properties(ucs4)->graphemeBreakClass);
}

WordBreakClass Util::wordBreakClass(uint32_t ucs4) noexcept
{
    return static_cast<WordBreakClass>(properties(ucs4)->wordBreakClass);
}

SentenceBreakClass Util::sentenceBreakClass(uint32_t ucs4) noexcept
{
    return static_cast<SentenceBreakClass>(properties(ucs4)->sentenceBreakClass);
}

LineBreakClass Util::lineBreakClass(uint32_t ucs4) noexcept
{
    return static_cast<LineBreakClass>(properties(ucs4)->lineBreakClass);
}


namespace GB {

/*
 * Most grapheme break rules can be implemented table driven, but rules GB10, GB12 and GB13 need a bit
 * of special treatment.
 */
enum State : uint8_t {
    Break,
    Inside,
    GB10,
    GB10_2,
    GB10_3,
    GB13, // also covers GB12
};

static const State breakTable[NumGraphemeBreakClasses][NumGraphemeBreakClasses] = {
    //    Any     CR      LF     Control  Extend  ZWJ     RI     Prepend  S-Mark  L       V       T       LV      LVT     E_B     E_M     GAZ     EBG
    { Break , Break , Break , Break , Inside, Inside, Break , Break , Inside, Break , Break , Break , Break , Break , Break , Break , Break , Break  }, // Any
    { Break , Break , Inside, Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break  }, // CR
    { Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break  }, // LF
    { Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break , Break  }, // Control
    { Break , Break , Break , Break , GB10_2, Inside, Break , Break , Inside, Break , Break , Break , Break , Break , Break , GB10_3, Break , Break  }, // Extend
    { Break , Break , Break , Break , Inside, Inside, Break , Break , Inside, Break , Break , Break , Break , Break , Break , Break , Inside, Inside }, // ZWJ
    { Break , Break , Break , Break , Inside, Inside, GB13  , Break , Inside, Break , Break , Break , Break , Break , Break , Break , Break , Break  }, // RegionalIndicator
    { Inside, Break , Break , Break , Inside, Inside, Inside, Inside, Inside, Inside, Inside, Inside, Inside, Inside, Inside, Inside, Inside, Inside }, // Prepend
    { Break , Break , Break , Break , Inside, Inside, Break , Break , Inside, Break , Break , Break , Break , Break , Break , Break , Break , Break  }, // SpacingMark
    { Break , Break , Break , Break , Inside, Inside, Break , Break , Inside, Inside, Inside, Break , Inside, Inside, Break , Break , Break , Break  }, // L
    { Break , Break , Break , Break , Inside, Inside, Break , Break , Inside, Break , Inside, Inside, Break , Break , Break , Break , Break , Break  }, // V
    { Break , Break , Break , Break , Inside, Inside, Break , Break , Inside, Break , Break , Inside, Break , Break , Break , Break , Break , Break  }, // T
    { Break , Break , Break , Break , Inside, Inside, Break , Break , Inside, Break , Inside, Inside, Break , Break , Break , Break , Break , Break  }, // LV
    { Break , Break , Break , Break , Inside, Inside, Break , Break , Inside, Break , Break , Inside, Break , Break , Break , Break , Break , Break  }, // LVT
    { Break , Break , Break , Break , GB10  , Inside, Break , Break , Inside, Break , Break , Break , Break , Break , Break , Inside, Break , Break  }, // E_B
    { Break , Break , Break , Break , Inside, Inside, Break , Break , Inside, Break , Break , Break , Break , Break , Break , Break , Break , Break  }, // E_M
    { Break , Break , Break , Break , Inside, Inside, Break , Break , Inside, Break , Break , Break , Break , Break , Break , Break , Break , Break  }, // GAZ
    { Break , Break , Break , Break , GB10  , Inside, Break , Break , Inside, Break , Break , Break , Break , Break , Break , Inside, Break , Break  }, // EBG
};

} // namespace GB


static void getGraphemeBreaks(const uint16_t *string, uint32_t len, LCharAttributes *attributes)
{
    GraphemeBreakClass lcls = GraphemeBreak_LF; // to meet GB1
    GB::State state = GB::Break; // only required to track some of the rules
    for (uint32_t i = 0; i != len; ++i) {
        uint32_t pos = i;
        uint32_t ucs4 = string[i];
        if (LStringUtil::isHighSurrogate(ucs4) && i + 1 != len) {
            uint16_t low = string[i + 1];
            if (LStringUtil::isLowSurrogate(low)) {
                ucs4 = LStringUtil::surrogateToUcs4(ucs4, low);
                ++i;
            }
        }

        const Properties *prop = Util::properties(ucs4);
        GraphemeBreakClass cls = (GraphemeBreakClass) prop->graphemeBreakClass;

        switch (GB::breakTable[lcls][cls]) {
        case GB::Break:{
            attributes[pos].graphemeBoundary = true;
            state = GB::Break;
            break;
        }
        case GB::Inside:{
            state = GB::Break;
            break;
        }
        case GB::GB10:{
            state = GB::GB10;
            break;
        }
        case GB::GB10_2:{
            if (state == GB::GB10 || state == GB::GB10_2){
                state = GB::GB10_2;
            }
            else{
                state = GB::Break;
            }
            break;
        }
        case GB::GB10_3:{
            if (state != GB::GB10 && state != GB::GB10_2){
                attributes[pos].graphemeBoundary = true;
            }
            state = GB::Break;
            break;
        }
        case GB::GB13:{
            if (state != GB::GB13) {
                state = GB::GB13;
            } else {
                attributes[pos].graphemeBoundary = true;
                state = GB::Break;
            }
            break;
        }
        }
        lcls = cls;
    }

    attributes[len].graphemeBoundary = true; // GB2
}

namespace WB {

enum Action {
    NoBreak,
    Break,
    Lookup,
    LookupW
};

static const uint8_t breakTable[NumWordBreakClasses][NumWordBreakClasses] = {
    //    Any      CR       LF       Newline  Extend   ZWJ      Format    RI       Katakana HLetter  ALetter  SQuote   DQuote  MidNumLet MidLetter MidNum  Numeric ExtNumLet E_Base   E_Mod    GAZ      EBG      WSeg
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // Any
    { Break  , Break  , NoBreak, Break  , Break  , Break  , Break  ,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // CR
    { Break  , Break  , Break  , Break  , Break  , Break  , Break  ,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // LF
    { Break  , Break  , Break  , Break  , Break  , Break  , Break  ,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // Newline
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // Extend
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , NoBreak, NoBreak, Break }, // ZWJ
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // Format
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  NoBreak, Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // RegionalIndicator
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , NoBreak, Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , NoBreak, Break  , Break  , Break  , Break  , Break }, // Katakana
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , NoBreak, NoBreak, LookupW, Lookup , LookupW, LookupW, Break  , NoBreak, NoBreak, Break  , Break  , Break  , Break  , Break }, // HebrewLetter
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , NoBreak, NoBreak, LookupW, Break  , LookupW, LookupW, Break  , NoBreak, NoBreak, Break  , Break  , Break  , Break  , Break }, // ALetter
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // SingleQuote
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // DoubleQuote
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // MidNumLet
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // MidLetter
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // MidNum
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , NoBreak, NoBreak, Lookup , Break  , Lookup , Break  , Lookup , NoBreak, NoBreak, Break  , Break  , Break  , Break  , Break }, // Numeric
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , NoBreak, NoBreak, NoBreak, Break  , Break  , Break  , Break  , Break  , NoBreak, NoBreak, Break  , Break  , Break  , Break  , Break }, // ExtendNumLet
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , NoBreak, Break  , Break  , Break }, // E_Base
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // E_Mod
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // GAZ
    { Break  , Break  , Break  , Break  , NoBreak, NoBreak, NoBreak,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , NoBreak, Break  , Break  , Break }, // EBG
    { Break  , Break  , Break  , Break  , Break  , Break  , Break  ,  Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break }, // WSeg
};

} // namespace WB

static void getWordBreaks(const uint16_t *string, uint32_t len, LCharAttributes *attributes)
{
    enum WordType {
        WordTypeNone, WordTypeAlphaNumeric, WordTypeHiraganaKatakana
    } currentWordType = WordTypeNone;

    WordBreakClass cls = WordBreak_LF; // to meet WB1
    for (uint32_t i = 0; i != len; ++i) {
        uint32_t pos = i;
        uint32_t ucs4 = string[i];
        if (LStringUtil::isHighSurrogate(ucs4) && i + 1 != len) {
            uint16_t low = string[i + 1];
            if (LStringUtil::isLowSurrogate(low)) {
                ucs4 = LStringUtil::surrogateToUcs4(ucs4, low);
                ++i;
            }
        }

        const Properties *prop = Util::properties(ucs4);
        WordBreakClass ncls = (WordBreakClass) prop->wordBreakClass;

        uint8_t action = WB::breakTable[cls][ncls];
        switch (action) {
        case WB::Break:{
            break;
        }
        case WB::NoBreak:{
            if (L_UNLIKELY(ncls == WordBreak_Extend || ncls == WordBreak_ZWJ || ncls == WordBreak_Format)) {
                // WB4: X(Extend|Format)* -> X
                if (cls != WordBreak_ZWJ){ // WB3c
                    continue;
                }
            }
            if (L_UNLIKELY(cls == WordBreak_RegionalIndicator)) {
                // WB15/WB16: break between pairs of Regional indicator
                ncls = WordBreak_Any;
            }
            break;
        }
        case WB::Lookup:
        case WB::LookupW:{
            for (uint32_t lookahead = i + 1; lookahead < len; ++lookahead) {
                ucs4 = string[lookahead];
                if (LStringUtil::isHighSurrogate(ucs4) && lookahead + 1 != len) {
                    uint16_t low = string[lookahead + 1];
                    if (LStringUtil::isLowSurrogate(low)) {
                        ucs4 = LStringUtil::surrogateToUcs4(ucs4, low);
                        ++lookahead;
                    }
                }

                prop = Util::properties(ucs4);
                WordBreakClass tcls = (WordBreakClass) prop->wordBreakClass;

                if (L_UNLIKELY(tcls == WordBreak_Extend || tcls == WordBreak_ZWJ || tcls == WordBreak_Format)) {
                    // WB4: X(Extend|Format)* -> X
                    continue;
                }

                if (L_LIKELY(tcls == cls || (action == WB::LookupW && (tcls == WordBreak_HebrewLetter
                                                                       || tcls == WordBreak_ALetter)))) {
                    i = lookahead;
                    ncls = tcls;
                    action = WB::NoBreak;
                }
                break;
            }
            if (action != WB::NoBreak) {
                action = WB::Break;
                if (L_UNLIKELY(ncls == WordBreak_SingleQuote && cls == WordBreak_HebrewLetter)){
                    action = WB::NoBreak; // WB7a
                }
            }
            break;
        }
        }

        cls = ncls;
        if (action == WB::Break) {
            attributes[pos].wordBreak = true;
            if (currentWordType != WordTypeNone){
                attributes[pos].wordEnd = true;
            }
            switch (cls) {
            case WordBreak_Katakana:{
                currentWordType = WordTypeHiraganaKatakana;
                attributes[pos].wordStart = true;
                break;
            }
            case WordBreak_HebrewLetter:
            case WordBreak_ALetter:
            case WordBreak_Numeric:{
                currentWordType = WordTypeAlphaNumeric;
                attributes[pos].wordStart = true;
                break;
            }
            default:{
                currentWordType = WordTypeNone;
                break;
            }
            }
        }
    }

    if (currentWordType != WordTypeNone){
        attributes[len].wordEnd = true;
    }
    attributes[len].wordBreak = true; // WB2
}


namespace SB {

enum State {
    Initial,
    Lower,
    Upper,
    LUATerm,
    ATerm,
    ATermC,
    ACS,
    STerm,
    STermC,
    SCS,
    BAfterC,
    BAfter,
    Break,
    Lookup
};

static const uint8_t breakTable[BAfter + 1][NumSentenceBreakClasses] = {
    //    Any      CR       LF       Sep      Extend   Sp       Lower    Upper    OLetter  Numeric  ATerm   SContinue STerm    Close
    { Initial, BAfterC, BAfter , BAfter , Initial, Initial, Lower  , Upper  , Initial, Initial, ATerm  , Initial, STerm  , Initial }, // Initial
    { Initial, BAfterC, BAfter , BAfter , Lower  , Initial, Initial, Initial, Initial, Initial, LUATerm, Initial, STerm  , Initial }, // Lower
    { Initial, BAfterC, BAfter , BAfter , Upper  , Initial, Initial, Upper  , Initial, Initial, LUATerm, Initial, STerm  , Initial }, // Upper

    { Lookup , BAfterC, BAfter , BAfter , LUATerm, ACS    , Initial, Upper  , Break  , Initial, ATerm  , STerm  , STerm  , ATermC  }, // LUATerm
    { Lookup , BAfterC, BAfter , BAfter , ATerm  , ACS    , Initial, Break  , Break  , Initial, ATerm  , STerm  , STerm  , ATermC  }, // ATerm
    { Lookup , BAfterC, BAfter , BAfter , ATermC , ACS    , Initial, Break  , Break  , Lookup , ATerm  , STerm  , STerm  , ATermC  }, // ATermC
    { Lookup , BAfterC, BAfter , BAfter , ACS    , ACS    , Initial, Break  , Break  , Lookup , ATerm  , STerm  , STerm  , Lookup  }, // ACS

    { Break  , BAfterC, BAfter , BAfter , STerm  , SCS    , Break  , Break  , Break  , Break  , ATerm  , STerm  , STerm  , STermC  }, // STerm,
    { Break  , BAfterC, BAfter , BAfter , STermC , SCS    , Break  , Break  , Break  , Break  , ATerm  , STerm  , STerm  , STermC  }, // STermC
    { Break  , BAfterC, BAfter , BAfter , SCS    , SCS    , Break  , Break  , Break  , Break  , ATerm  , STerm  , STerm  , Break   }, // SCS
    { Break  , Break  , BAfter , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break   }, // BAfterC
    { Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break  , Break   }, // BAfter
};

} // namespace SB

static void getSentenceBreaks(const uint16_t *string, uint32_t len, LCharAttributes *attributes)
{
    uint8_t state = SB::BAfter; // to meet SB1
    for (uint32_t i = 0; i != len; ++i) {
        uint32_t pos = i;
        uint32_t ucs4 = string[i];
        if (LStringUtil::isHighSurrogate(ucs4) && (i + 1) != len) {
            uint16_t low = string[i + 1];
            if (LStringUtil::isLowSurrogate(low)) {
                ucs4 = LStringUtil::surrogateToUcs4(ucs4, low);
                ++i;
            }
        }

        const Properties *prop = Util::properties(ucs4);
        SentenceBreakClass ncls = (SentenceBreakClass) prop->sentenceBreakClass;

        state = SB::breakTable[state][ncls];
        if (L_UNLIKELY(state == SB::Lookup)) { // SB8
            state = SB::Break;
            for (uint32_t lookahead = i + 1; lookahead < len; ++lookahead) {
                ucs4 = string[lookahead];
                if (LStringUtil::isHighSurrogate(ucs4) && lookahead + 1 != len) {
                    uint16_t low = string[lookahead + 1];
                    if (LStringUtil::isLowSurrogate(low)) {
                        ucs4 = LStringUtil::surrogateToUcs4(ucs4, low);
                        ++lookahead;
                    }
                }

                prop = Util::properties(ucs4);
                SentenceBreakClass tcls = (SentenceBreakClass) prop->sentenceBreakClass;
                switch (tcls) {
                case SentenceBreak_Any:
                case SentenceBreak_Extend:
                case SentenceBreak_Sp:
                case SentenceBreak_Numeric:
                case SentenceBreak_SContinue:
                case SentenceBreak_Close:{
                    continue;
                }
                case SentenceBreak_Lower:{
                    i = lookahead;
                    state = SB::Initial;
                    break;
                }
                default:{
                    break;
                }
                }
                break;
            }
        }
        if (L_UNLIKELY(state == SB::Break)) {
            attributes[pos].sentenceBoundary = true;
            state = SB::breakTable[SB::Initial][ncls];
        }
    }

    attributes[len].sentenceBoundary = true; // SB2
}

// -----------------------------------------------------------------------------------------------------
//
// The line breaking algorithm.
// See http://www.unicode.org/reports/tr14/tr14-39.html
//
// -----------------------------------------------------------------------------------------------------

namespace LB {

namespace NS { // Number Sequence

// LB25 recommends to not break lines inside numbers of the form
// described by the following regular expression:
//  (PR|PO)?(OP|HY)?NU(NU|SY|IS)*(CL|CP)?(PR|PO)?

enum Action {
    None,
    Start,
    Continue,
    Break
};

enum Class {
    XX,
    PRPO,
    OPHY,
    NU,
    SYIS,
    CLCP
};

static const uint8_t actionTable[CLCP + 1][CLCP + 1] = {
    //     XX       PRPO      OPHY       NU       SYIS      CLCP
    { None    , Start   , Start   , Start   , None    , None     }, // XX
    { None    , Start   , Continue, Continue, None    , None     }, // PRPO
    { None    , Start   , Start   , Continue, None    , None     }, // OPHY
    { Break   , Break   , Break   , Continue, Continue, Continue }, // NU
    { Break   , Break   , Break   , Continue, Continue, Continue }, // SYIS
    { Break   , Continue, Break   , Break   , Break   , Break    }, // CLCP
};

inline Class toClass(LineBreakClass lbc, Category category)
{
    switch (lbc) {
    case LineBreak_AL:{// case LineBreak_AI:
        // resolve AI math symbols in numerical context to IS
        if (category == Symbol_Math){
            return SYIS;
        }
        break;
    }
    case LineBreak_PR:
    case LineBreak_PO:{
        return PRPO;
    }
    case LineBreak_OP:
    case LineBreak_HY:{
        return OPHY;
    }
    case LineBreak_NU:{
        return NU;
    }
    case LineBreak_SY:
    case LineBreak_IS:{
        return SYIS;
    }
    case LineBreak_CL:
    case LineBreak_CP:{
        return CLCP;
    }
    default:{
        break;
    }
    }
    return XX;
}

} // namespace NS

/* In order to support the tailored implementation of LB25 properly
   the following changes were made in the pair table to allow breaks
   where the numeric expression doesn't match the template (i.e. [^NU](IS|SY)NU):
   (CL)(PO) from IB to DB
   (CP)(PO) from IB to DB
   (CL)(PR) from IB to DB
   (CP)(PR) from IB to DB
   (PO)(OP) from IB to DB
   (PR)(OP) from IB to DB
   (IS)(NU) from IB to DB
   (SY)(NU) from IB to DB
*/

/* In order to implementat LB21a properly a special rule HH has been introduced and
   the following changes were made in the pair table to disallow breaks after Hebrew + Hyphen:
   (HL)(HY|BA) from IB to CI
   (HY|BA)(!CB) from DB to HH
*/

enum Action {
    ProhibitedBreak, PB = ProhibitedBreak,
    DirectBreak, DB = DirectBreak,
    IndirectBreak, IB = IndirectBreak,
    CombiningIndirectBreak, CI = CombiningIndirectBreak,
    CombiningProhibitedBreak, CP = CombiningProhibitedBreak,
    ProhibitedBreakAfterHebrewPlusHyphen, HH = ProhibitedBreakAfterHebrewPlusHyphen
};

static const uint8_t breakTable[LineBreak_SA][LineBreak_SA] = {
    /*         OP  CL  CP  QU  GL  NS  EX  SY  IS  PR  PO  NU  AL  HL  ID  IN  HY  BA  BB  B2  ZW  CM  WJ  H2  H3  JL  JV  JT  RI  CB  EB  EM  ZWJ*/
    /* OP */ { PB, PB, PB, PB, PB, PB, PB, PB, PB, PB, PB, PB, PB, PB, PB, PB, PB, PB, PB, PB, PB, CP, PB, PB, PB, PB, PB, PB, PB, PB, PB, PB, PB },
    /* CL */ { DB, PB, PB, IB, IB, PB, PB, PB, PB, DB, DB, DB, DB, DB, DB, DB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* CP */ { DB, PB, PB, IB, IB, PB, PB, PB, PB, DB, DB, IB, IB, IB, DB, DB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* QU */ { PB, PB, PB, IB, IB, IB, PB, PB, PB, IB, IB, IB, IB, IB, IB, IB, IB, IB, IB, IB, PB, CI, PB, IB, IB, IB, IB, IB, IB, IB, IB, IB, IB },
    /* GL */ { IB, PB, PB, IB, IB, IB, PB, PB, PB, IB, IB, IB, IB, IB, IB, IB, IB, IB, IB, IB, PB, CI, PB, IB, IB, IB, IB, IB, IB, IB, IB, IB, IB },
    /* NS */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, DB, DB, DB, DB, DB, DB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* EX */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, DB, DB, DB, DB, DB, IB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* SY */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, DB, DB, DB, IB, DB, DB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* IS */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, DB, DB, IB, IB, DB, DB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* PR */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, DB, IB, IB, IB, IB, DB, IB, IB, DB, DB, PB, CI, PB, IB, IB, IB, IB, IB, DB, DB, IB, IB, IB },
    /* PO */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, DB, IB, IB, IB, DB, DB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* NU */ { IB, PB, PB, IB, IB, IB, PB, PB, PB, IB, IB, IB, IB, IB, DB, IB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* AL */ { IB, PB, PB, IB, IB, IB, PB, PB, PB, IB, IB, IB, IB, IB, DB, IB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* HL */ { IB, PB, PB, IB, IB, IB, PB, PB, PB, IB, IB, IB, IB, IB, DB, IB, CI, CI, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* ID */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, IB, DB, DB, DB, DB, IB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* IN */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, DB, DB, DB, DB, DB, IB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* HY */ { HH, PB, PB, IB, HH, IB, PB, PB, PB, HH, HH, IB, HH, HH, HH, HH, IB, IB, HH, HH, PB, CI, PB, HH, HH, HH, HH, HH, HH, DB, DB, DB, IB },
    /* BA */ { HH, PB, PB, IB, HH, IB, PB, PB, PB, HH, HH, HH, HH, HH, HH, HH, IB, IB, HH, HH, PB, CI, PB, HH, HH, HH, HH, HH, HH, DB, DB, DB, IB },
    /* BB */ { IB, PB, PB, IB, IB, IB, PB, PB, PB, IB, IB, IB, IB, IB, IB, IB, IB, IB, IB, IB, PB, CI, PB, IB, IB, IB, IB, IB, IB, DB, IB, IB, IB },
    /* B2 */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, DB, DB, DB, DB, DB, DB, IB, IB, DB, PB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* ZW */ { DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB },
    /* CM */ { IB, PB, PB, IB, IB, IB, PB, PB, PB, DB, DB, IB, IB, IB, DB, IB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* WJ */ { IB, PB, PB, IB, IB, IB, PB, PB, PB, IB, IB, IB, IB, IB, IB, IB, IB, IB, IB, IB, PB, CI, PB, IB, IB, IB, IB, IB, IB, IB, IB, IB, IB },
    /* H2 */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, IB, DB, DB, DB, DB, IB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, IB, IB, DB, DB, DB, DB, IB },
    /* H3 */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, IB, DB, DB, DB, DB, IB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, IB, DB, DB, DB, DB, IB },
    /* JL */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, IB, DB, DB, DB, DB, IB, IB, IB, DB, DB, PB, CI, PB, IB, IB, IB, IB, DB, DB, DB, DB, DB, IB },
    /* JV */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, IB, DB, DB, DB, DB, IB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, IB, IB, DB, DB, DB, DB, IB },
    /* JT */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, IB, DB, DB, DB, DB, IB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, IB, DB, DB, DB, DB, IB },
    /* RI */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, DB, DB, DB, DB, DB, DB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, IB, DB, DB, DB, IB },
    /* CB */ { DB, PB, PB, IB, IB, DB, PB, PB, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* EB */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, IB, DB, DB, DB, DB, IB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, IB, IB },
    /* EM */ { DB, PB, PB, IB, IB, IB, PB, PB, PB, DB, IB, DB, DB, DB, DB, IB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, DB, DB, IB },
    /* ZWJ*/ { IB, PB, PB, IB, IB, IB, PB, PB, PB, IB, IB, IB, IB, IB, IB, IB, IB, IB, DB, DB, PB, CI, PB, DB, DB, DB, DB, DB, DB, DB, IB, IB, IB }
};

// The following line break classes are not treated by the pair table
// and must be resolved outside:
//  AI, BK, CB, CJ, CR, LF, NL, SA, SG, SP, XX

} // namespace LB

static void getLineBreaks(const uint16_t *string, uint32_t len, LCharAttributes *attributes, LFlags<LCharAttributeOption> options)
{
    uint32_t nestart = 0;
    LB::NS::Class nelast = LB::NS::XX;

    LineBreakClass lcls = LineBreak_LF; // to meet LB10
    LineBreakClass cls = lcls;
    for (uint32_t i = 0; i < len; ++i) {
        uint32_t pos = i;
        uint32_t ucs4 = string[i];
        if (LStringUtil::isHighSurrogate(ucs4) && (i + 1) < len) {
            uint16_t low = string[i + 1];
            if (LStringUtil::isLowSurrogate(low)) {
                ucs4 = LStringUtil::surrogateToUcs4(ucs4, low);
                ++i;
            }
        }

        const Properties *prop = Util::properties(ucs4);
        LineBreakClass ncls = (LineBreakClass) prop->lineBreakClass;
        LineBreakClass tcls;

        if (options & LCharAttributeOption::HangulLineBreakTailoring) {
            if (L_UNLIKELY((ncls >= LineBreak_H2
                            &&  ncls <= LineBreak_JT)
                           || (ucs4 >= 0x3130 && ucs4 <= 0x318F && ncls == LineBreak_ID))
                ) {
                // LB27: use SPACE for line breaking
                // "When Korean uses SPACE for line breaking, the classes in rule LB26,
                // as well as characters of class ID, are often tailored to AL; see Section 8, Customization."
                // In case of Korean syllables: "3130..318F  HANGUL COMPATIBILITY JAMO"
                ncls = LineBreak_AL;
            } else {
                if (L_UNLIKELY(ncls == LineBreak_SA)) {
                    // LB1: resolve SA to AL, except of those that have Category Mn or Mc be resolved to CM
                    static const int test = FLAG(Mark_NonSpacing) | FLAG(Mark_SpacingCombining);
                    if (FLAG(prop->category) & test){
                        ncls = LineBreak_CM;
                    }
                }
                if (L_UNLIKELY(ncls == LineBreak_CM)) {
                    // LB10: treat CM that follows SP, BK, CR, LF, NL, or ZW as AL
                    if (lcls == LineBreak_ZW || lcls >= LineBreak_SP){
                        ncls = LineBreak_AL;
                    }
                }
            }
        }

        if (L_UNLIKELY(ncls == LineBreak_SA)) {
            // LB1: resolve SA to AL, except of those that have Category Mn or Mc be resolved to CM
            static const int test = FLAG(Mark_NonSpacing) | FLAG(Mark_SpacingCombining);
            if (FLAG(prop->category) & test){
                ncls = LineBreak_CM;
            }
        }

        if (L_UNLIKELY(lcls >= LineBreak_CR)) {
            // LB4: BK!, LB5: (CRxLF|CR|LF|NL)!
            if (lcls > LineBreak_CR || ncls != LineBreak_LF){
                attributes[pos].lineBreak = attributes[pos].mandatoryBreak = true;
            }
            if (L_UNLIKELY(ncls == LineBreak_CM || ncls == LineBreak_ZWJ)) {
                cls = LineBreak_AL;
                goto next_no_cls_update;
            }
            goto next;
        }

        if (L_UNLIKELY(ncls >= LineBreak_SP)) {
            if (ncls > LineBreak_SP){
                goto next; // LB6: x(BK|CR|LF|NL)
            }
            goto next_no_cls_update; // LB7: xSP
        }

        if (L_UNLIKELY(ncls == LineBreak_CM || ncls == LineBreak_ZWJ)) {
            // LB9: treat CM that don't follows SP, BK, CR, LF, NL, or ZW as X
            if (lcls != LineBreak_ZW && lcls < LineBreak_SP){
                // don't update anything
                goto next_no_cls_update;
            }
        }

        if (L_UNLIKELY(lcls == LineBreak_ZWJ)) {
            // LB8a: ZWJ x (ID | EB | EM)
            if (ncls == LineBreak_ID || ncls == LineBreak_EB || ncls == LineBreak_EM){
                goto next;
            }
        }

        // LB25: do not break lines inside numbers
        {
            LB::NS::Class necur = LB::NS::toClass(ncls, (Category)prop->category);
            switch (LB::NS::actionTable[nelast][necur]) {
            case LB::NS::Break:{
                // do not change breaks before and after the expression
                for (uint32_t j = nestart + 1; j < pos; ++j){
                    attributes[j].lineBreak = false;
                }
                //No break;
            }
            case LB::NS::None:{
                nelast = LB::NS::XX; // reset state
                break;
            }
            case LB::NS::Start:{
                nestart = i;
                //No break;
            }
            default:{
                nelast = necur;
                break;
            }
            }
        }

        if (L_UNLIKELY(ncls == LineBreak_RI && lcls == LineBreak_RI)) {
            // LB30a
            ncls = LineBreak_SP;
            goto next;
        }

        // for South East Asian chars that require a complex analysis, the Unicode
        // standard recommends to treat them as AL. tailoring that do dictionary analysis can override
        if (L_UNLIKELY(cls >= LineBreak_SA)){
            cls = LineBreak_AL;
        }

        tcls = cls;
        if (tcls == LineBreak_CM){
            // LB10
            tcls = LineBreak_AL;
        }
        switch (LB::breakTable[tcls][ncls < LineBreak_SA ? ncls : LineBreak_AL]) {
        case LB::DirectBreak:{
            //Todo 中文会跑这边，导致错误换行
            attributes[pos].lineBreak = true;
            break;
        }
        case LB::IndirectBreak:{
            if (lcls == LineBreak_SP){
                attributes[pos].lineBreak = true;
            }
            break;
        }
        case LB::CombiningIndirectBreak:{
            if (lcls != LineBreak_SP){
                goto next_no_cls_update;
            }
            attributes[pos].lineBreak = true;
            break;
        }
        case LB::CombiningProhibitedBreak:{
            if (lcls != LineBreak_SP){
                goto next_no_cls_update;
            }
            break;
        }
        case LB::ProhibitedBreakAfterHebrewPlusHyphen:{
            if (lcls != LineBreak_HL){
                attributes[pos].lineBreak = true;
            }
            break;
        }
        case LB::ProhibitedBreak:{
            // nothing to do
            break;
        }
        default:{
            break;
        }
        }

    next:
        cls = ncls;
    next_no_cls_update:
        lcls = ncls;
    }

    if (L_UNLIKELY(LB::NS::actionTable[nelast][LB::NS::XX] == LB::NS::Break)) {
        // LB25: do not break lines inside numbers
        for (uint32_t j = nestart + 1; j < len; ++j){
            attributes[j].lineBreak = false;
        }
    }

    attributes[0].lineBreak = attributes[0].mandatoryBreak = false; // LB2
    attributes[len].lineBreak = attributes[len].mandatoryBreak = true; // LB3
}

static void getWhiteSpaces(const uint16_t *string, uint32_t len, LCharAttributes *attributes)
{
    for (uint32_t i = 0; i != len; ++i) {
        uint32_t ucs4 = string[i];
        if (LStringUtil::isHighSurrogate(ucs4) && i + 1 != len) {
            uint16_t low = string[i + 1];
            if (LStringUtil::isLowSurrogate(low)) {
                ucs4 = LStringUtil::surrogateToUcs4(ucs4, low);
                ++i;
            }
        }

        if (L_UNLIKELY(LStringUtil::isSpace(ucs4))){
            attributes[i].whiteSpace = true;
        }
    }
}


void Util::initCharAttributes(const uint16_t *string, int length,
                        LCharAttributes *attributes,
                        LFlags<LCharAttributeOption> options){
    if (length <= 0){
        return;
    }

    if (!(options & LCharAttributeOption::DontClearAttributes)){
        ::memset(attributes, 0, (length + 1) * sizeof(LCharAttributes));
    }

    if (options & LCharAttributeOption::GraphemeBreaks){
        getGraphemeBreaks(string, length, attributes);
    }
    if (options & LCharAttributeOption::WordBreaks){
        getWordBreaks(string, length, attributes);
    }
    if (options & LCharAttributeOption::SentenceBreaks){
        getSentenceBreaks(string, length, attributes);
    }
    if (options & LCharAttributeOption::LineBreaks){
        getLineBreaks(string, length, attributes, options);
    }
    if (options & LCharAttributeOption::WhiteSpaces){
        getWhiteSpaces(string, length, attributes);
    }
}


void Util::initScripts(const uint16_t *string, int length, uint8_t *scripts){
    int sor = 0;
    int eor = 0;
    uint8_t script = Script_Common;

    for (int i = 0; i < length; ++i, eor = i) {
        uint32_t ucs4 = string[i];
        if (LStringUtil::isHighSurrogate(ucs4) && (i + 1) < length) {
            uint16_t low = string[i + 1];
            if (LStringUtil::isLowSurrogate(low)) {
                ucs4 = LStringUtil::surrogateToUcs4(ucs4, low);
                ++i;
            }
        }

        const Properties *prop = properties(ucs4);
        uint8_t nscript = prop->script;

        if (L_LIKELY(nscript == script || nscript <= Script_Common)){
            continue;
        }

        // inherit preceding Common-s
        if (L_UNLIKELY(script <= Script_Common)) {
            // also covers a case where the base character of Common script followed
            // by one or more combining marks of non-Inherited, non-Common script
            script = nscript;
            continue;
        }

        // Never break between a combining mark (gc= Mc, Mn or Me) and its base character.
        // Thus, a combining mark - whatever its script property value is - should inherit
        // the script property value of its base character.
        static const int test = (FLAG(Mark_NonSpacing) | FLAG(Mark_SpacingCombining) | FLAG(Mark_Enclosing));
        if (L_UNLIKELY(FLAG(prop->category) & test)){
            continue;
        }

        if(eor >sor){
            ::memset(scripts + sor, script, (eor - sor));
        }
        sor = eor;
        script = nscript;
    }
    ::memset(scripts + sor, script, (eor - sor));

}

bool Util::isScriptLeftToRight(uint8_t script)
{
    switch(script){
    case Script_Adlam:
    case Script_Arabic:
    case Script_Avestan:
    case Script_Cypriot:
    case Script_Hatran:
    case Script_Hebrew:
    case Script_ImperialAramaic:
    case Script_InscriptionalPahlavi:
    case Script_InscriptionalParthian:
    case Script_Kharoshthi:
    case Script_Lydian:
    case Script_Mandaean:
    case Script_Manichaean:
    case Script_MendeKikakui:
    case Script_MeroiticCursive:
    case Script_MeroiticHieroglyphs:
    case Script_Nabataean:
    case Script_Nko:
    case Script_OldHungarian:
    case Script_OldNorthArabian:
    case Script_OldSouthArabian:
    case Script_OldTurkic:
    case Script_Palmyrene:
    case Script_Phoenician:
    case Script_PsalterPahlavi:
    case Script_Samaritan:
    case Script_Syriac:
    case Script_Thaana:{
        return false;
    }
    }
    return true;
}

const static std::string lkScriptShorts[ScriptCount-Script_Common]={
    "Zzzz", // Any
    "Latn", // Latin
    "Grek", // Greek
    "Cyrl", // Cyrillic
    "Armn", // Armenian
    "Hebr", // Hebrew
    "Arab", // Arabic
    "Syrc", // Syriac
    "Thaa", // Thaana
    "Deva", // Devanagari
    "Beng", // Bengali
    "Guru", // Gurmukhi
    "Gujr", // Gujarati
    "Orya", // Oriya
    "Taml", // Tamil
    "Telu", // Telugu
    "Knda", // Kannada
    "Mlym", // Malayalam
    "Sinh", // Sinhala
    "Thai", // Thai
    "Laoo", // Lao
    "Tibt", // Tibetan
    "Mymr", // Myanmar
    "Geor", // Georgian
    "Hang", // Hangul
    "Ethi", // Ethiopic
    "Cher", // Cherokee
    "Cans", // Canadian Aboriginal
    "Ogam", // Ogham
    "Runr", // Runic
    "Khmr", // Khmer
    "Mong", // Mongolian
    "Hira", // Hiragana
    "Kana", // Katakana
    "Bopo", // Bopomofo
    "Hang", // Hangul
    "Yiii", // Yi
    "Ital", // Old Italic
    "Goth", // Gothic
    "Dsrt", // Deseret
    "Tglg", // Tagalog
    "Hano", // Hanunoo
    "Buhd", // Buhid
    "Tagb", // Tagbanwa
    "Copt", // Coptic
    "Limb", // Limbu
    "Tale", // TaiLe
    "Linb", // Linear B
    "Ugar", // Ugaritic
    "Shaw", // Shavian
    "Osma", // Osmanya
    "Cprt", // Cypriot
    "Brai", // Braille
    "Bugi", // Buginese
    "Talu", // New Tai Lue
    "Glag", // Glagolitic
    "Tfng", // Tifinagh
    "Sylo", // Syloti Nagri
    "Xpeo", // Old Persian
    "Khar", // Kharoshthi
    "Bali", // Balinese
    "Xsux", // Cuneiform
    "Phnx", // Phoenician
    "Phag", // Phagspa
    "Nkoo", // Nko
    "Sund", // Sundanese
    "Lepc", // Lepcha
    "Olck", // Ol Chiki
    "Vaii", // Vai
    "Saur", // Saurashtra
    "Kali", // Kayah Li
    "Rjng", // Rejang
    "Lyci", // Lycian
    "Cari", // Carian
    "Lydi", // Lydian
    "Cham", // Cham
    "Tale", // Tai Tham
    "Tavt", // Tai Viet
    "Avst", // Avestan
    "Egyp", // Egyptian Hieroglyphs
    "Samr", // Samaritan
    "Lisu", // Fraser
    "Bamu", // Bamum
    "Java", // Javanese
    "Mtei", // Meitei Mayek
    "Armi", // Imperial Aramaic
    "Sarb", // Old South Arabian
    "Phli", // Inscriptional Parthian
    "Prti", // Inscriptional Pahlavi
    "Orkh", // Old Turkic
    "Kthi", // Kaithi
    "Batk", // Batak
    "Brah", // Brahmi
    "Mand", // Mandaean
    "Cakm", // Chakma
    "Merc", // Meroitic Cursive
    "Mero", // Meroitic Hieroglyphs
    "Plrd", // Pollard Miao
    "Shrd", // Sharada
    "Sora", // Sora Sompeng
    "Takr", // Takri
    "Aghb", // Caucasian Albanian
    "Bass", // Bassa Vah
    "Dupl", // Duployan
    "Elba", // Elbasan
    "Gran", // Grantha
    "Hmng", // Pahawh Hmong
    "Khoj", // Khojki
    "Lina", // Linear A
    "Mahj", // Mahajani
    "Mani", // Manichaean
    "Mend", // Mende Kikakui
    "Modi", // Modi
    "Mroo", // Mro
    "Narb", // Old North Arabian
    "Nbat", // Nabataean
    "Palm", // Palmyrene
    "Pauc", // Pau Cin Hau
    "Perm", // Old Permic
    "Phlp", // Psalter Pahlavi
    "Sidd", // Siddham
    "Sind", // Khudawadi
    "Tirh", // Tirhuta
    "Wara", // Warang Citi/Varang Kshiti
    "Ahom", // Ahom
    "Hluw", // Anatolian Hieroglyphs
    "Hatr", // Hatran
    "Mult", // Multani
    "Hung", // Old Hungarian
    "Sgnw", // SignWriting
    "Adlm", // Adlam
    "Bhks", // Bhaiksuki
    "Marc", // Marchen
    "Newa", // Newa
    "Osge", // Osage
    "Tang", // Tangut
    "Gonm", // Masaram Gondi
    "Nshu", // Nushu
    "Soyo", // Soyombo
    "Zanb", // Zanabazar Square
    "Dogr", // Dogra
    "Gong", // Gunjala Gondi
    "Rohg", // Hanifi Rohingya
    "Maka", // Makasar
    "Medf", // Medefaidrin
    "Sogo", // Old Sogdian
    "Sogd", // Sogdian
    "Elym", // Elymaic
    "Nand", // Nandinagari
    "Hmnp", // Nyiakeng PuachueHmong
    "Wcho", // Wancho
    "Chrs", // Chorasmian
    "Diak", // Dives Akuru
    "Kitl", // Khitan Small Script
    "Yezi", // Yezidi
};

const std::string& Util::scriptShortName(uint8_t script)
{
    if(script >= Script_Common && script < ScriptCount){
        return lkScriptShorts[script-Script_Common];
    }
    return LStringUtil::empty;
}

uint8_t Util::shortNameToScript(std::string name)
{
    if(name.length() != 4){
        return Script_Unknown;
    }

    name[0] = toupper(name[0]);
    for(uint8_t i=1; i<4; ++i){
        name[i] = tolower(name[i]);
    }

    for(uint8_t i =0; i<ScriptCount; ++i){
        if(lkScriptShorts[i-Script_Common] == name){
            return i+Script_Common;
        }
    }
    return Script_Unknown;
}


}
LK_NAMESPACE_END
