#ifndef LCHAR_H
#define LCHAR_H
#include "lkcore/lglobal.h"
#include "lkcore/lflags.h"

LK_NAMESPACE_BEGIN

union LCharAttributes{
uint8_t value = 0;
struct{
    uint8_t graphemeBoundary : 1;
    uint8_t wordBreak        : 1;
    uint8_t sentenceBoundary : 1;
    uint8_t lineBreak        : 1;
    uint8_t whiteSpace       : 1;
    uint8_t wordStart        : 1;
    uint8_t wordEnd          : 1;
    uint8_t mandatoryBreak   : 1;
};
};

enum class LCharAttributeOption {
    GraphemeBreaks = 0x01,
    WordBreaks = 0x02,
    SentenceBreaks = 0x04,
    LineBreaks = 0x08,
    WhiteSpaces = 0x10,
    HangulLineBreakTailoring = 0x20,
    DefaultOptionsCompat = GraphemeBreaks | LineBreaks | WhiteSpaces, // ### remove

    DontClearAttributes = 0x1000
};

L_EN_FLAGS(LCharAttributeOption)


namespace lchar {
struct LScriptItem
{
int position;
int script;
};


enum Case {
    LowerCase,
    UpperCase,
    TitleCase,
    CaseFold,
    NumCases
};

struct Properties {
    uint16_t category            : 8; /* 5 used */
    uint16_t direction           : 8; /* 5 used */
    uint16_t combiningClass      : 8;
    uint16_t joining             : 3;
    short digitValue    : 5;
    short mirrorDiff    : 16;
    uint16_t unicodeVersion      : 8; /* 5 used */
    uint16_t nfQuickCheck        : 8;

    struct CaseItem{
        uint16_t special    : 1;
        short diff : 15;
    } cases[NumCases];

    uint16_t graphemeBreakClass  : 5; /* 5 used */
    uint16_t wordBreakClass      : 5; /* 5 used */
    uint16_t lineBreakClass      : 6; /* 6 used */
    uint16_t sentenceBreakClass  : 8; /* 4 used */
    uint16_t script              : 8;
};

enum GraphemeBreakClass {
    GraphemeBreak_Any,
    GraphemeBreak_CR,
    GraphemeBreak_LF,
    GraphemeBreak_Control,
    GraphemeBreak_Extend,
    GraphemeBreak_ZWJ,
    GraphemeBreak_RegionalIndicator,
    GraphemeBreak_Prepend,
    GraphemeBreak_SpacingMark,
    GraphemeBreak_L,
    GraphemeBreak_V,
    GraphemeBreak_T,
    GraphemeBreak_LV,
    GraphemeBreak_LVT,
    Graphemebreak_E_Base,
    Graphemebreak_E_Modifier,
    Graphemebreak_Glue_After_Zwj,
    Graphemebreak_E_Base_GAZ,
    NumGraphemeBreakClasses
};

enum WordBreakClass {
    WordBreak_Any,
    WordBreak_CR,
    WordBreak_LF,
    WordBreak_Newline,
    WordBreak_Extend,
    WordBreak_ZWJ,
    WordBreak_Format,
    WordBreak_RegionalIndicator,
    WordBreak_Katakana,
    WordBreak_HebrewLetter,
    WordBreak_ALetter,
    WordBreak_SingleQuote,
    WordBreak_DoubleQuote,
    WordBreak_MidNumLet,
    WordBreak_MidLetter,
    WordBreak_MidNum,
    WordBreak_Numeric,
    WordBreak_ExtendNumLet,
    WordBreak_E_Base,
    WordBreak_E_Modifier,
    WordBreak_Glue_After_Zwj,
    WordBreak_E_Base_GAZ,
    WordBreak_WSegSpace,
    NumWordBreakClasses
};

enum SentenceBreakClass {
    SentenceBreak_Any,
    SentenceBreak_CR,
    SentenceBreak_LF,
    SentenceBreak_Sep,
    SentenceBreak_Extend,
    SentenceBreak_Sp,
    SentenceBreak_Lower,
    SentenceBreak_Upper,
    SentenceBreak_OLetter,
    SentenceBreak_Numeric,
    SentenceBreak_ATerm,
    SentenceBreak_SContinue,
    SentenceBreak_STerm,
    SentenceBreak_Close,
    NumSentenceBreakClasses
};

// see http://www.unicode.org/reports/tr14/tr14-30.html
// we don't use the XX and AI classes and map them to AL instead.
enum LineBreakClass {
    LineBreak_OP, LineBreak_CL, LineBreak_CP, LineBreak_QU, LineBreak_GL,
    LineBreak_NS, LineBreak_EX, LineBreak_SY, LineBreak_IS, LineBreak_PR,
    LineBreak_PO, LineBreak_NU, LineBreak_AL, LineBreak_HL, LineBreak_ID,
    LineBreak_IN, LineBreak_HY, LineBreak_BA, LineBreak_BB, LineBreak_B2,
    LineBreak_ZW, LineBreak_CM, LineBreak_WJ, LineBreak_H2, LineBreak_H3,
    LineBreak_JL, LineBreak_JV, LineBreak_JT, LineBreak_RI, LineBreak_CB,
    LineBreak_EB, LineBreak_EM, LineBreak_ZWJ,
    LineBreak_SA, LineBreak_SG, LineBreak_SP,
    LineBreak_CR, LineBreak_LF, LineBreak_BK,
    NumLineBreakClasses
};


enum Category
{
    Mark_NonSpacing,          //   Mn
    Mark_SpacingCombining,    //   Mc
    Mark_Enclosing,           //   Me

    Number_DecimalDigit,      //   Nd
    Number_Letter,            //   Nl
    Number_Other,             //   No

    Separator_Space,          //   Zs
    Separator_Line,           //   Zl
    Separator_Paragraph,      //   Zp

    Other_Control,            //   Cc
    Other_Format,             //   Cf
    Other_Surrogate,          //   Cs
    Other_PrivateUse,         //   Co
    Other_NotAssigned,        //   Cn

    Letter_Uppercase,         //   Lu
    Letter_Lowercase,         //   Ll
    Letter_Titlecase,         //   Lt
    Letter_Modifier,          //   Lm
    Letter_Other,             //   Lo

    Punctuation_Connector,    //   Pc
    Punctuation_Dash,         //   Pd
    Punctuation_Open,         //   Ps
    Punctuation_Close,        //   Pe
    Punctuation_InitialQuote, //   Pi
    Punctuation_FinalQuote,   //   Pf
    Punctuation_Other,        //   Po

    Symbol_Math,              //   Sm
    Symbol_Currency,          //   Sc
    Symbol_Modifier,          //   Sk
    Symbol_Other              //   So
};

enum Script
{
    Script_Unknown,
    Script_Inherited,
    Script_Common,

    Script_Latin,
    Script_Greek,
    Script_Cyrillic,
    Script_Armenian,
    Script_Hebrew,
    Script_Arabic,
    Script_Syriac,
    Script_Thaana,
    Script_Devanagari,
    Script_Bengali,
    Script_Gurmukhi,
    Script_Gujarati,
    Script_Oriya,
    Script_Tamil,
    Script_Telugu,
    Script_Kannada,
    Script_Malayalam,
    Script_Sinhala,
    Script_Thai,
    Script_Lao,
    Script_Tibetan,
    Script_Myanmar,
    Script_Georgian,
    Script_Hangul,
    Script_Ethiopic,
    Script_Cherokee,
    Script_CanadianAboriginal,
    Script_Ogham,
    Script_Runic,
    Script_Khmer,
    Script_Mongolian,
    Script_Hiragana,
    Script_Katakana,
    Script_Bopomofo,
    Script_Han,
    Script_Yi,
    Script_OldItalic,
    Script_Gothic,
    Script_Deseret,
    Script_Tagalog,
    Script_Hanunoo,
    Script_Buhid,
    Script_Tagbanwa,
    Script_Coptic,

    // Unicode 4.0 additions
    Script_Limbu,
    Script_TaiLe,
    Script_LinearB,
    Script_Ugaritic,
    Script_Shavian,
    Script_Osmanya,
    Script_Cypriot,
    Script_Braille,

    // Unicode 4.1 additions
    Script_Buginese,
    Script_NewTaiLue,
    Script_Glagolitic,
    Script_Tifinagh,
    Script_SylotiNagri,
    Script_OldPersian,
    Script_Kharoshthi,

    // Unicode 5.0 additions
    Script_Balinese,
    Script_Cuneiform,
    Script_Phoenician,
    Script_PhagsPa,
    Script_Nko,

    // Unicode 5.1 additions
    Script_Sundanese,
    Script_Lepcha,
    Script_OlChiki,
    Script_Vai,
    Script_Saurashtra,
    Script_KayahLi,
    Script_Rejang,
    Script_Lycian,
    Script_Carian,
    Script_Lydian,
    Script_Cham,

    // Unicode 5.2 additions
    Script_TaiTham,
    Script_TaiViet,
    Script_Avestan,
    Script_EgyptianHieroglyphs,
    Script_Samaritan,
    Script_Lisu,
    Script_Bamum,
    Script_Javanese,
    Script_MeeteiMayek,
    Script_ImperialAramaic,
    Script_OldSouthArabian,
    Script_InscriptionalParthian,
    Script_InscriptionalPahlavi,
    Script_OldTurkic,
    Script_Kaithi,

    // Unicode 6.0 additions
    Script_Batak,
    Script_Brahmi,
    Script_Mandaean,

    // Unicode 6.1 additions
    Script_Chakma,
    Script_MeroiticCursive,
    Script_MeroiticHieroglyphs,
    Script_Miao,
    Script_Sharada,
    Script_SoraSompeng,
    Script_Takri,

    // Unicode 7.0 additions
    Script_CaucasianAlbanian,
    Script_BassaVah,
    Script_Duployan,
    Script_Elbasan,
    Script_Grantha,
    Script_PahawhHmong,
    Script_Khojki,
    Script_LinearA,
    Script_Mahajani,
    Script_Manichaean,
    Script_MendeKikakui,
    Script_Modi,
    Script_Mro,
    Script_OldNorthArabian,
    Script_Nabataean,
    Script_Palmyrene,
    Script_PauCinHau,
    Script_OldPermic,
    Script_PsalterPahlavi,
    Script_Siddham,
    Script_Khudawadi,
    Script_Tirhuta,
    Script_WarangCiti,

    // Unicode 8.0 additions
    Script_Ahom,
    Script_AnatolianHieroglyphs,
    Script_Hatran,
    Script_Multani,
    Script_OldHungarian,
    Script_SignWriting,

    // Unicode 9.0 additions
    Script_Adlam,
    Script_Bhaiksuki,
    Script_Marchen,
    Script_Newa,
    Script_Osage,
    Script_Tangut,

    // Unicode 10.0 additions
    Script_MasaramGondi,
    Script_Nushu,
    Script_Soyombo,
    Script_ZanabazarSquare,

    // Unicode 12.1 additions
    Script_Dogra,
    Script_GunjalaGondi,
    Script_HanifiRohingya,
    Script_Makasar,
    Script_Medefaidrin,
    Script_OldSogdian,
    Script_Sogdian,
    Script_Elymaic,
    Script_Nandinagari,
    Script_NyiakengPuachueHmong,
    Script_Wancho,

    // Unicode 13.0 additions
    Script_Chorasmian,
    Script_DivesAkuru,
    Script_KhitanSmallScript,
    Script_Yezidi,

    ScriptCount
};

enum Direction
{
    DirL, DirR, DirEN, DirES, DirET, DirAN, DirCS, DirB, DirS, DirWS, DirON,
    DirLRE, DirLRO, DirAL, DirRLE, DirRLO, DirPDF, DirNSM, DirBN,
    DirLRI, DirRLI, DirFSI, DirPDI
};

enum Decomposition
{
    NoDecomposition,
    Canonical,
    Font,
    NoBreak,
    Initial,
    Medial,
    Final,
    Isolated,
    Circle,
    Super,
    Sub,
    Vertical,
    Wide,
    Narrow,
    Small,
    Square,
    Compat,
    Fraction
};

enum JoiningType {
    Joining_None,
    Joining_Causing,
    Joining_Dual,
    Joining_Right,
    Joining_Left,
    Joining_Transparent
};

enum SpecialChar {
    Null = 0x0000,
    Tabulation = 0x0009,
    LineFeed = 0x000a,
    FormFeed = 0x000c,
    CarriageReturn = 0x000d,
    Space = 0x0020,
    Nbsp = 0x00a0,
    SoftHyphen = 0x00ad,
    ReplacementCharacter = 0xfffd,
    ObjectReplacementCharacter = 0xfffc,
    ByteOrderMark = 0xfeff,
    ByteOrderSwapped = 0xfffe,
    ParagraphSeparator = 0x2029,
    LineSeparator = 0x2028,
    LastValidCodePoint = 0x10ffff,
#ifdef _WIN32
    PathSeparator = char('\\')
#else
    PathSeparator = char('/')
#endif
};

struct LKCOREDEF Util
{
    static const Properties* properties(uint32_t ucs4) noexcept;
    static const Properties* properties(uint16_t ucs2) noexcept;

    static GraphemeBreakClass graphemeBreakClass(uint32_t ucs4) noexcept;
    static WordBreakClass wordBreakClass(uint32_t ucs4) noexcept;
    static SentenceBreakClass sentenceBreakClass(uint32_t ucs4) noexcept;
    static LineBreakClass lineBreakClass(uint32_t ucs4) noexcept;


    static void initCharAttributes(const uint16_t *string, int length,
                            LCharAttributes *attributes,
                            LFlags<LCharAttributeOption> options = LCharAttributeOption::DefaultOptionsCompat);


    static void initScripts(const uint16_t *string, int length, uint8_t *scripts);
    static bool isScriptLeftToRight(uint8_t script);
    static const std::string& scriptShortName(uint8_t script);
    static uint8_t shortNameToScript(std::string name);
};
}

LK_NAMESPACE_END
#endif // LCHAR_H
