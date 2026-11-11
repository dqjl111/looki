#ifndef LRUBBERBAND_H
#define LRUBBERBAND_H

#include "lkwidgets/core/lwidget.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LRubberBand : public LWidget
{
public:
    enum Shape { Line, Rectangle };
    LRubberBand(Shape s, std::shared_ptr<LWidget> parent = nullptr);

    Shape shape() const;
protected:
    void paintEvent(std::shared_ptr<LPaintEvent> e) override;
    void changeEvent(std::shared_ptr<LEvent> e) override;
    void showEvent(std::shared_ptr<LEvent> e) override;
    void resizeEvent(std::shared_ptr<LResizeEvent> e) override;
    void moveEvent(std::shared_ptr<LMoveEvent> e) override;
    virtual void initSpecific() override;

    void updateMask();

    LRect<int> m_rect;
    Shape m_shape;
    std::shared_ptr<LRegion> m_clipping;
};


LK_NAMESPACE_END

#endif // LRUBBERBAND_H
