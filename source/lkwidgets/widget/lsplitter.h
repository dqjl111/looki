#ifndef LSPLITTER_H
#define LSPLITTER_H

#include "lkwidgets/widget/lframe.h"

LK_NAMESPACE_BEGIN

class LSplitterHandle;
class LSplitter;
class LKWIDGETSDEF LSplitterLayoutItem
{
public:
    LSplitterLayoutItem(std::shared_ptr<LWidget> w = nullptr);
    int getWidgetSize(LOrientation orient) const;
    int getHandleSize(LOrientation orient) const;
    int pick(const LSize<int> &size, LOrientation orient) const
    { return (orient == LOrientation::Horizontal) ? size.width() : size.height(); }

protected:
    LRect<int> m_rect;
    int m_sizer = -1;

    std::shared_ptr<LWidget> m_widget;
    std::shared_ptr<LSplitterHandle> m_handle;
    union{
        uint8_t m_flags = 0;
        struct{
            uint8_t m_collapsed : 1;
            uint8_t m_collapsible : 2;
        };
    };

    friend class LSplitter;
    friend class LSplitterHandle;
};


class LKWIDGETSDEF LSplitterHandle : public LWidget
{
public:
    _LIVarTagEx(LSplitterHandle, LWidget)
    explicit LSplitterHandle(std::shared_ptr<LSplitter> parent = nullptr);

    void setOrientation(LOrientation o);
    LOrientation orientation() const;
    bool opaqueResize() const;
    std::shared_ptr<LSplitter> splitter() const;
    virtual LSize<int> sizeHint() const override;
    virtual bool handleEvent(std::shared_ptr<LEvent> e) override;
protected:
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
    virtual void mouseMoveEvent(std::shared_ptr<LMouseEvent> e) override;
    virtual void mousePressEvent(std::shared_ptr<LMouseEvent> e) override;
    virtual void mouseReleaseEvent(std::shared_ptr<LMouseEvent> e) override;
    virtual void resizeEvent(std::shared_ptr<LResizeEvent> e) override;
    virtual void initSpecific() override;

    void moveSplitter(int p);
    int closestLegalPosition(int p);
    inline int pick(const LPoint<int> &pos) const
    { return m_orient == LOrientation::Horizontal ? pos.m_x : pos.m_y; }

    int m_mouseOffset;
    union{
        uint8_t m_splitterHandleFlags = 0;
        struct{
            LOrientation m_orient : 2;
            uint8_t m_hover : 1;
            uint8_t m_pressed : 1;
        };
    };
};



class LKWIDGETSDEF LSplitter : public LFrame
{
public:
     _LIVarTagEx(LSplitter, LWidget)

    explicit LSplitter(LOrientation o = LOrientation::Horizontal, std::shared_ptr<LWidget> parent = nullptr);

    void setOrientation(LOrientation o);
    LOrientation orientation() const;

    void setCollapsible(int index, bool);
    bool isCollapsible(int index) const;
    void setOpaqueResize(bool opaque = true);
    bool opaqueResize() const;
    int handleWidth() const;
    void setHandleWidth(int);

    virtual LSize<int> sizeHint() const override;
    virtual LSize<int> minimumSizeHint() const override;

    std::list<int> sizes() const;
    void setSizes(const std::list<int> &ss);

    int removeWidget(std::shared_ptr<LWidget> child, bool update = true);
    std::shared_ptr<LWidget> removeWidget(uint32_t pos, bool update = true);
    int addWidget(std::shared_ptr<LWidget> child, int pos = -1, bool update = true);
    void clearInvalidItem(bool update = true);
    int indexOfItem(std::shared_ptr<LWidget> item) const;
    virtual bool handleEvent(std::shared_ptr<LEvent> e) override;
protected:
    virtual void resizeEvent(std::shared_ptr<LResizeEvent> e) override;

    void recalc(bool update = false);
    inline int pick(const LPoint<int> &pos) const
    { return m_orient == LOrientation::Horizontal ? pos.m_x : pos.m_y; }
    inline int pick(const LSize<int> &s) const
    { return m_orient == LOrientation::Horizontal ? s.width() : s.height(); }

    inline int trans(const LPoint<int> &pos) const
    { return m_orient == LOrientation::Vertical ? pos.m_x : pos.m_y; }
    inline int trans(const LSize<int> &s) const
    { return m_orient == LOrientation::Vertical ? s.width() : s.height(); }
    void doResize();
    void setGeo(LSplitterLayoutItem& item, int pos, int size, bool allowCollapse);
    void setSizesHelper(const std::list<int> &sizes, bool clampNegativeSize = false);

    bool collapsible(const LSplitterLayoutItem& item) const;
    bool collapsible(int index) const
    { return (index < 0 || index >= m_items.size()) ? true : collapsible(m_items.at(index)); }

    int addWidgetHelper(std::shared_ptr<LWidget> w, int pos);
    void moveSplitter(int pos, int index);
    int closestLegalPosition(int, int);
    virtual void initSpecific() override;


    std::vector<LSplitterLayoutItem> m_items;

    int m_handleWidth =3;
    union{
        uint8_t m_splitterFlags = 0;
        struct{
            LOrientation m_orient : 2;
            uint8_t m_opaque : 1;
            uint8_t m_firstShow : 1;
            uint8_t m_childrenCollapsible : 1;
            uint8_t m_compatMode : 1;
            uint8_t m_blockChildAdd : 1;
            uint8_t m_opaqueResizeSet : 1;
        };
    };
    friend class LSplitterHandle;

private:
    void getRange(int index, int *, int *, int *, int *) const;
    int adjustPos(int, int, int *, int *, int *, int *) const;
    void addContribution(int, int *, int *, bool) const;
    int findWidgetJustBeforeOrJustAfter(int index, int delta, int &collapsibleSize) const;
    void doMove(bool backwards, int pos, int index, int delta,
                bool mayCollapse, int *positions, int *widths);
};
LK_NAMESPACE_END

#endif // LSPLITTER_H
