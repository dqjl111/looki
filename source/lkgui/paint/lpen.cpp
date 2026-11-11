#include "lpen.h"
#include "lkgui/paint/lstroker.h"

LK_NAMESPACE_BEGIN

LPen::LPen(LPen* p, bool brushNew)
{
    if(p != nullptr){
        m_width = p->m_width;
        if(brushNew){
            if(p->m_brush!= nullptr){
                m_brush = std::make_shared<LBrush>(p->m_brush.get());
            }
        }else{
            m_brush = p->m_brush;
        }

        m_style = p->m_style;
        m_capStyle = p->m_capStyle;
        m_joinStyle = p->m_joinStyle;
        m_dashPattern = p->m_dashPattern;
        m_dashOffset = p->m_dashOffset;
        m_miterLimit = p->m_miterLimit;
        m_cosmetic = p->m_cosmetic;
        m_defaultWidth = p->m_defaultWidth; // default-constructed width? used for cosmetic pen compatibility
    }

    if(m_brush == nullptr)
    {
        m_brush = std::make_shared<LBrush>();
    }
}

LPen::LPen(const LColor &color)
{
    m_brush = std::make_shared<LBrush>(color);
}

LPen::LPen(const LColor &color, double width, LPenStyle s, LPenCapStyle c, LPenJoinStyle j)
    :m_width(width),m_style(s), m_capStyle(c), m_joinStyle(j)
{
    m_brush = std::make_shared<LBrush>(color);
}


LPen::LPen(const std::shared_ptr<LBrush> brush, double width, LPenStyle s, LPenCapStyle c, LPenJoinStyle j)
    :m_width(width),m_style(s), m_capStyle(c), m_joinStyle(j)
{
    if(brush == nullptr){
        m_brush = std::make_shared<LBrush>();
    }else{
        m_brush = brush;
    }
}

std::shared_ptr<LPen> LPen::backPen()
{
    std::shared_ptr<LPen> ret = std::make_shared<LPen>(this, true);
    return ret;
}

void LPen::restorePen(LPen *p)
{
    m_width = p->m_width;
    m_brush = p->m_brush;
    m_style = p->m_style;
    m_capStyle = p->m_capStyle;
    m_joinStyle = p->m_joinStyle;
    m_dashPattern = p->m_dashPattern;
    m_dashOffset = p->m_dashOffset;
    m_miterLimit = p->m_miterLimit;
    m_cosmetic = p->m_cosmetic;
    m_defaultWidth = p->m_defaultWidth;
}

bool LPen::operator==(const LPen &p) const
{
    if(this == &p){
        return true;
    }

    return  (p.m_style == m_style
            && p.m_capStyle == m_capStyle
            && p.m_joinStyle == m_joinStyle
            && p.m_width == m_width
            && p.m_miterLimit == m_miterLimit
            && (m_style != LPenStyle::CustomDash
                || (lFuzzyCompare(p.m_dashOffset, m_dashOffset) &&
                    p.m_dashPattern == m_dashPattern))
            && *p.m_brush == *m_brush
            && p.m_cosmetic == m_cosmetic
            && p.m_defaultWidth == m_defaultWidth);
}

LColor LPen::color() const
{
    return m_brush->m_color;
}

void LPen::setColor(const LColor &color)
{
    m_brush->m_color = color;
}

void LPen::resetStyle(LPenStyle s)
{
    m_style = s;
    m_dashPattern.clear();
    m_dashOffset = 0;
}

bool LPen::isCosmetic() const
{
    return (m_cosmetic == true) || m_width == 0;
}

std::vector<double> LPen::dashPattern() const
{
    if(m_style == LPenStyle::CustomDash){
        return m_dashPattern;
    }
    return LDashStroker::patternForStyle(m_style);
}

void LPen::setDashPattern(const std::vector<double> &pattern)
{
    m_dashPattern = pattern;
    m_style = LPenStyle::CustomDash;

    if ((m_dashPattern.size() % 2) == 1) {
        lWarn() <<"<LPen::setDashPattern: Pattern not of even length";
        m_dashPattern.push_back(1);
    }
}

int LPen::dashPatternSize() const
{
    switch (m_style) {
    case LPenStyle::Dash:
    case LPenStyle::Dot:{
        return 2;
    }
    case LPenStyle::DashDot:{
        return 4;
    }

    case LPenStyle::DashDotDot:{
        return 6;
    }
    case LPenStyle::CustomDash:{
        return m_dashPattern.size();
    }
    default:
        return 0;
    }
}

void LPen::setBrush(std::shared_ptr<LBrush> b)
{
    if(b != nullptr){
        m_brush = b;
    }else{
        m_brush->m_style = LBrush::NoPattern;
    }
}



LK_NAMESPACE_END
