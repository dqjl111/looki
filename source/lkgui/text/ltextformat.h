#ifndef LTEXTFORMAT_H
#define LTEXTFORMAT_H
#include "lkgui/paint/lcolor.h"
#include "lkgui/text/lfont.h"
#include "lkcore/io/lcbor.h"
#include "lkcore/lvariant.h"


LK_NAMESPACE_BEGIN
class LBrush;
class LPen;
class LTextFormatData;
class LTextCharFormat;
class LTextFormatCollection;
class LKGUIDEF LTextFormat
{
public:
    LTextFormat();

    LTextFormat(const LTextFormat &rhs);
    LTextFormat &operator=(const LTextFormat &rhs);

    enum FormatType {
        InvalidFormat = -1,
        BlockFormat = 1,
        CharFormat = 2,
        ListFormat = 3,
        FrameFormat = 5,
        UserFormat = 100
    };

    enum UnderlineStyle { // keep in sync with Qt::PenStyle!
        NoUnderline,
        SingleUnderline,
        DashUnderline,
        DotLine,
        DashDotLine,
        DashDotDotLine,
        WaveUnderline,
        SpellCheckUnderline
    };

    enum Property {
        ObjectIndex = 0x0,

        // paragraph and char
        CssFloat = 0x0800,
        LayoutDirection = 0x0801,

        OutlinePen = 0x810,
        BackgroundBrush = 0x820,
        ForegroundBrush = 0x821,
        ObjectSelectionBrush = 0x822,
        BackgroundImageUrl = 0x823,

        // paragraph
        BlockAlignment = 0x1010,
        BlockTopMargin = 0x1030,
        BlockBottomMargin = 0x1031,
        BlockLeftMargin = 0x1032,
        BlockRightMargin = 0x1033,
        TextIndent = 0x1034,
        TabPositions = 0x1035,
        BlockIndent = 0x1040,
        LineHeight = 0x1048,
        LineHeightType = 0x1049,
        BlockNonBreakableLines = 0x1050,
        BlockTrailingHorizontalRulerWidth = 0x1060,
        HeadingLevel = 0x1070,
        BlockQuoteLevel = 0x1080,
        BlockCodeLanguage = 0x1090,
        BlockCodeFence = 0x1091,
        BlockMarker = 0x10A0,

        // character properties
        FirstFontProperty = 0x1FE0,
        FontCapitalization = FirstFontProperty,
        FontLetterSpacingType = 0x2033,
        FontLetterSpacing = 0x1FE1,
        FontWordSpacing = 0x1FE2,
        FontStretch = 0x2034,
        FontStyleHint = 0x1FE3,
        FontStyleStrategy = 0x1FE4,
        FontKerning = 0x1FE5,
        FontHintingPreference = 0x1FE6,
//        FontFamilies = 0x1FE7,
        FontStyleName = 0x1FE8,
        FontFamily = 0x2000,
        FontPointSize = 0x2001,
        FontSizeAdjustment = 0x2002,
        FontSizeIncrement = FontSizeAdjustment, // old name, compat
        FontWeight = 0x2003,
        FontItalic = 0x2004,
        FontUnderline = 0x2005,
        FontOverline = 0x2006,
        FontStrikeOut = 0x2007,
        FontFixedPitch = 0x2008,
        FontPixelSize = 0x2009,
        LastFontProperty = FontPixelSize,

        TextUnderlineColor = 0x2010,
        TextVerticalAlignment = 0x2021,
        TextOutline = 0x2022,
        TextUnderlineStyle = 0x2023,
        TextToolTip = 0x2024,

        IsAnchor = 0x2030,
        AnchorHref = 0x2031,
        AnchorName = 0x2032,
        ObjectType = 0x2f00,

        // list properties
        ListStyle = 0x3000,
        ListIndent = 0x3001,
        ListNumberPrefix = 0x3002,
        ListNumberSuffix = 0x3003,

        // table and frame properties
        FrameBorder = 0x4000,
        FrameMargin = 0x4001,
        FramePadding = 0x4002,
        FrameWidth = 0x4003,
        FrameHeight = 0x4004,
        FrameTopMargin    = 0x4005,
        FrameBottomMargin = 0x4006,
        FrameLeftMargin   = 0x4007,
        FrameRightMargin  = 0x4008,
        FrameBorderBrush = 0x4009,
        FrameBorderStyle = 0x4010,

        TableColumns = 0x4100,
        TableColumnWidthConstraints = 0x4101,
        TableCellSpacing = 0x4102,
        TableCellPadding = 0x4103,
        TableHeaderRowCount = 0x4104,
        TableBorderCollapse = 0x4105,

        // table cell properties
        TableCellRowSpan = 0x4810,
        TableCellColumnSpan = 0x4811,

        TableCellTopPadding = 0x4812,
        TableCellBottomPadding = 0x4813,
        TableCellLeftPadding = 0x4814,
        TableCellRightPadding = 0x4815,

        TableCellTopBorder = 0x4816,
        TableCellBottomBorder = 0x4817,
        TableCellLeftBorder = 0x4818,
        TableCellRightBorder = 0x4819,

        TableCellTopBorderStyle = 0x481a,
        TableCellBottomBorderStyle = 0x481b,
        TableCellLeftBorderStyle = 0x481c,
        TableCellRightBorderStyle = 0x481d,

        TableCellTopBorderBrush = 0x481e,
        TableCellBottomBorderBrush = 0x481f,
        TableCellLeftBorderBrush = 0x4820,
        TableCellRightBorderBrush = 0x4821,

        // image properties
        ImageName = 0x5000,
        ImageTitle = 0x5001,
        ImageAltText = 0x5002,
        ImageWidth = 0x5010,
        ImageHeight = 0x5011,
        ImageQuality = 0x5014,

        // internal
        SuppressText = 0x5012,
        SuppressBackground = 0x513,

        // selection properties
        FullWidthSelection = 0x06000,

        // page break properties
        PageBreakPolicy = 0x7000,

        // --
        UserProperty = 0x100000
    };

    LVariant property(uint32_t propertyId) const;
    void setProperty(uint32_t propertyId, const LVariant &value);
    void clearProperty(uint32_t propertyId);
    bool hasProperty(uint32_t propertyId) const;

    bool boolProperty(int propertyId) const;
    int intProperty(int propertyId) const;
    double doubleProperty(int propertyId) const;
    std::string stringProperty(int propertyId) const;
    std::wstring wstringProperty(int propertyId) const;


    std::shared_ptr<LBrush> brushProperty(int propertyId) const;
    std::shared_ptr<LPen> penProperty(int propertyId) const;

    void setBackground(const std::shared_ptr<LBrush> brush);
    inline std::shared_ptr<LBrush> background() const
    { return brushProperty(BackgroundBrush); }
    inline void clearBackground()
    { clearProperty(BackgroundBrush); }

    void setForeground(const std::shared_ptr<LBrush> brush);
    inline std::shared_ptr<LBrush> foreground() const
    { return brushProperty(ForegroundBrush); }
    inline void clearForeground()
    { clearProperty(ForegroundBrush); }

    bool operator==(const LTextFormat &rhs) const;
    inline bool operator!=(const LTextFormat &rhs) const { return !operator==(rhs); }

    inline int type() const{
        return m_formatType;
    }
    void merge(const LTextFormat &other);

    inline bool isCharFormat() const { return m_formatType == CharFormat; }
//    inline bool isBlockFormat() const { return type() == BlockFormat; }
//    inline bool isListFormat() const { return type() == ListFormat; }
//    inline bool isFrameFormat() const { return type() == FrameFormat; }
//    inline bool isImageFormat() const { return type() == CharFormat && objectType() == ImageObject; }
//    inline bool isTableFormat() const { return type() == FrameFormat && objectType() == TableObject; }
//    inline bool isTableCellFormat() const { return type() == CharFormat && objectType() == TableCellObject; }

//    LTextBlockFormat toBlockFormat() const;
    LTextCharFormat toCharFormat() const;
//    LTextListFormat toListFormat() const;
//    LTextTableFormat toTableFormat() const;
//    LTextFrameFormat toFrameFormat() const;
//    LTextImageFormat toImageFormat() const;
//    LTextTableCellFormat toTableCellFormat() const;


protected:
    std::shared_ptr<LTextFormatData> m_data;
    int32_t m_formatType = InvalidFormat;

    friend class LTextFormatCollection;
};

class LKGUIDEF LTextCharFormat : public LTextFormat
{
public:
    enum VerticalAlignment {
        AlignNormal = 0,
        AlignSuperScript,
        AlignSubScript,
        AlignMiddle,
        AlignTop,
        AlignBottom,
        AlignBaseline
    };

    enum FontPropertiesInheritanceBehavior {
        FontPropertiesSpecifiedOnly,
        FontPropertiesAll
    };

    LTextCharFormat();

    void setFont(const std::shared_ptr<LFont> font,
                 FontPropertiesInheritanceBehavior = FontPropertiesAll);
    std::shared_ptr<LFont> font() const;

    inline void setFontUnderline(bool underline)
    { setProperty(TextUnderlineStyle, underline ? (uint32_t)LPenStyle::Solid : (uint32_t)LPenStyle::No); }
    bool fontUnderline() const;

    void setUnderlineStyle(UnderlineStyle style);
    inline LPenStyle underlineStyle() const
    { return static_cast<LPenStyle>(intProperty(TextUnderlineStyle)); }

    inline void setVerticalAlignment(VerticalAlignment alignment)
    { setProperty(TextVerticalAlignment, alignment); }
    inline VerticalAlignment verticalAlignment() const
    { return static_cast<VerticalAlignment>(intProperty(TextVerticalAlignment)); }

    void setTextOutline(const std::shared_ptr<LPen> pen);
    inline std::shared_ptr<LPen> textOutline() const
    { return penProperty(TextOutline); }

    inline void setFontFamily(const std::wstring &family)
    { setProperty(FontFamily, family); }
    inline std::wstring fontFamily() const
    { return wstringProperty(FontFamily); }

    inline void setFontStyleName(const std::wstring &styleName)
    { setProperty(FontStyleName, styleName); }
    inline std::wstring fontStyleName() const
    { return wstringProperty(FontStyleName); }

    inline void setFontPointSize(double size)
    { setProperty(FontPointSize, size); }
    inline double fontPointSize() const
    { return doubleProperty(FontPointSize); }

    inline void setFontWeight(int weight)
    { setProperty(FontWeight, weight); }
    inline int fontWeight() const
    { return hasProperty(FontWeight) ? intProperty(FontWeight) : LFont::Normal; }

    inline void setFontItalic(bool italic)
    { setProperty(FontItalic, italic); }
    inline bool fontItalic() const
    { return boolProperty(FontItalic); }

    inline void setFontOverline(bool overline)
    { setProperty(FontOverline, overline); }
    inline bool fontOverline() const
    { return boolProperty(FontOverline); }

    inline void setFontStrikeOut(bool strikeOut)
    { setProperty(FontStrikeOut, strikeOut); }
    inline bool fontStrikeOut() const
    { return boolProperty(FontStrikeOut); }

    inline void setFontFixedPitch(bool fixedPitch)
    { setProperty(FontFixedPitch, fixedPitch); }
    inline bool fontFixedPitch() const
    { return boolProperty(FontFixedPitch); }

    inline void setFontCapitalization(LFont::Capitalization capitalization)
    { setProperty(FontCapitalization, capitalization); }
    inline LFont::Capitalization fontCapitalization() const
    { return static_cast<LFont::Capitalization>(intProperty(FontCapitalization)); }

    inline void setFontWordSpacing(double spacing)
    { setProperty(FontWordSpacing, spacing); }
    inline double fontWordSpacing() const
    { return doubleProperty(FontWordSpacing); }

    inline void setFontLetterSpacingType(LFont::SpacingType letterSpacingType)
    { setProperty(FontLetterSpacingType, letterSpacingType); }
    inline LFont::SpacingType fontLetterSpacingType() const
    { return static_cast<LFont::SpacingType>(intProperty(FontLetterSpacingType)); }
    inline void setFontLetterSpacing(double spacing)
    { setProperty(FontLetterSpacing, spacing); }
    inline double fontLetterSpacing() const
    { return doubleProperty(FontLetterSpacing); }

    inline void setFontStretch(int factor)
    { setProperty(FontStretch, factor); }
    inline int fontStretch() const
    { return intProperty(FontStretch); }

    inline void setFontStyleHint(LFont::StyleHint hint, LFont::StyleStrategy strategy = LFont::PreferDefault)
    { setProperty(FontStyleHint, hint); setProperty(FontStyleStrategy, strategy); }
    inline void setFontStyleStrategy(LFont::StyleStrategy strategy)
    { setProperty(FontStyleStrategy, strategy); }
    LFont::StyleHint fontStyleHint() const
    { return static_cast<LFont::StyleHint>(intProperty(FontStyleHint)); }
    LFont::StyleStrategy fontStyleStrategy() const
    { return static_cast<LFont::StyleStrategy>(intProperty(FontStyleStrategy)); }

    inline void setFontHintingPreference(LFont::HintingPreference hintingPreference)
    {
        setProperty(FontHintingPreference, hintingPreference);
    }

    inline LFont::HintingPreference fontHintingPreference() const
    {
        return static_cast<LFont::HintingPreference>(intProperty(FontHintingPreference));
    }
    inline void setFontKerning(bool enable)
    { setProperty(FontKerning, enable); }
    inline bool fontKerning() const
    { return boolProperty(FontKerning); }

private:
    explicit LTextCharFormat(const LTextFormat &fmt);
    friend class LTextFormat;
};


class LKGUIDEF LTextFormatCollection
{
public:
    LTextFormatCollection() {}
    ~LTextFormatCollection();

    void clear();

    inline LTextFormat objectFormat(int objectIndex) const
    { return format(objectFormatIndex(objectIndex)); }
    inline void setObjectFormat(int objectIndex, const LTextFormat &format)
    { setObjectFormatIndex(objectIndex, indexForFormat(format)); }

    int objectFormatIndex(int objectIndex) const;
    void setObjectFormatIndex(int objectIndex, int formatIndex);

    int createObjectIndex(const LTextFormat &f);

    int indexForFormat(const LTextFormat &f);
    bool hasFormatCached(const LTextFormat &format) const;

    LTextFormat format(int idx) const;

    inline LTextCharFormat charFormat(int index)
    { return format(index).toCharFormat(); }

//    inline LTextBlockFormat blockFormat(int index) const
//    { return format(index).toBlockFormat(); }
//    inline LTextListFormat listFormat(int index) const
//    { return format(index).toListFormat(); }
//    inline LTextTableFormat tableFormat(int index) const
//    { return format(index).toTableFormat(); }
//    inline LTextImageFormat imageFormat(int index) const
//    { return format(index).toImageFormat(); }

    inline int numFormats() const { return m_formats.size(); }

    std::vector<LTextFormat> m_formats;
    std::vector<int32_t> m_objFormats;
    std::multimap<uint64_t,int> m_hashes;

    inline std::shared_ptr<LFont> defaultFont() const { return m_defaultFnt; }
    void setDefaultFont(const std::shared_ptr<LFont> f);
private:
    std::shared_ptr<LFont> m_defaultFnt;
};


LK_NAMESPACE_END
#endif // LTEXTFORMAT_H
