#ifndef LSLIDER_H
#define LSLIDER_H
#include "lkwidgets/widget/labstractslider.h"

LK_NAMESPACE_BEGIN
class LKWIDGETSDEF LSlider : public LAbstractSlider
{
public:
    _LIVarTagEx(LSlider, LWidget)

    LSlider(std::shared_ptr<LWidget> parent = nullptr,
            LOrientation orient = LOrientation::Horizontal);

    enum TickPosition : uint8_t{
        NoTicks = 0,
        TicksAbove = 1,
        TicksLeft = TicksAbove,
        TicksBelow = 2,
        TicksRight = TicksBelow,
        TicksBothSides = 3
    };

    LSize<int> sizeHint() const override;
    LSize<int> minimumSizeHint() const override;

    void setTickPosition(TickPosition position);
    inline TickPosition tickPosition() const{
        return m_tickPosition;
    }

    void setTickInterval(uint32_t ti);
    inline uint32_t tickInterval() const{
        return m_tickInterval;
    }
protected:
    virtual void paintEvent(std::shared_ptr<LPaintEvent> ev) override;
    virtual void mousePressEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseReleaseEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseMoveEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void leaveEvent(std::shared_ptr<LEvent> ev) override;
    virtual bool enterSubItem(const LPoint<int>& p) override;

    virtual void initSpecific() override;
    inline int pick(const LPoint<int> &pt) const{
        return m_orientation == LOrientation::Horizontal ? pt.m_x : pt.m_y;
    }
    int pixelPosToRangeValue(int pos) const;
    LUiSubCtrl newHoverControl(const LPoint<int> &pos);

    LUiSubCtrl m_pressedCtrl;
    LUiSubCtrl m_hoverCtrl;
    LRect<int> m_hoverRect;
    int m_clickOffset = 0;
    uint32_t m_tickInterval = 0;
    LSlider::TickPosition m_tickPosition = NoTicks;
};

LK_NAMESPACE_END

#endif // LSLIDER_H
