#include "lsvgfont.h"
#include "lkcore/text/utf8iterator.h"

LK_NAMESPACE_BEGIN

LSvgGlyph::LSvgGlyph(uint32_t unicode, const std::shared_ptr<LPainterPath> path, double horizAdvX)
    : m_unicode(unicode), m_path(path), m_horizAdvX(horizAdvX)
{

}


LSvgFont::LSvgFont(double horizAdvX)
    : m_horizAdvX(horizAdvX)
{
}

void LSvgFont::addGlyph(uint32_t unicode, const std::shared_ptr<LPainterPath> path, double horizAdvX )
{
    if(path != nullptr){
        m_glyphs[unicode] =  LSvgGlyph(unicode, path, (horizAdvX==-1)?m_horizAdvX:horizAdvX);
    }
}


void LSvgFont::draw(LPainter *p, const LPoint<double> &point, const std::string &str,
                    double pixelSize, LAlignment alignment) const
{
    drawHelper(p, point, str, pixelSize, alignment, nullptr);
}

LRect<double> LSvgFont::boundingRect(LPainter *p, const LPoint<double> &point, const std::string &str,
                              double pixelSize, LAlignment alignment) const
{
    LRect<double> bounds;
    drawHelper(p, point, str, pixelSize, alignment, &bounds);
    return bounds;
}

void LSvgFont::drawHelper(LPainter *p, const LPoint<double> &point, const std::string &str, double pixelSize,
                           LAlignment alignment, LRect<double> *boundingRect) const
{
    const bool isPainting = (boundingRect == nullptr);

    p->save();
    p->translate(point);
    p->scale(pixelSize / m_unitsPerEm, -pixelSize / m_unitsPerEm);

    int textWidth = 0;

    LUtf8Iterator iter(str);
    bool hasNext = iter.next();

    std::unordered_map<uint32_t, LSvgGlyph>::const_iterator svgIter;
    std::unordered_map<uint32_t, LSvgGlyph>::const_iterator svgZeroIter = m_glyphs.find(0);

    while (hasNext) {
        uint32_t unicode = iter.unicode();
        svgIter = m_glyphs.find(unicode);
        if (svgIter == m_glyphs.cend()) {
            if (svgZeroIter == m_glyphs.cend()) {
                hasNext = iter.next();
                continue;
            }
            svgIter = svgZeroIter;
        }
        textWidth += static_cast<int>(svgIter->second.m_horizAdvX);
        hasNext = iter.next();
    }

    LPoint<double> alignmentOffset(0, 0);
    if (alignment == LAlignment::HCenter) {
        alignmentOffset.m_x = -textWidth / 2;
    } else if (alignment == LAlignment::Right) {
        alignmentOffset.m_x = -textWidth;
    }

    p->translate(alignmentOffset);

    // since in SVG the embedded font ain't really a path
    // the outline has got to stay untransformed...

    std::shared_ptr<LPen> pen = p->pen()->backPen();
    double penWidth = pen->m_width;
    penWidth /= (pixelSize/m_unitsPerEm);
    pen->m_width = penWidth;
    p->setPen(pen);

    iter.init(str);
    hasNext = iter.next();
    while (hasNext) {
        uint32_t unicode = iter.unicode();
        svgIter = m_glyphs.find(unicode);
        if (svgIter == m_glyphs.cend()) {
            if (svgZeroIter == m_glyphs.cend()) {
                hasNext = iter.next();
                continue;
            }
            svgIter = svgZeroIter;
        }

        if (isPainting){
            p->drawPath(svgIter->second.m_path);
        }

        if (boundingRect != nullptr) {
            LPainterPathStroker stroker;
            stroker.setWidth(penWidth);
            stroker.setJoinStyle(pen->m_joinStyle);
            stroker.setMiterLimit(pen->m_miterLimit);
            std::shared_ptr<LPainterPath> stroke = stroker.createStroke(svgIter->second.m_path);
            *boundingRect |= p->transform().map(stroke)->boundingRect();
        }

        p->translate(LPoint<double>(svgIter->second.m_horizAdvX, 0));
        hasNext = iter.next();
    }

    p->restore();
}

void LSvgFont::setFamilyName(const std::string &name)
{
    m_familyName = name;
}

void LSvgFont::setUnitsPerEm(double upem)
{
    m_unitsPerEm = upem;
}



LK_NAMESPACE_END
