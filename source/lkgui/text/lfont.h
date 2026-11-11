#ifndef LFONT_H
#define LFONT_H
#include "lkcore/lglobal.h"
#include "lkcore/livar.h"
#include <list>

LK_NAMESPACE_BEGIN
class LPaintDevice;
class LFontEngine;
class LFontDatabase;
class LPainter;
class LTextEngine;
class LKGUIDEF LFont : public LIVar
{
public:
    LFont();
    LFont(const std::shared_ptr<LFont> font);
    LFont(const LFont &font);
    LFont(const std::wstring &family, int pointSize = -1, int weight = -1, bool italic = false);
    LFont(const std::string &family, int pointSize = -1, int weight = -1, bool italic = false);

    enum StyleHint {
        Helvetica,  SansSerif = Helvetica,
        Times,      Serif = Times,
        Courier,    TypeWriter = Courier,
        OldEnglish, Decorative = OldEnglish,
        System,
        AnyStyle,
        Cursive,
        Monospace,
        Fantasy
    };

    enum Style {
        StyleNormal,
        StyleItalic,
        StyleOblique
    };

    enum StyleStrategy {
        PreferDefault       = 0x0001,
        PreferBitmap        = 0x0002,
        PreferDevice        = 0x0004,
        PreferOutline       = 0x0008,
        ForceOutline        = 0x0010,
        PreferMatch         = 0x0020,
        PreferQuality       = 0x0040,
        PreferAntialias     = 0x0080,
        NoAntialias         = 0x0100,
        NoSubpixelAntialias = 0x0800
    };


    enum Stretch {
        AnyStretch     =   0,
        UltraCondensed =  50,
        ExtraCondensed =  62,
        Condensed      =  75,
        SemiCondensed  =  87,
        Unstretched    = 100,
        SemiExpanded   = 112,
        Expanded       = 125,
        ExtraExpanded  = 150,
        UltraExpanded  = 200
    };


    enum HintingPreference {
        PreferDefaultHinting        = 0,
        PreferNoHinting             = 1,
        PreferVerticalHinting       = 2,
        PreferFullHinting           = 3
    };

    enum Weight {
        Thin     = 0,    // 100
        ExtraLight = 12, // 200
        Light    = 25,   // 300
        Normal   = 50,   // 400
        Medium   = 57,   // 500
        DemiBold = 63,   // 600
        Bold     = 75,   // 700
        ExtraBold = 81,  // 800
        Black    = 87    // 900
    };

    enum Capitalization {
        MixedCase,
        AllUppercase,
        AllLowercase,
        SmallCaps,
        Capitalize
    };

    enum SpacingType {
        PercentageSpacing,
        AbsoluteSpacing
    };

    enum ResolveProperties {
        NoPropertiesResolved        = 0x0000,
        FamilyResolved              = 0x0001,
        SizeResolved                = 0x0002,
        StyleHintResolved           = 0x0004,
        StyleStrategyResolved       = 0x0008,
        WeightResolved              = 0x0010,
        StyleResolved               = 0x0020,
        UnderlineResolved           = 0x0040,
        OverlineResolved            = 0x0080,
        StrikeOutResolved           = 0x0100,
        FixedPitchResolved          = 0x0200,
        StretchResolved             = 0x0400,
        KerningResolved             = 0x0800,
        CapitalizationResolved      = 0x1000,
        LetterSpacingResolved       = 0x2000,
        WordSpacingResolved         = 0x4000,
        HintingPreferenceResolved   = 0x8000,
        StyleNameResolved           = 0x10000,
        AllPropertiesResolved       = 0x1ffff
    };


    struct Def
    {
        inline Def()
            : pointSize(-1.0), pixelSize(-1.0),
            styleStrategy(LFont::PreferDefault), styleHint(LFont::AnyStyle),
            weight(50), fixedPitch(false), style(LFont::StyleNormal), stretch(LFont::AnyStretch),
            hintingPreference(LFont::PreferDefaultHinting), ignorePitch(true),
            fixedPitchComputed(0), reserved(0)
        {
        }

        std::wstring family;
        std::wstring styleName;

        double pointSize; //不参与比较
        double pixelSize;

        uint32_t styleStrategy : 16;
        uint32_t styleHint     : 8;

        uint32_t weight     :  7; // 0-99
        uint32_t fixedPitch :  1;
        uint32_t style      :  2;
        uint32_t stretch    : 12; // 0-4000

        uint32_t hintingPreference : 2;
        uint32_t ignorePitch : 1;
        uint32_t fixedPitchComputed : 1; // for Mac OS X only
        uint32_t reserved   : 14; // for future extensions

        bool exactMatch(const Def &other) const;
        bool operator==(const Def &other) const
        {
            return pixelSize == other.pixelSize
                   && weight == other.weight
                   && style == other.style
                   && stretch == other.stretch
                   && styleHint == other.styleHint
                   && styleStrategy == other.styleStrategy
                   && ignorePitch == other.ignorePitch && fixedPitch == other.fixedPitch
                   && family == other.family
                   && styleName == other.styleName
                   && hintingPreference == other.hintingPreference
                ;
        }
        inline bool operator<(const Def &other) const
        {
            if (pixelSize != other.pixelSize){
                return pixelSize < other.pixelSize;
            }
            if (weight != other.weight){
                return weight < other.weight;
            }
            if (style != other.style){
                return style < other.style;
            }
            if (stretch != other.stretch){
                return stretch < other.stretch;
            }
            if (styleHint != other.styleHint){
                return styleHint < other.styleHint;
            }
            if (styleStrategy != other.styleStrategy){
                return styleStrategy < other.styleStrategy;
            }
            if (family != other.family){
                return family < other.family;
            }
            if (styleName != other.styleName){
                return styleName < other.styleName;
            }
            if (hintingPreference != other.hintingPreference){
                return hintingPreference < other.hintingPreference;
            }

            if (ignorePitch != other.ignorePitch){
                return ignorePitch < other.ignorePitch;
            }
            if (fixedPitch != other.fixedPitch){
                return fixedPitch < other.fixedPitch;
            }
            return false;
        }
    };

    std::shared_ptr<LFontEngine> engine();

    void setStyle(Style style);
    Style style() const;

    inline bool italic() const
    {
        return (style() != StyleNormal);
    }

    inline void setItalic(bool b) {
        setStyle(b ? StyleItalic : StyleNormal);
    }

    const std::wstring& family() const;
    void setFamily(const std::wstring &);
    void setFamily(const std::string &);

    const std::wstring& styleName() const;
    void setStyleName(const std::wstring &);
    void setStyleName(const std::string &);

    int weight() const;
    void setWeight(int);

    int pointSize() const;
    void setPointSize(int);

    double pointSizeF() const;
    void setPointSizeF(double);

    int pixelSize() const;
    void setPixelSize(int);

    double pixelSizeF() const;
    void setPixelSizeF(double);

    bool underline() const;
    void setUnderline(bool);

    bool overline() const;
    void setOverline(bool);

    bool strikeOut() const;
    void setStrikeOut(bool);
    void alterCharForCapitalization(wchar_t &c) const;

    void setCapitalization(Capitalization);
    Capitalization capitalization() const;

    double letterSpacing() const;
    SpacingType letterSpacingType() const;
    void setLetterSpacing(SpacingType type, double spacing);

    double wordSpacing() const;
    void setWordSpacing(double spacing);

    bool fixedPitch() const;
    void setFixedPitch(bool);

    bool kerning() const;
    void setKerning(bool);

    StyleHint styleHint() const;
    StyleStrategy styleStrategy() const;
    void setStyleHint(StyleHint, StyleStrategy = PreferDefault);
    void setStyleStrategy(StyleStrategy s);

    void setHintingPreference(HintingPreference hintingPreference);
    HintingPreference hintingPreference() const;

    int stretch() const;
    void setStretch(int);
    std::shared_ptr<LFont> resolve(const std::shared_ptr<LFont> ) const;
    inline uint32_t getResolve() const { return m_resolveMask; }
    inline void setResolve(uint32_t mask) { m_resolveMask = mask; }

    std::shared_ptr<LFontEngine> engine() const;

    static int defaultDpiX();
    static int defaultDpiY();
    static int defaultDpi();

    bool operator==(const LFont &f) const;
    bool operator!=(const LFont &f) const;
    bool operator<(const LFont &f) const;

    inline int dpi(){
        return m_dpi;
    }
protected:
    void resolve(uint32_t mask, const std::shared_ptr<LFont> other);

    std::shared_ptr<LFontEngine> m_engine = nullptr;


    Def m_request;
    uint32_t m_resolveMask=0;
    int m_dpi;

    uint32_t m_underline  :  1;
    uint32_t m_overline   :  1;
    uint32_t m_strikeOut  :  1;
    uint32_t m_kerning    :  1;
    uint32_t m_capital    :  3;
    bool m_letterSpacingIsAbsolute : 1;

    double m_letterSpacing = 0;
    double m_wordSpacing = 0;

    friend class LFontDatabase;
    friend class LPainter;
    friend class LTextEngine;
};

LK_NAMESPACE_END

template<>
struct std::hash<_LK::LFont::Def>
{
    std::size_t operator()(const _LK::LFont::Def& fd) const noexcept
    {
        uint32_t seed=0;
        seed = _LK::lHashCombine(seed, std::llround(fd.pixelSize*10000)); // use only 4 fractional digits
        seed = _LK::lHashCombine(seed, fd.weight);
        seed = _LK::lHashCombine(seed, fd.style);
        seed = _LK::lHashCombine(seed, fd.stretch);
        seed = _LK::lHashCombine(seed, fd.styleHint);
        seed = _LK::lHashCombine(seed, fd.styleStrategy);
        seed = _LK::lHashCombine(seed, fd.ignorePitch);
        seed = _LK::lHashCombine(seed, fd.fixedPitch);
        seed = _LK::lHashCombine(seed, fd.family);
        seed = _LK::lHashCombine(seed, fd.styleName);
        seed = _LK::lHashCombine(seed, fd.hintingPreference);
        return seed;
    }
};
#endif // LFONT_H
