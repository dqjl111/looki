#include "lrubberband.h"

LK_NAMESPACE_BEGIN

LRubberBand::LRubberBand(Shape s, std::shared_ptr<LWidget> parent): LWidget(parent) {
    m_shape = s;
}


void LRubberBand::initSpecific()
{
    setAttribute(LWidgetAttribute::TransparentForMouseEvents);
    setAttribute(LWidgetAttribute::NoSystemBackground);
    setAttribute(LWidgetAttribute::WStateExplicitShowHide);
    setVisible(false);
}


void LRubberBand::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    LPainter painter(paintDevice());
    std::shared_ptr<LPalette> pal = palette();
    LColor highlight = pal->color(LPalette::Active, LPalette::Highlight);
    painter.setPen(highlight.darker(120));
    LColor dimHighlight(std::min<int>(highlight.m_red/2 + 110, 255),
                        std::min<int>(highlight.m_green/2 + 110, 255),
                        std::min<int>(highlight.m_blue/2 + 110, 255),
                        isWindow()? 255 : 127);
    painter.setBrush(dimHighlight);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void LRubberBand::changeEvent(std::shared_ptr<LEvent> e)
{
    LWidget::changeEvent(e);
    switch (e->type()) {
    case LEvent::ParentChange:
        if (parent()) {
            setWindowFlags(windowFlags() & ~(uint32_t)LWindowType::ToolTip);
        } else {
            setWindowFlags(windowFlags() | LWindowType::ToolTip);
        }
        break;
    default:
        break;
    }

    if (e->type() == LEvent::ZOrderChange){
        raise();
    }
}

void LRubberBand::showEvent(std::shared_ptr<LEvent> e)
{
    raise();
    e->ignore();
}

void LRubberBand::resizeEvent(std::shared_ptr<LResizeEvent> e)
{
    updateMask();
}

void LRubberBand::moveEvent(std::shared_ptr<LMoveEvent> e)
{
    updateMask();
}

void LRubberBand::updateMask()
{
    if (m_shape == Rectangle) {
        LRect<int> tRect = rect();
        std::shared_ptr<LRegion> r = std::make_shared<LRegion>(tRect);
        LMargins<int> margin = boundWidths();
        tRect.adjust(margin.m_left, margin.m_top, -margin.m_right, -margin.m_bottom);
        r->subtract(tRect);
        setMask(r);
    }else{
        clearMask();
    }
}

LRubberBand::Shape LRubberBand::shape() const
{
    return m_shape;
}


LK_NAMESPACE_END
