#include "lsvggraphics.h"
#include "lkgui/text/lfontmetrics.h"
#include "lsvgstructure.h"
#include "lkgui/text/ltextlayout.h"
#include "lkgui/text/ltextengine.h"
#include "lsvgfont.h"
LK_NAMESPACE_BEGIN

#ifndef LK_SVG_MAX_LAYOUT_SIZE
#define LK_SVG_MAX_LAYOUT_SIZE (int64_t(INT_MAX/512))
#endif

void LSvgDummyNode::drawCommand(LPainter *, LSvgExtraStates &)
{
    lWarn() << "Dummy node not meant to be drawn";
}

LSvgEllipse::LSvgEllipse(std::shared_ptr<LSvgNode> parent, const LRect<double> &rect)
    : LSvgNode(parent), m_bounds(rect)
{
}

LRect<double> LSvgEllipse::internalFastBounds(LPainter *p, LSvgExtraStates &) const
{
    return p->transform().mapRect(m_bounds);
}

LRect<double> LSvgEllipse::internalBounds(LPainter *p, LSvgExtraStates &) const
{
    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    path->addEllipse(m_bounds);
    double sw = strokeWidth(p);
    if(lFuzzyIsZero(sw)){
        path = p->transform().map(path);
        return path->boundingRect();
    }else{
        return boundsOnStroke(p, path, sw, BoundsMode::Simplistic);
    }
}

LRect<double> LSvgEllipse::decoratedInternalBounds(LPainter *p, LSvgExtraStates &) const
{
    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    path->addEllipse(m_bounds);
    double sw = strokeWidth(p);

    if(lFuzzyIsZero(sw)){
        path = p->transform().map(path);
        return filterRegion(path->boundingRect());
    }else{
        return filterRegion(boundsOnStroke(p, path, sw, BoundsMode::IncludeMiterLimit));
    }
}

void LSvgEllipse::drawCommand(LPainter *p, LSvgExtraStates &)
{
    p->drawEllipse(m_bounds);
}

bool LSvgEllipse::separateFillStroke(const LSvgExtraStates &s) const
{
    return true;
}

LSvgImage::LSvgImage(std::shared_ptr<LSvgNode> parent,
                     const std::shared_ptr<LImage> image,
                     const std::string &filename,
                     const LRect<double> &bounds)
    : LSvgNode(parent)
    , m_filename(filename)
    , m_image(image)
    , m_bounds(bounds)
{
    if(m_image != nullptr){
        if (m_bounds.width() == 0.0){
            m_bounds.setWidth(static_cast<double>(m_image->width()));
        }
        if (m_bounds.height() == 0.0){
            m_bounds.setHeight(static_cast<double>(m_image->height()));
        }
    }
}

void LSvgImage::drawCommand(LPainter *p, LSvgExtraStates &)
{
    p->drawImage(m_bounds, m_image, LRect<double>(0, 0, m_image->width(), m_image->height(),false));
}

LSvgLine::LSvgLine(std::shared_ptr<LSvgNode> parent, const LLine<double> &line)
    : LSvgNode(parent), m_line(line)
{
}

void LSvgLine::drawCommand(LPainter *p, LSvgExtraStates &states)
{
    std::shared_ptr<LPen> pen = p->pen();
    if (pen->m_width != 0) {
        double oldOpacity = p->opacity();
        p->setOpacity(oldOpacity * states.strokeOpacity);
        if (m_line.isNull() && pen->m_capStyle != LPenCapStyle::Flat){
            p->drawPoint(m_line.m_pt1);
        }else{
            p->drawLine(m_line);
        }
        p->setOpacity(oldOpacity);
    }
    std::shared_ptr<LSvgNode> tThis = getPtrT<LSvgNode>();
    LSvgMarker::drawMarkersForNode(tThis, p, states);
}

LSvgPath::LSvgPath(std::shared_ptr<LSvgNode> parent, const std::shared_ptr<LPainterPath> qpath)
    : LSvgNode(parent), m_path(qpath)
{
}

void LSvgPath::drawCommand(LPainter *p, LSvgExtraStates &states)
{
    m_path->setFillRule(states.fillRule);
    if (m_path->boundingRect().isNull() && p->pen()->m_capStyle != LPenCapStyle::Flat){
        p->drawPoint(m_path->boundingRect().topLeft());
    }else{
        p->drawPath(m_path);
    }
    std::shared_ptr<LSvgNode> tThis = getPtrT<LSvgNode>();
    LSvgMarker::drawMarkersForNode(tThis, p, states);
}

bool LSvgPath::separateFillStroke(const LSvgExtraStates &s) const
{
    return !lFuzzyCompare(s.fillOpacity, s.strokeOpacity);
}

LRect<double> LSvgPath::internalFastBounds(LPainter *p, LSvgExtraStates &) const
{
    return p->transform().mapRect(m_path->controlPointRect());
}

LRect<double> LSvgPath::internalBounds(LPainter *p, LSvgExtraStates &) const
{
    double sw = strokeWidth(p);
    if(lFuzzyIsZero(sw)){
        std::shared_ptr<LPainterPath> path = p->transform().map(m_path);
        return filterRegion(path->boundingRect());
    }else{
        return filterRegion(boundsOnStroke(p, m_path, sw, BoundsMode::Simplistic));
    }
}

LRect<double> LSvgPath::decoratedInternalBounds(LPainter *p, LSvgExtraStates &s) const
{
    double sw = strokeWidth(p);
    std::shared_ptr<LSvgNode> tThis = getPtrT<LSvgNode>();
    if(lFuzzyIsZero(sw)){
        std::shared_ptr<LPainterPath> path = p->transform().map(m_path);
        LRect<double> rect = path->boundingRect();
        rect |= LSvgMarker::markersBoundsForNode(tThis, p, s);
        return filterRegion(rect);
    }else{
        LRect<double> rect = boundsOnStroke(p, m_path, sw, BoundsMode::IncludeMiterLimit);
        rect |= LSvgMarker::markersBoundsForNode(tThis, p, s);
        return filterRegion(rect);
    }
}

bool LSvgPath::requiresGroupRendering() const
{
    return hasAnyMarker();
}

LSvgPolygon::LSvgPolygon(std::shared_ptr<LSvgNode> parent, const LPolygon<double> &poly)
    : LSvgNode(parent), m_poly(poly)
{
}

LRect<double> LSvgPolygon::internalFastBounds(LPainter *p, LSvgExtraStates &) const
{
    return p->transform().mapRect(m_poly.boundingRect());
}

LRect<double> LSvgPolygon::internalBounds(LPainter *p, LSvgExtraStates &s) const
{
    return internalBounds(p, s, BoundsMode::Simplistic);
}

LRect<double> LSvgPolygon::decoratedInternalBounds(LPainter *p, LSvgExtraStates &s) const
{
    LRect<double> rect = internalBounds(p, s, BoundsMode::IncludeMiterLimit);
    std::shared_ptr<LSvgNode> tThis = getPtrT<LSvgNode>();
    rect |= LSvgMarker::markersBoundsForNode(tThis, p, s);
    return filterRegion(rect);
}

bool LSvgPolygon::requiresGroupRendering() const
{
    return hasAnyMarker();
}

LRect<double> LSvgPolygon::internalBounds(LPainter *p, LSvgExtraStates &, BoundsMode mode) const
{
    double sw = strokeWidth(p);
    if (lFuzzyIsZero(sw)) {
        return p->transform().map(m_poly).boundingRect();
    } else {
        std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
        path->addPolygon(m_poly);
        return boundsOnStroke(p, path, sw, mode);
    }
}

void LSvgPolygon::drawCommand(LPainter *p, LSvgExtraStates &states)
{
    if (m_poly.boundingRect().isNull() && p->pen()->m_capStyle != LPenCapStyle::Flat){
        p->drawPoint(m_poly.front());
    }else{
        p->drawPolygon(m_poly, states.fillRule);
    }
    std::shared_ptr<LSvgNode> tThis =getPtrT<LSvgNode>();
    LSvgMarker::drawMarkersForNode(tThis, p, states);
}

bool LSvgPolygon::separateFillStroke(const LSvgExtraStates &s) const
{
    return true;
}

LSvgPolyline::LSvgPolyline(std::shared_ptr<LSvgNode> parent, const LPolygon<double> &poly)
    : LSvgNode(parent), m_poly(poly)
{

}

void LSvgPolyline::drawCommand(LPainter *p, LSvgExtraStates &states)
{
    if (p->brush()->m_style != LBrush::NoPattern) {
        p->drawPolygon(m_poly, states.fillRule);
    } else {
        if (m_poly.boundingRect().isNull() && p->pen()->m_capStyle != LPenCapStyle::Flat){
            p->drawPoint(m_poly.front());
        }else{
            p->drawPolyline(m_poly);
        }
        std::shared_ptr<LSvgNode> tThis =getPtrT<LSvgNode>();
        LSvgMarker::drawMarkersForNode(tThis, p, states);
    }
}

bool LSvgPolyline::separateFillStroke(const LSvgExtraStates &s) const
{
    return true;
}

LSvgRect::LSvgRect(std::shared_ptr<LSvgNode> node, const LRect<double> &rect, double rx, double ry)
    : LSvgNode(node),
    m_rect(rect), m_rx(rx), m_ry(ry)
{
}

LRect<double> LSvgRect::internalFastBounds(LPainter *p, LSvgExtraStates &) const
{
    return p->transform().mapRect(m_rect);
}

LRect<double> LSvgRect::internalBounds(LPainter *p, LSvgExtraStates &s) const
{
    return internalBounds(p, s, BoundsMode::Simplistic);
}

LRect<double> LSvgRect::decoratedInternalBounds(LPainter *p, LSvgExtraStates &s) const
{
    return filterRegion(internalBounds(p, s, BoundsMode::IncludeMiterLimit));
}

LRect<double> LSvgRect::internalBounds(LPainter *p, LSvgExtraStates &, BoundsMode mode) const
{
    double sw = strokeWidth(p);
    if (lFuzzyIsZero(sw)) {
        return p->transform().mapRect(m_rect);
    } else {
        std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
        path->addRect(m_rect);
        return boundsOnStroke(p, path, sw, mode);
    }
}

void LSvgRect::drawCommand(LPainter *p, LSvgExtraStates &)
{
    if (m_rx || m_ry){
        // SVG 的 rx/ry 是像素半径，应使用绝对尺寸模式
        p->drawRoundedRect(m_rect, m_rx, m_ry, LSizeMode::Absolute);
    }else{
        p->drawRect(m_rect);
    }
}

bool LSvgRect::separateFillStroke(const LSvgExtraStates &s) const
{
    return true;
}

std::shared_ptr<LSvgTspan> LSvgText::LINEBREAK = nullptr;

LSvgText::LSvgText(std::shared_ptr<LSvgNode> parent, const LPoint<double> &coord)
    : LSvgNode(parent)
    , m_coord(coord)
    , m_type(Text)
    , m_size(0, 0)
    , m_mode(Default)
{
}

void LSvgText::setTextArea(const LSize<double> &size)
{
    m_size = size;
    m_type = Textarea;
}

LRect<double> LSvgText::internalFastBounds(LPainter *p, LSvgExtraStates &) const
{
    std::shared_ptr<LFont> font = m_style.font ? m_style.font->font() : p->font();
    LFontMetrics fm(font);

    int charCount = 0;
    for (const std::shared_ptr<LSvgTspan>& span : m_tspans) {
        if (span != LINEBREAK){
            charCount += span->text().size();
        }
    }

    LRect<double> approxMaximumBrect(m_coord.m_x,
                              m_coord.m_y,
                              charCount * fm.averageCharWidth(),
                              -m_tspans.size() * fm.height());
    return p->transform().mapRect(approxMaximumBrect);
}

LRect<double> LSvgText::internalBounds(LPainter *p, LSvgExtraStates &states) const
{
    LRect<double> boundingRect;
    if (shouldDrawNode(p, states))
        drawHelper(p, states, &boundingRect);
    return p->transform().mapRect(boundingRect);
}

void LSvgText::drawCommand(LPainter *p, LSvgExtraStates &states)
{
    drawHelper(p, states);
}

bool LSvgText::shouldDrawNode(LPainter *p, LSvgExtraStates &) const
{
    lsizetype numChars = 0;
    std::shared_ptr<LFont> font = p->font();
    double originalFontSize = font->pointSizeF();
    double maxFontSize = originalFontSize;
    for (const std::shared_ptr<LSvgTspan>& span : m_tspans) {
        if (span == LINEBREAK){
            continue;
        }

        numChars += span->text().size();

        std::shared_ptr<LSvgFontStyle> style = std::dynamic_pointer_cast<LSvgFontStyle>(span->styleProperty(LSvgStyleProperty::FONT));
        if (style != nullptr && style->font()->pointSizeF() > maxFontSize)
            maxFontSize = style->font()->pointSizeF();
    }


    font->setPixelSize(maxFontSize);
    LFontMetrics fm(font);
    if (m_tspans.size() * fm.height() >= LK_SVG_MAX_LAYOUT_SIZE) {
        lWarn() << "Text element too high to lay out, ignoring";
        return false;
    }

    if (numChars * fm.maxWidth() >= LK_SVG_MAX_LAYOUT_SIZE) {
        lWarn() << "Text element too wide to lay out, ignoring";
        return false;
    }

    return true;
}

void LSvgText::drawHelper(LPainter *p, LSvgExtraStates &states, LRect<double> *boundingRect) const
{
    const bool isPainting = (boundingRect == nullptr);
    if (!isPainting || shouldDrawNode(p, states)) {
        double oldOpacity = p->opacity();
        p->setOpacity(oldOpacity * states.fillOpacity);

        std::shared_ptr<LFont> font = p->font();
        LAlignment alignment = states.textAnchor;

        double y = 0;
        bool initial = true;
        double px = m_coord.m_x;
        double py = m_coord.m_y;

        if (m_type == Textarea) {
            if (alignment == LAlignment::HCenter){
                px += m_size.width() / 2;
            }
            else if (alignment == LAlignment::Right){
                px += m_size.width();
            }
        }

        LRect<double> bounds;
        if (m_size.height() != 0)
            bounds = LRect<double>(0, py, 1, m_size.height()); // x and width are not used.

        bool appendSpace = false;
        std::list<std::string> paragraphs;
        std::list<std::vector<LTextLayout::FormatRange> > formatRanges;
        paragraphs.push_back(std::string());
        formatRanges.push_back(std::vector<LTextLayout::FormatRange>());

        for (const std::shared_ptr<LSvgTspan>& span :  m_tspans) {
            if (span == LINEBREAK) {
                if (m_type == Textarea) {
                    if (paragraphs.back().empty()) {
                        font->setPixelSize(font->pointSizeF());
                        font->setHintingPreference(LFont::PreferNoHinting);

                        LTextLayout::FormatRange range;
                        range.start = 0;
                        range.length = 1;
                        range.format.setFont(font);
                        formatRanges.back().push_back(range);
                        paragraphs.back().append(" ");
                    }
                    appendSpace = false;
                    paragraphs.push_back(std::string());
                    formatRanges.push_back(std::vector<LTextLayout::FormatRange>());
                }
            } else {
                WhitespaceMode mode = span->whitespaceMode();
                span->applyStyle(p, states);

                font = p->font();
                font->setPixelSize(font->pointSizeF());
                font->setHintingPreference(LFont::PreferNoHinting);

                std::string newText= LStringUtil::replace(span->text(), "\t", " ");
                newText= LStringUtil::replace(newText, "\n", " ");

                bool prependSpace = !appendSpace && !span->isTspan() && (mode == Default) && !paragraphs.back().empty() && newText.starts_with(' ');
                if (appendSpace || prependSpace)
                    paragraphs.back().append(" ");

                bool appendSpaceNext = (!span->isTspan() && (mode == Default) && newText.ends_with(' '));

                if (mode == Default) {
                    newText = LStringUtil::simplified(newText);
                    if (newText.empty()){
                        appendSpaceNext = false;
                    }
                }

                LTextLayout::FormatRange range;
                range.start = paragraphs.back().size();
                range.length = newText.size();
                range.format.setFont(font);
                range.format.setTextOutline(p->pen());
                range.format.setForeground(p->brush());

                if (appendSpace) {
                    assert(!formatRanges.back().empty());
                    ++formatRanges.back().back().length;
                } else if (prependSpace) {
                    --range.start;
                    ++range.length;
                }
                formatRanges.back().push_back(range);
                appendSpace = appendSpaceNext;
                paragraphs.back().append(newText);

                span->revertStyle(p, states);
            }
        }

        if (states.svgFont != nullptr) {
            // SVG fonts not fully supported...
            std::list<std::string>::const_iterator iter = paragraphs.cbegin();
            std::string text = *iter;
            ++iter;
            for (; iter != paragraphs.cend(); ++iter) {
                text.append("\n");
                text.append(*iter);
            }
            if (isPainting) {
                states.svgFont->draw( p, m_coord, text, p->font()->pointSizeF(), states.textAnchor);
            }
            if (boundingRect) {
                *boundingRect = states.svgFont->boundingRect(p, m_coord, text, p->font()->pointSizeF(), states.textAnchor);
            }
        } else {
            LRect<double> brect;
            LTextLayout tl;
            std::list<std::vector<LTextLayout::FormatRange> >::const_iterator iter = formatRanges.cbegin();
            for (const std::string& s : paragraphs) {
                tl.setText(LStringUtil::utf8ToWide(s));
                std::shared_ptr<LTextEngine>  engine = tl.engine();
                engine->m_option.m_wordWrap = (LTextOption::WrapAtWordBoundaryOrAnywhere);
                engine->setFormats(*iter);
                ++iter;
                tl.beginLayout();

                while(true) {
                    LTextLine line = tl.createLine();
                    if (!line.isValid()){
                        break;
                    }
                    if (m_size.width() != 0){
                        line.setLineWidth(m_size.width());
                    }
                }
                tl.endLayout();

                bool endOfBoundsReached = false;
                for (int i = 0; i < tl.lineCount(); ++i) {
                    LTextLine line = tl.lineAt(i);

                    double x = 0;
                    if (alignment == LAlignment::HCenter){
                        x -= 0.5 * line.naturalTextWidth();
                    }
                    else if (alignment == LAlignment::Right){
                        x -= line.naturalTextWidth();
                    }

                    if (initial && m_type == Text){
                        y -= line.ascent();
                    }
                    initial = false;

                    line.setPosition(LPoint<double>(x, y));
                    brect |= line.naturalTextRect();

                    // Check if the current line fits into the bounding rectangle.
                    if ((m_size.width() != 0 && line.naturalTextWidth() > m_size.width())
                        || (m_size.height() != 0 && (y + line.height()) > m_size.height())) {
                        // I need to set the bounds height to 'y-epsilon' to avoid drawing the current
                        // line. Since the font is scaled to 100 units, 1 should be a safe epsilon.
                        bounds.setHeight(y - 1);
                        endOfBoundsReached = true;
                        break;
                    }

                    y += 1.1 * line.height();
                }
                if (isPainting){
                    tl.draw(p, LPoint<double>(px, py), std::vector<LTextLayout::FormatRange>(), bounds);
                }

                if (endOfBoundsReached){
                    break;
                }
            }
            if (boundingRect) {
                brect.translate(m_coord);
                if (bounds.height() > 0){
                    brect.setY2(std::min<double>(brect.y2(), bounds.y2()));
                }
                *boundingRect = brect;
            }
        }

        p->setOpacity(oldOpacity);
    }
}

void LSvgText::addText(const std::string &text)
{
    std::shared_ptr<LSvgNode> tThis = getPtrT<LSvgNode>();
    std::shared_ptr<LSvgTspan> t= std::make_shared<LSvgTspan>(tThis, false);
    t->setWhitespaceMode(m_mode);
    t->addText(text);
    m_tspans.push_back(t);
}

LSvgUse::LSvgUse(const LPoint<double> &start, std::shared_ptr<LSvgNode> parent, std::shared_ptr<LSvgNode> node)
    : LSvgNode(parent), m_link(node), m_start(start), m_recursing(false)
{

}

LSvgUse::LSvgUse(const LPoint<double> &start, std::shared_ptr<LSvgNode> parent, const std::string &linkId)
    : LSvgNode(parent),  m_start(start), m_linkId(linkId), m_recursing(false)
{

}

void LSvgUse::drawCommand(LPainter *p, LSvgExtraStates &states)
{
    if (L_UNLIKELY(!m_link || isDescendantOf(m_link) || m_recursing)){
        return;
    }

    assert(states.nestedUseCount == 0 || states.nestedUseLevel > 0);
    if (states.nestedUseLevel > 3 && states.nestedUseCount > (256 + states.nestedUseLevel * 2)) {
        lWarn() <<"Too many nested use nodes at " << m_linkId;
        return;
    }

    states.inUse = true;
    if (!m_start.isNull()) {
        p->translate(m_start);
    }

    if (states.nestedUseLevel > 0){
        ++states.nestedUseCount;
    }

    {
        states.nestedUseLevel ++;
        m_recursing = true;
        m_link->draw(p, states);
        m_recursing = false;
        states.nestedUseLevel --;
    }

    if (states.nestedUseLevel == 0){
        states.nestedUseCount = 0;
    }

    if (!m_start.isNull()) {
        p->translate(-m_start);
    }
    states.inUse = false;
}

LSvgNode::Type LSvgDummyNode::type() const
{
    return FeUnsupported;
}

LSvgNode::Type LSvgCircle::type() const
{
    return Circle;
}

LSvgNode::Type LSvgEllipse::type() const
{
    return Ellipse;
}

LSvgNode::Type LSvgImage::type() const
{
    return Image;
}

LSvgNode::Type LSvgLine::type() const
{
    return Line;
}

LSvgNode::Type LSvgPath::type() const
{
    return Path;
}

LSvgNode::Type LSvgPolygon::type() const
{
    return Polygon;
}

LSvgNode::Type LSvgPolyline::type() const
{
    return Polyline;
}

LSvgNode::Type LSvgRect::type() const
{
    return Rect;
}

LSvgNode::Type LSvgText::type() const
{
    return m_type;
}

LSvgNode::Type LSvgUse::type() const
{
    return Use;
}

LSvgNode::Type LSvgVideo::type() const
{
    return Video;
}

LRect<double> LSvgUse::internalBounds(LPainter *p, LSvgExtraStates &states) const
{
    LRect<double> bounds;
    if (L_LIKELY(m_link && !isDescendantOf(m_link) && !m_recursing)) {
        m_recursing = true;
        p->translate(m_start);
        bounds = m_link->bounds(p, states);
        p->translate(-m_start);
        m_recursing = true;
    }
    return bounds;
}

LRect<double> LSvgUse::decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const
{
    LRect<double> bounds;
    if (L_LIKELY(m_link && !isDescendantOf(m_link) && !m_recursing)) {
        m_recursing = true;
        p->translate(m_start);
        bounds = m_link->decoratedBounds(p, states);
        p->translate(-m_start);
        m_recursing = false;
    }
    return bounds;
}

LRect<double> LSvgPolyline::internalFastBounds(LPainter *p, LSvgExtraStates &) const
{
    return p->transform().mapRect(m_poly.boundingRect());
}

LRect<double> LSvgPolyline::internalBounds(LPainter *p, LSvgExtraStates &s) const
{
    return internalBounds(p, s, BoundsMode::Simplistic);
}

LRect<double> LSvgPolyline::decoratedInternalBounds(LPainter *p, LSvgExtraStates &s) const
{
    LRect<double> rect = internalBounds(p, s, BoundsMode::IncludeMiterLimit);
    std::shared_ptr<LSvgNode> tThis = getPtrT<LSvgNode>();
    rect |= LSvgMarker::markersBoundsForNode(tThis, p, s);
    return filterRegion(rect);
}

bool LSvgPolyline::requiresGroupRendering() const
{
    return hasAnyMarker();
}

LRect<double> LSvgPolyline::internalBounds(LPainter *p, LSvgExtraStates &, BoundsMode mode) const
{
    double sw = strokeWidth(p);
    if (lFuzzyIsZero(sw)) {
        return p->transform().map(m_poly).boundingRect();
    } else {
        std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
        path->addPolygon(m_poly);
        return boundsOnStroke(p, path, sw, mode);
    }
}

LRect<double> LSvgImage::internalBounds(LPainter *p, LSvgExtraStates &) const
{
    return p->transform().mapRect(m_bounds);
}

LRect<double> LSvgLine::internalFastBounds(LPainter *p, LSvgExtraStates &) const
{
    LPoint<double> p1 = p->transform().map(m_line.m_pt1);
    LPoint<double> p2 = p->transform().map(m_line.m_pt2);
    double minX = std::min<double>(p1.m_x, p2.m_x);
    double minY = std::min<double>(p1.m_y, p2.m_y);
    double maxX = std::max<double>(p1.m_x, p2.m_x);
    double maxY = std::max<double>(p1.m_y, p2.m_y);
    return LRect<double>(minX, minY, maxX - minX, maxY - minY);
}

LRect<double> LSvgLine::internalBounds(LPainter *p, LSvgExtraStates &s) const
{
    return  internalBounds(p, s, BoundsMode::Simplistic);
}

LRect<double> LSvgLine::decoratedInternalBounds(LPainter *p, LSvgExtraStates &s) const
{
    LRect<double> rect = internalBounds(p, s, BoundsMode::IncludeMiterLimit);
    std::shared_ptr<LSvgNode> tThis = getPtrT<LSvgNode>();
    rect |= LSvgMarker::markersBoundsForNode(tThis, p, s);
    return filterRegion(rect);
}

bool LSvgLine::requiresGroupRendering() const
{
    return hasAnyMarker();
}

LRect<double> LSvgLine::internalBounds(LPainter *p, LSvgExtraStates &s, BoundsMode mode) const
{
    double sw = strokeWidth(p);
    if (lFuzzyIsZero(sw)) {
        return internalFastBounds(p, s);
    } else {
        std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
        path->moveTo(m_line.m_pt1);
        path->lineTo(m_line.m_pt2);
        return boundsOnStroke(p, path, sw, mode);
    }
}

LK_NAMESPACE_END
