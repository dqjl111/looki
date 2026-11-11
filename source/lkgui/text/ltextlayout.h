#ifndef LTEXTLAYOUT_H
#define LTEXTLAYOUT_H
#include "lkgui/lrect.h"
#include "lkgui/text/ltextformat.h"
#include "lkgui/text/ltextoption.h"

LK_NAMESPACE_BEGIN
#define LK_TEXT_LINE_MAX_WIDTH 8388607

class LTextEngine;
class LPainter;
class LTextLine;
class LKGUIDEF LTextLayout
{
public:
    LTextLayout(std::shared_ptr<LTextEngine> e = nullptr);

    struct FormatRange {
        int start;
        int length;
        LTextCharFormat format;

        friend bool operator==(const FormatRange &lhs, const FormatRange &rhs)
        { return lhs.start == rhs.start && lhs.length == rhs.length && lhs.format == rhs.format; }
        friend bool operator!=(const FormatRange &lhs, const FormatRange &rhs)
        { return !operator==(lhs, rhs); }
    };

    inline std::shared_ptr<LTextEngine> engine() const { return m_engine; }

    const std::shared_ptr<LFont> font() const;
    void setFont(std::shared_ptr<LFont> f);

    void beginLayout();
    void endLayout();
    void clearLayout();

    LTextLine createLine();
    int lineCount() const;
    LTextLine lineAt(int i) const;
    LTextLine lineForTextPosition(int pos) const;
    int lineNumberForTextPosition(int pos) const;

    LPoint<double> position() const;
    void setPosition(const LPoint<double> &p);

    LRect<double> boundingRect() const;
    double minimumWidth() const;
    double maximumWidth() const;

    enum CursorMode {
        SkipCharacters,
        SkipWords
    };
    bool isValidCursorPosition(int pos) const;
    int nextCursorPosition(int oldPos, CursorMode mode = SkipCharacters) const;
    int previousCursorPosition(int oldPos, CursorMode mode = SkipCharacters) const;
    int leftCursorPosition(int oldPos) const;
    int rightCursorPosition(int oldPos) const;

    void setText(const std::wstring& v);
    const std::wstring& text() const;
    void insertText(int pos, const std::wstring& v);

    void draw(LPainter *p,
              const LPoint<double> &pos,
              const std::vector<FormatRange> &selections = std::vector<FormatRange>(),
              const LRect<double> &clip = LRect<double>()) const;
    void drawCursor(LPainter *p, const LPoint<double> &pos, int cursorPosition, int width =1) const;

    void setCacheEnabled(bool enable);
    bool cacheEnabled() const;
private:
    std::shared_ptr<LTextEngine> m_engine;
};

class LKGUIDEF LTextLine
{
public:
    inline LTextLine() : index(0), m_engine(nullptr) {}
//    LTextLine(int line, std::shared_ptr<LTextEngine> e) : index(line), m_engine(e) {}
    inline bool isValid() const { return m_engine != nullptr; }

    enum Edge {
        Leading,
        Trailing
    };
    enum CursorPosition {
        CursorBetweenCharacters,
        CursorOnCharacter
    };


    void setLineWidth(double width);
    void setPosition(const LPoint<double> &pos);
    LPoint<double> position() const;

    void setNumColumns(int columns);
    void setNumColumns(int columns, double alignmentWidth);

    int textStart() const;
    int textLength() const;

    int lineIndex() const { return index; }

    LRect<double> rect() const;
    double x() const;
    double y() const;

    double width() const;
    double ascent() const;
    double descent() const;
    double height() const;
    double leading() const;

    double naturalTextWidth() const;
    LRect<double> naturalTextRect() const;

    double horizontalAdvance() const;

    void draw(LPainter *p,
              const LPoint<double> &point,
              const LTextLayout::FormatRange *selection = nullptr) const;

    int xToCursor(double x, CursorPosition cpos = CursorBetweenCharacters) const;
    double cursorToX(int *cursorPos, Edge edge = Leading) const;
    inline double cursorToX(int cursorPos, Edge edge = Leading) const { return cursorToX(&cursorPos, edge); }
private:
    LTextLine(int line, std::shared_ptr<LTextEngine> e) : index(line), m_engine(e) {}
    void layoutHelper(int maxGlyphs);

    friend class LTextLayout;
    int index;
    std::shared_ptr<LTextEngine> m_engine;
};


LK_NAMESPACE_END
#endif // LTEXTLAYOUT_H
