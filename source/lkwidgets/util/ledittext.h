#ifndef LEDITTEXT_H
#define LEDITTEXT_H
#include "leditbase.h"
#include "lkgui/text/ltextlayout.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LEditText : public LEditBase
{
public:
    LEditText();

    virtual bool isText() const override;
    virtual uint32_t type() const override;
    virtual bool isScript() const; //是否是输入脚本，指令字串
    virtual bool isSriptEnd() const; //输入脚本是否结束
    virtual bool canMergeText(const std::wstring& ch = L"") const;  //是否允许文本合并

    virtual int activePos() const override;
    virtual void setActivePos(int pos) override;
    virtual bool isAtStart() const override;
    virtual bool isAtEnd() const override;
    virtual void moveToStart() override;
    virtual void moveToEnd() override;

    virtual std::list<std::shared_ptr<LEditOp> > removeSelection() override;
    virtual bool moveCursor(LMoveOper op) override;
    virtual int maxIndex() const override;

    virtual LPoint<double> activePoint() const override;
    virtual bool setToActivePoint(const LPoint<double>& p) override;

    void buildTextIndexes(std::vector<int>& indexes);
    std::vector<int> textIndexes();

    virtual std::list<std::shared_ptr<LEditOp> > input(const std::wstring& v);

    virtual std::wstring parseInput(const std::wstring& v);
    virtual void setText(const std::wstring& v);
    virtual void inputText(const std::wstring& v);
    virtual std::shared_ptr<LEditText> create(const std::wstring& v);
    virtual int maxLineWidth() const;

    virtual const std::wstring& text() const{
        return m_textLayout.text();
    }

    std::wstring selectText() const;

    double naturalTextWidth() const { return m_textLayout.lineAt(0).naturalTextWidth(); }

    virtual void calculateSize() override;
    virtual void draw(LPainter *painter,
                      const LPoint<double> &pos = LPoint<double>(),
                      int flags = (int)(DrawFlags::All),
                      const LRect<double> &clip = LRect<double>()) const override;

    virtual void drawCursor(LPainter *p,
                            const LPoint<double> &pos = LPoint<double>(),
                            int width = 1,
                            const LRect<double> &clip = LRect<double>()) const override;

    bool deletePos(int pos, int length = 1);
    bool deleteActive(bool isForward);
    int xToPos(double x, LTextLine::CursorPosition = LTextLine::CursorBetweenCharacters) const;
    int yToLine(double y);

    inline int textLine(int cursor) const{
        return m_textLayout.lineNumberForTextPosition(cursor);
    }

    inline double cursorToX(int cursor) const
    {
        return m_textLayout.lineForTextPosition(cursor).cursorToX(cursor)+m_rect.m_x;
    }

    inline double cursorToY(int cursor) const{
        return m_textLayout.lineForTextPosition(cursor).y()+m_rect.m_y;
    }

    inline double cursorToX() const{
        return cursorToX(m_activePos);
    }

    inline double cursorToY() const{
        return cursorToY(m_activePos);
    }

    LRect<double> rectForPos(int cursor, int cursorWith) const;
    inline const LTextLayout& textlayout(){
        return m_textLayout;
    }
protected:
    int m_activePos = 0;
    mutable LTextLayout m_textLayout;

    void redoTextLayout() const;
    virtual void clear() override;
    virtual void reserve(const std::wstring& v);

    friend class LTextMergeSplit;
};

LK_NAMESPACE_END

#endif // LEDITTEXT_H
