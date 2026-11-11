#ifndef LEDITBASE_H
#define LEDITBASE_H
#include "lkgui/lrect.h"
#include "lkgui/lguienums.h"
#include "lkgui/paint/lpainter.h"
#include "lkgui/lpalette.h"
#include "lkwidgets/draw/lstyle.h"
#include <vector>


LK_NAMESPACE_BEGIN


enum class LEditType{
    Unknown = 0,
    Container = 1,
    Row = 2,
    Rows = 3,
    Text = 4,
    Editor = 5,
    UserDefine = 256
};

enum class LEditJob{
    Line,
    Doc,
    TextLine,
    TextDoc,
    UserDefine = 256
};


class LEditOp;
class LEditContainer;
class LEditBase;
class LEditDoc;
class LIEditor;

class LKWIDGETSDEF LEditBase : public std::enable_shared_from_this<LEditBase>
{
public:
    LEditBase();

    template<class T>
    std::shared_ptr<T> getEditT() const{
        std::shared_ptr<LEditBase> t =
            const_cast<LEditBase*>(this)->shared_from_this();
        return std::dynamic_pointer_cast<T>(t);
    }


    enum SelectState{NoSelect=0x00, PartialSelect=0x01};
    union{
        uint8_t m_editFlags = 0;
        struct{
            uint8_t m_cursorState : 1;
            uint8_t m_isHiding: 1;
            uint8_t m_readOnlyReserve: 1;
        };
    };

    LRect<double> m_rect;

    enum class CborId{
        Type,
        Job,
        Style,
        Text,
        Children,
        Notation,
        Bevelled,
        RowNum,
        ColumnNum,
        RowSpacing,
        ColumnSpacing,
        Dir,
        Open,
        Close
    };

    enum class DrawFlags {
        Text = 0x01,
        Selections = 0x02,
        Cursor = 0x04,
        All = Text | Selections | Cursor
    };


    static int compareIndexes(const std::vector<int>& i1, const std::vector<int>& i2);

    virtual LCbor::map toCbor(bool filterSelect) const;
    virtual bool fromCbor(const LCbor::map& cborMap,
                          bool checkType,
                          bool newStyle,
                          std::shared_ptr<LEditDoc> pDoc = nullptr);

    inline const LRect<double>& rect() const{
        return m_rect;
    }
    LRect<double> bounds() const{
        return m_isHiding ?  LRect<double>() : m_rect.marginsAdded(margins());
    }

    virtual bool isText() const;
    virtual bool isContainer() const;
    virtual bool isRow() const;

    virtual uint32_t type() const;

    virtual void draw(LPainter *painter,
                      const LPoint<double> &pos = LPoint<double>(),
                      int flags = (int)(DrawFlags::All),
                      const LRect<double> &clip = LRect<double>()) const;

    virtual void drawCursor(LPainter *p,
                            const LPoint<double> &pos = LPoint<double>(),
                            int width = 1,
                            const LRect<double> &clip = LRect<double>()) const;

    virtual bool isAtStart() const;
    virtual bool isAtEnd() const;

    virtual void moveToStart(){}
    virtual void moveToEnd(){}

    virtual bool hasSelect() const;
    virtual bool isAllSelect() const;

    virtual void deSelect();
    virtual void selectAll(bool isLeftToRight = true);
    virtual std::list<std::shared_ptr<LEditOp>> removeSelection();
    virtual bool moveCursor(LMoveOper oper);
    virtual bool mouseClick(const LPoint<double>& p);

    virtual int maxIndex() const  { return 0;}
    virtual int minIndex() const{ return 0;}
    virtual int activePos() const { return 0; }
    virtual void setActivePos(int pos) {}

    virtual LPoint<double> activePoint() const;
    virtual bool setToActivePoint(const LPoint<double>& p);

    virtual bool isStretchX() const;
    virtual bool isStretchY() const;

    virtual double upHeight() const;
    virtual double downHeight() const;

    virtual std::shared_ptr<LPalette> palette() const;
//    virtual uint8_t cursorWidth() const;
    virtual bool blinkStatus() const;

    virtual const std::shared_ptr<LFont> font() const;
    const std::shared_ptr<LFont> scriptFont(std::shared_ptr<LFont> f = nullptr) const;

    static double scriptFontSize(double baseSize, int level);
    static double mmlSpacing(LMathMLSpacing spaceType, double baseSize, int level);

    double marginLeft() const { return margins().m_left;}
    double marginTop() const { return margins().m_top;}
    double marginRight() const { return margins().m_right;}
    double marginBottom() const { return margins().m_bottom;}

    virtual const LMargins<double>& margins() const;
    virtual void setMargins(const LMargins<double>& v);
    static LMargins<double>& defaultMargins();

    void buildIndexes(std::vector<int>& indexes) const;
    std::vector<int> fullIndexes() const;
    static std::string indexesString(const std::vector<int>& indexes);

    double fullWidth() const;
    double fullHeight() const;

    double midX() const
    {
        return m_rect.m_x + m_rect.width()/2;
    }

    double midY() const
    {
        return m_rect.m_y + m_rect.height()/2;
    }

    std::shared_ptr<LEditContainer> parent() const{
        return m_parent.lock();
    }

    virtual std::shared_ptr<LIEditor> editor() const;
    std::shared_ptr<LStyle> style();
    void setStyle(std::shared_ptr<LStyle> s);

    bool isMathMLDisplayStyle() const;
    int stylecSriptlevel() const;

    int scriptLevel() const;
    inline int levelOffset() const{
        return m_levelOffset;
    }

    inline int partSelectStartId() const{
        return m_partSelectStartId;
    }

    inline int partSelectEndId() const{
        return m_partSelectEndId;
    }

    static std::shared_ptr<LEditBase> startBase();
    static bool isValid(std::shared_ptr<LEditBase> base);
    static bool isStart(std::shared_ptr<LEditBase> base);

    static double m_minFontSize;
    static double m_scriptSizeMultiplier;
protected:
    std::weak_ptr<LEditContainer> m_parent;
    int m_partSelectStartId = 0;
    int m_partSelectEndId = 0;
    std::shared_ptr<LStyle> m_style;
    int m_levelOffset = 0;


    inline double baseFullWidth() const{
        return m_rect.width() + marginLeft() + marginRight();
    }
    inline double baseFullHeight() const{
        return m_rect.height() + marginTop() + marginBottom();
    }


    enum SelectEndType{SelectAsValue, SelectToLeft, SelectToRight};
    virtual void selectFrom(const std::vector<int>& indexes, int level, uint8_t type=SelectAsValue);
    virtual void selectTo(const std::vector<int>& indexes, int level, uint8_t type=SelectAsValue);
    virtual void selectRange(const std::vector<int>& start,
                             const std::vector<int>& end,
                             int level,
                             uint8_t type=SelectAsValue);
    virtual void clear();

    virtual void stretchSize();
    virtual void calculateSize();
    virtual void markSelectStart();


    friend class LEditContainer;
    friend class LIEditor;
    friend class LEditOp;
    friend class LEditDoc;
};

typedef std::function<std::shared_ptr<LEditBase>()> LEditBaseCreator;

template<class T>
LEditBaseCreator makeEditCreator(){
    LEditBaseCreator fn =
        []() {
            std::shared_ptr<T> t = std::make_shared<T>();
            return t;
        };
    return fn;
}


LK_NAMESPACE_END

#endif // LEDITBASE_H
