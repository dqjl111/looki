#ifndef LSVGFONT_H
#define LSVGFONT_H

#include "lkgui/paint/lpainterpath.h"
#include "lkgui/paint/lpainter.h"
#include <unordered_map>

LK_NAMESPACE_BEGIN

class LKSVGDEF LSvgGlyph
{
public:
    LSvgGlyph(uint32_t unicode, const std::shared_ptr<LPainterPath>  path, double horizAdvX);
    LSvgGlyph() : m_unicode(0), m_horizAdvX(0) {}

    uint32_t m_unicode;
    std::shared_ptr<LPainterPath> m_path;
    double m_horizAdvX;
};


class LKSVGDEF LSvgFont : public LIVar
{
public:
    static constexpr double DEFAULT_UNITS_PER_EM = 1000;
    LSvgFont(double horizAdvX);

    void setFamilyName(const std::string &name);
    inline const std::string& familyName() const{
        return m_familyName;
    }

    void setUnitsPerEm(double upem);
    void addGlyph(uint32_t unicode, const std::shared_ptr<LPainterPath> path, double horizAdvX = -1);

    void draw(LPainter *p,
              const LPoint<double> &point,
              const std::string &str,
              double pixelSize,
              LAlignment alignment) const;
    LRect<double> boundingRect(LPainter *p,
                               const LPoint<double> &point,
                               const std::string &str,
                               double pixelSize,
                               LAlignment alignment) const;

public:
    std::string m_familyName;
    double m_unitsPerEm = DEFAULT_UNITS_PER_EM;
    double m_horizAdvX;
    std::unordered_map<uint32_t, LSvgGlyph> m_glyphs;

private:
    void drawHelper(LPainter *p,
                    const LPoint<double> &point,
                    const std::string &str,
                    double pixelSize,
                    LAlignment alignment,
                    LRect<double>* boundingRect = nullptr) const;
};


LK_NAMESPACE_END

#endif // LSVGFONT_H
