#ifndef LTEXTENGINE_H
#define LTEXTENGINE_H
#include "lfont.h"
#include "lkcore/text/lchar.h"
#include "lkgui/paint/lline.h"
#include "lkgui/text/ltextoption.h"
#include "lkgui/text/ltextitem.h"
#include "lkgui/text/ltextlayout.h"

LK_NAMESPACE_BEGIN

struct LScriptAnalysis
{
    enum Flags {
        None = 0,
        Lowercase = 1,
        Uppercase = 2,
        SmallCaps = 3,
        LineOrParagraphSeparator = 4,
        Space = 5,
        SpaceTabOrObject = Space,
        Nbsp = 6,
        Tab = 7,
        TabOrObject = Tab,
        Object = 8
    };
    enum BidiFlags {
        BidiBN = 1,
        BidiMaybeResetToParagraphLevel = 2,
        BidiResetToParagraphLevel = 4,
        BidiMirrored = 8
    };
    unsigned short script    : 8;
    unsigned short flags     : 4;
    unsigned short bidiFlags : 4;
    unsigned short bidiLevel : 8;  // Unicode Bidi algorithm embedding level (0-125)
    lchar::Direction bidiDirection : 8; // used when running the bidi algorithm
    inline bool operator == (const LScriptAnalysis &other) const {
        return script == other.script && bidiLevel == other.bidiLevel && flags == other.flags;
    }
};


struct LScriptItem
{
    LScriptItem(int p, LScriptAnalysis a) noexcept
        : position(p), analysis(a),
        numGlyphs(0), descent(-1), ascent(-1), leading(-1), width(-1),
        glyphDataOffset(0) {}

    int position;
    LScriptAnalysis analysis;
    unsigned short numGlyphs;
    double descent;
    double ascent;
    double leading;
    double width;
    int glyphDataOffset;
    double height() const noexcept { return ascent + descent; }
private:
    friend class std::vector<LScriptItem>;
    LScriptItem() {} // for vector, don't use
};


class LTextEngine;
struct LScriptLine
{
    // created and filled in QTextLine::layout_helper
    LScriptLine()
        : length(0),
        justified(0), gridfitted(0),
        hasTrailingSpaces(0), leadingIncluded(0) {}
    double descent = 0;
    double ascent = 0;
    double leading = 0;
    double x = 0;
    double y = 0;
    double width = 0;
    double textWidth = 0;
    double textAdvance = 0;
    int from  = 0;
    unsigned short trailingSpaces = 0;
    signed int length : 28;
    mutable uint32_t justified : 1;
    mutable uint32_t gridfitted : 1;
    uint32_t hasTrailingSpaces : 1;
    uint32_t leadingIncluded : 1;
    double height() const { return ascent + descent
                                   + (leadingIncluded?  std::max<double>(0,leading) : 0); }
    double base() const { return ascent; }
    void setDefaultHeight(std::shared_ptr<LTextEngine> eng);
    void operator+=(const LScriptLine &other);
};

class LPainter;
class LPen;
class LKGUIDEF LTextEngine : public LIVar
{
public:
    LTextEngine();
    LTextEngine(const std::wstring &string, std::shared_ptr<LFont> f);

    enum LayoutState {
        LayoutEmpty,
        InLayout,
        LayoutFailed
    };

    struct LayoutData {
        LayoutData(const std::wstring &str, void **stackMemory, int memSize);
        LayoutData();
        ~LayoutData();
        bool reallocate(int totalGlyphs);

        mutable std::vector<LScriptItem> m_items;
        int m_allocated;
        int m_availableGlyphs;
        void **m_memory;
        unsigned short *m_logClustersPtr;
        LGlyphLayout m_glyphLayout;
        mutable int m_used;
        uint32_t m_hasBidi : 1;
        uint32_t m_layoutState : 2;
        uint32_t m_memoryOnStack : 1;  //memory中数据内存由外部管理
        uint32_t m_haveCharAttributes : 1;
        std::wstring m_string;
    };

    struct ItemDecoration {
        ItemDecoration() {}
        ItemDecoration(double x1, double x2, double y, const std::shared_ptr<LPen> pen):
            x1(x1), x2(x2), y(y), pen(pen) {}

        double x1 = 0;
        double x2 = 0;
        double y = 0;
        std::shared_ptr<LPen> pen = nullptr;
    };


    void invalidate();
    void clearLineData();

    void addUnderline(LPainter *painter, const LLine<double> &line);
    void addStrikeOut(LPainter *painter, const LLine<double> &line);
    void addOverline(LPainter *painter, const LLine<double> &line);

    void validate() const;
    void itemize() const;

    void justify(const LScriptLine &si);
    void shapeLine(const LScriptLine &line);
    double leadingSpaceWidth(const LScriptLine &line);
    int findItem(int strPos, int firstItem = 0) const;
    bool isRightToLeft() const;
    double width(int charFrom, int numChars) const;
    static void bidiReorder(int numRuns, const uint8_t *levels, int *visualOrder);

    std::shared_ptr<LFont> font(const LScriptItem &si) const;
    const std::shared_ptr<LFont> font() const;
    inline void setFont(std::shared_ptr<LFont> f){
        m_font = f;
    }

    inline uint16_t *logClusters(const LScriptItem *si) const
    { return m_layoutData->m_logClustersPtr+si->position; }

    inline LGlyphLayout availableGlyphs(const LScriptItem *si) const {
        return m_layoutData->m_glyphLayout.mid(si->glyphDataOffset);
    }
    inline LGlyphLayout shapedGlyphs(const LScriptItem *si) const {
        return m_layoutData->m_glyphLayout.mid(si->glyphDataOffset, si->numGlyphs);
    }
    inline bool ensureSpace(int nGlyphs) const {
        if (m_layoutData->m_glyphLayout.numGlyphs - m_layoutData->m_used < nGlyphs){
            return m_layoutData->reallocate((((m_layoutData->m_used + nGlyphs)*3/2 + 15) >> 4) << 4);
        }
        return true;
    }

    void enableDelayDecorations(bool enable = true) { m_delayDecorations = enable; }

    int length(uint32_t item) const;
    int length(const LScriptItem *si) const;

    inline bool hasFormats() const
    { return (m_specialData != nullptr) && !m_specialData->m_formats.empty(); }
    inline std::vector<LTextLayout::FormatRange> formats() const
    { return (m_specialData != nullptr) ? m_specialData->m_formats : std::vector<LTextLayout::FormatRange>(); }
    void setFormats(const std::vector<LTextLayout::FormatRange> &formats);
    LTextCharFormat format(const LScriptItem *si) const;
    int formatIndex(const LScriptItem *si) const;

    void freeMemory();
    int lineNumberForTextPosition(int pos);
    void drawDecorations(LPainter *painter);
    void clearDecorations();
    void adjustUnderlines();
    const LCharAttributes *attributes() const;
    void shape(int item) const;
    inline std::shared_ptr<LFontEngine> fontEngine() const{
        return m_font->engine();
    }

    double calculateTabWidth(int index, double x) const;
    double alignLine(const LScriptLine &line);
    double offsetInLigature(const LScriptItem *si, int pos, int max, int glyphPos) const;

    inline std::shared_ptr<LTextFormatCollection> formatCollection() const {
        return (m_specialData != nullptr) ? m_specialData->m_formatCollection : nullptr;
    }

    std::vector<int> insertionPointsForLine(int lineNum) const;
    int endOfLine(int lineNum) const;
    int beginningOfLine(int lineNum) const;
    int positionInLigature(const LScriptItem *si, int end,
                           double x, double edge,
                           int glyphPos, bool cursorOnCharacter) const;
    int previousLogicalPosition(int oldPos) const;
    int nextLogicalPosition(int oldPos) const;
    bool atWordSeparator(int position) const;
    int positionAfterVisualMovement(int oldPos, LMoveOper op);

    std::wstring elidedText(LTextElide mode,
                            const double width,
                            int flags = 0,
                            int from = 0,
                            int count = -1) const;

    std::wstring m_text;

    LTextOption m_option;

    double m_minWidth;
    double m_maxWidth;
    LPoint<double> m_position;
    uint8_t m_ignoreBidi : 1;
    uint8_t m_cacheGlyphs : 1;
    uint8_t m_stackEngine : 1;
    uint8_t m_forceJustification : 1;
    uint8_t m_visualMovement : 1;
    uint8_t m_delayDecorations: 1;
    mutable std::shared_ptr<LayoutData> m_layoutData;

    std::vector<ItemDecoration> m_underlines;
    std::vector<ItemDecoration> m_strikeOuts;
    std::vector<ItemDecoration> m_overlines;
    std::vector<LScriptLine> m_lines;
private:
    std::shared_ptr<LFont> m_font;

    struct SpecialData {
        int m_preeditPosition;
        std::wstring m_preeditText;
        std::vector<LTextLayout::FormatRange> m_formats;
        std::vector<LTextCharFormat> m_resolvedFormats;
        std::shared_ptr<LTextFormatCollection> m_formatCollection;
    };
    std::shared_ptr<SpecialData> m_specialData;


    static void init(LTextEngine *e);
    void addItemDecoration(LPainter *painter,
                           const LLine<double> &line,
                           std::vector<ItemDecoration>& decorations);
    void indexFormats();
    void resolveFormats() const;

    void drawItemDecorations(LPainter *painter, const std::vector<ItemDecoration>& decorations);
    void adjustUnderlines(int start,
                          int end,
                          double underlinePos,
                          double penWidt);
    void shapeText(int item) const;
    void addRequiredBoundaries() const;
    void setBoundary(int strPos) const;
    int getClusterLength(uint16_t *logClusters, const LCharAttributes *attributes,
                         int from, int to, int glyphPos, int *start) const;

};


class LKGUIDEF LStackTextEngine : public LTextEngine {
public:
    enum { MemSize = 256*40/sizeof(void *) };
    LStackTextEngine(const std::wstring &str, std::shared_ptr<LFont> f);
    //LayoutData _layoutData;
    //void* m_memory[MemSize];
};

struct LTextLineItemIterator
{
    LTextLineItemIterator(std::shared_ptr<LTextEngine> _eng,
                          int lineNum,
                          const LPoint<double> &pos = LPoint<double>(),
                          const LTextLayout::FormatRange *_selection = nullptr);

    inline bool atEnd() const { return logicalItem >= nItems - 1; }
    inline bool atBeginning() const { return logicalItem <= 0; }
    LScriptItem &next();

    bool getSelectionBounds(double *selectionX, double *selectionWidth) const;
    inline bool isOutsideSelection() const {
        double tmp1, tmp2;
        return !getSelectionBounds(&tmp1, &tmp2);
    }

    std::shared_ptr<LTextEngine> eng;

    double x;
    const LScriptLine &line;
    LScriptItem *si;

    const int lineNum;
    const int lineEnd;
    const int firstItem;
    const int lastItem;
    const int nItems;
    int logicalItem;
    int item;
    int itemLength;

    int glyphsStart;
    int glyphsEnd;
    int itemStart;
    int itemEnd ;

    double itemWidth;
    std::vector<int> visualOrder;
    const LTextLayout::FormatRange *selection;
};

LK_NAMESPACE_END

#endif // LTEXTENGINE_H
