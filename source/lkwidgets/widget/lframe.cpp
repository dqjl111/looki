#include "lframe.h"
#include "lkwidgets/draw/lstyle.h"

LK_NAMESPACE_BEGIN
LFrameData::LFrameData()
{

}

LFrameData::LFrameData(uint8_t shape, uint8_t shadow, uint8_t lineWidth, uint8_t midLineWidth)
    :m_shape(shape), m_shadow(shadow), m_lineWidth(lineWidth), m_midLineWidth(midLineWidth)
{

}

LFrameData::Shape LFrameData::frameShape() const
{
    return Shape(m_shape);
}



void LFrameData::setFrameShape(Shape s)
{
    m_shape = s;
}

LFrameData::Shadow LFrameData::frameShadow() const
{
    return Shadow(m_shadow);
}

void LFrameData::setFrameShadow(Shadow s)
{
    m_shadow = s;
}

uint8_t LFrameData::lineWidth() const
{
    return m_lineWidth;
}

void LFrameData::setLineWidth(uint8_t w)
{
    if (uint8_t(w) == m_lineWidth){
        return;
    }
    m_lineWidth = w;
}

uint8_t LFrameData::midLineWidth() const
{
    return m_midLineWidth;
}

void LFrameData::setMidLineWidth(uint8_t w)
{
    if (uint8_t(w) == m_midLineWidth){
        return;
    }
    m_midLineWidth = w;
}

uint8_t LFrameData::frameWidth() const
{
    switch (m_shape) {
    case LFrameData::NoFrame:{
        return 0;
    }

    case LFrameData::Box:
    case LFrameData::HLine:
    case LFrameData::VLine:{
        switch (m_shadow) {
        case LFrameData::Plain:{
            return m_lineWidth;
        }
        case LFrameData::Raised:
        case LFrameData::Sunken:{
            return m_lineWidth*2 + m_midLineWidth;
        }
        default:{
            break;
        }
        }
        break;
    }
    case LFrameData::WinPanel:{
        return 2;
    }
    case LFrameData::Panel:
    case LFrameData::StyledPanel:{
        switch (m_shadow) {
        case LFrameData::Plain:
        case LFrameData::Raised:
        case LFrameData::Sunken:{
            return m_lineWidth;
        }
        default:{
            break;
        }
        }
        break;
    }
    }
    return 0;
}

LFrame::LFrame(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f) : LWidget(parent, f)
{
    m_sizePolicy.setControlType(LSizePolicy::Frame);
}

void LFrame::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    LPainter painter(paintDevice());
    std::shared_ptr<LStyle> pStyle = style();
    std::shared_ptr<LWidget> f = getPtrT<LWidget>();
    pStyle->drawFrame(f, &painter, *this);
}

LK_NAMESPACE_END
