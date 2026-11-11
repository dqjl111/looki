#ifndef LSCROLLBAR_H
#define LSCROLLBAR_H

#include "lkwidgets/widget/labstractslider.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LScrollBar : public LAbstractSlider
{
public:
    _LIVarTagEx(LScrollBar, LWidget)

    LScrollBar(std::shared_ptr<LWidget> parent = nullptr,
               LOrientation orient = LOrientation::Horizontal);

    LSize<int> sizeHint() const override;
    virtual void blink() override;
protected:
    virtual void paintEvent(std::shared_ptr<LPaintEvent> ev) override;
    virtual void mousePressEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseReleaseEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseMoveEvent(std::shared_ptr<LMouseEvent> ev) override;
    void activateControl(LUiSubCtrl control, int threshold= 500);

    virtual void initSpecific() override;
    void stopRepeatAction();
    int pixelPosToRangeValue(int pos) const;

    LUiSubCtrl m_pressedCtrl;
    LUiSubCtrl m_hoverCtrl;
    LRect<int> m_hoverRect;
    bool m_pointerOutsidePressedCtrl = true;
    int m_clickOffset= 0;
};

LK_NAMESPACE_END

#endif // LSCROLLBAR_H
