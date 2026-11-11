#ifndef LPEN_H
#define LPEN_H
#include "lkgui/paint/lbrush.h"

LK_NAMESPACE_BEGIN
class LPainter;
class LKGUIDEF LPen : public LIVar
{
public:
    LPen(LPen* p= nullptr, bool brushNew = false);
    LPen(const LColor &color);
    LPen(const LColor &color,
         double width,
         LPenStyle s = LPenStyle::Solid,
         LPenCapStyle c = LPenCapStyle::Square,
         LPenJoinStyle j = LPenJoinStyle::Bevel);
    LPen(const std::shared_ptr<LBrush> brush,
         double width,
         LPenStyle s = LPenStyle::Solid,
         LPenCapStyle c = LPenCapStyle::Square,
         LPenJoinStyle j = LPenJoinStyle::Bevel);

    std::shared_ptr<LPen> backPen();
    void restorePen(LPen* pen);

    bool operator==(const LPen &p) const;
    inline bool operator!=(const LPen &p) const { return !(operator==(p)); }

    LColor color() const;
    void setColor(const LColor &color);
    void resetStyle(LPenStyle s);
    bool isCosmetic() const;
    std::vector<double> dashPattern() const;
    void setDashPattern(const std::vector<double> &pattern);
    int dashPatternSize() const;

    inline const std::vector<double>& customDashPattern() const{
        return m_dashPattern;
    }

    inline const std::shared_ptr<LBrush>& brush() const{
        return m_brush;
    }

    void setBrush(std::shared_ptr<LBrush> b);

    double m_width = 1;

    LPenStyle m_style = LPenStyle::Solid;
    LPenCapStyle m_capStyle = LPenCapStyle::Square;
    LPenJoinStyle m_joinStyle = LPenJoinStyle::Bevel;

    double m_dashOffset = 0;
    double m_miterLimit = 2;
    uint32_t m_cosmetic : 1;
    uint32_t m_defaultWidth : 1; // default-constructed width? used for cosmetic pen compatibility

protected:
    mutable std::vector<double> m_dashPattern;
    std::shared_ptr<LBrush> m_brush = nullptr;
    friend class LPainter;
};

LK_NAMESPACE_END

#endif // LPEN_H
