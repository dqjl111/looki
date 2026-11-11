#include "lpaintengine.h"
#include "lkgui/paint/lpainter.h"
#include "lkgui/text/lfontengine.h"

LK_NAMESPACE_BEGIN

#if !defined(LK_MAX_CACHED_GLYPH_SIZE)
#  define LK_MAX_CACHED_GLYPH_SIZE 64
#endif

LPixmapFragment LPixmapFragment::create(const LPoint<double> &pos, const LRect<double> &sourceRect, double scaleX, double scaleY, double rotation, double opacity)
{
    LPixmapFragment fragment = {pos.m_x, pos.m_y, sourceRect.m_x, sourceRect.m_y,
                                sourceRect.width(), sourceRect.height(), scaleX, scaleY, rotation, opacity};
    return fragment;
}


LPaintEngine::LPaintEngine()
{
    m_stroker = std::make_shared<LStroker>();
    m_dasher = std::make_shared<LDashStroker>(m_stroker);
}

std::shared_ptr<LPainterState> LPaintEngine::createState(std::shared_ptr<LPainterState> org) const
{
    std::shared_ptr<LPainterState> ret = nullptr;
    if (org == nullptr){
        ret = std::make_shared<LPainterState>();
    }else{
        ret = std::make_shared<LPainterState>(*org);
    }
    return ret;
}

void LPaintEngine::updateState(const std::shared_ptr<LPainterState> state)
{
    // do nothing...
}

void LPaintEngine::setState(std::shared_ptr<LPainterState> s)
{
    m_state = s;
}

void LPaintEngine::setSystemTransform(const LTransform2D &xform)
{
    m_systemTransform = xform;
    m_hasSystemTransform = !xform.isIdentity();
    updateSystemClip();
    if (m_state != nullptr){
        systemStateChanged();
    }
}

void LPaintEngine::updateSystemClip()
{
    m_systemClip = m_baseSystemClip;
    if(m_systemClip == nullptr){
        return;
    }
    if (m_systemClip->isEmpty()){
        return;
    }

    if (m_hasSystemTransform) {
        if (m_systemTransform.type() <= LTransform2D::TxTranslate){
            m_systemClip->translate(std::lround(m_systemTransform.dx()), std::lround(m_systemTransform.dy()));
        }
        else{
            m_systemClip = m_systemTransform.map(m_systemClip);
        }
    }

    // Make sure we're inside the viewport.
    if (m_hasSystemTransform) {
        m_systemClip->intersect(m_systemViewport);
        if (m_systemClip->isEmpty()) {
            if(m_systemViewport != nullptr){
                // We don't want to paint without system clip, so set it to 1 pixel :)
                *m_systemClip = LRect<int>(m_systemViewport->boundingRect().topLeft(), 1, 1);
            }
        }
    }
}

LPoint<int> LPaintEngine::coordinateOffset() const
{
    return LPoint<int>();
}

void LPaintEngine::drawTextItem(const LPoint<double> &p, const LTextItem &ti)
{
    if (ti.glyphs.numGlyphs == 0){
        return;
    }

    if (ti.fontEngine->m_glyphFormat == LGlyphFormat::ARGB) {
        std::vector<LPoint<double> > positions;
        std::vector<uint32_t> glyphs;
        LTransform2D matrix = LTransform2D::fromTranslate(p.m_x, p.m_y - ti.fontEngine->ascent());
        ti.fontEngine->getGlyphPositions(ti.glyphs, matrix, ti.flags, glyphs, positions);

        LPainter* paint = painter();
        if(paint != nullptr){
            paint->save();
            paint->setRenderHints(LRenderHint::SmoothPixmapTransform,
                                  bool((paint->renderHints() & LRenderHint::TextAntialiasing)
                                       && !(paint->font()->styleStrategy() & LFont::NoAntialias)));
            for (int i = 0; i < ti.glyphs.numGlyphs; ++i) {
                std::shared_ptr<LImage> glyph = ti.fontEngine->bitmapForGlyph(glyphs[i], 0, LTransform2D());
                if(glyph != nullptr){
                    paint->drawImage(positions[i], glyph);
                }
            }
            paint->restore();
        }
        return;
    }

    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    path->setFillRule(LFillRule::Winding);
    ti.fontEngine->addOutlineToPath(0, 0, ti.glyphs, path.get(), ti.flags);
    if (!path->isEmpty()) {
        LPainter* paint = painter();
        if(paint != nullptr){

            paint->save();
            paint->setRenderHints(LRenderHint::Antialiasing,
                                  bool((paint->renderHints() & LRenderHint::TextAntialiasing)
                                       && !(paint->font()->styleStrategy() & LFont::NoAntialias)));

            paint->translate(LPoint<double>(p.m_x, p.m_y));
            paint->fillPath(path, paint->pen()->brush());
            paint->restore();
        }
    }
}


LPainter *LPaintEngine::painter() const
{
    return m_state != nullptr ? m_state->painter : nullptr;
}


void LPaintEngine::stroke(const std::shared_ptr<LPainterPath> path, const std::shared_ptr<LPen> pen)
{
    if (path==nullptr || path->isEmpty()){
        return;
    }

    if(pen == nullptr){
        return;
    }

    if (m_strokeHooker == nullptr) {
        m_strokeHooker = std::make_shared<LStrokeHooker>();
    }

    if (pen !=  m_strokerPen) {
        m_strokerPen = pen;
        m_stroker->setJoinStyle(pen->m_joinStyle);
        m_stroker->setCapStyle(pen->m_capStyle);
        m_stroker->m_miterLimit = pen->m_miterLimit;
        m_stroker->m_strokeWidth = pen->m_width;
        if (lFuzzyIsZero(m_stroker->m_strokeWidth)){
            m_stroker->m_strokeWidth = 1;
        }

        LPenStyle style = pen->m_style;
        if (style == LPenStyle::Solid) {
            m_activeStroker = m_stroker;
        } else if (style == LPenStyle::No) {
            m_activeStroker = nullptr;
        } else {
            m_dasher->m_dashPattern = pen->dashPattern();
            m_dasher->m_dashOffset = pen->m_dashOffset;
            m_activeStroker = m_dasher;
        }
    }

    if (m_activeStroker == nullptr) {
        return;
    }

    std::shared_ptr<LPainterState> pState = state();

    if (pen->m_style > LPenStyle::Solid) {
        if (pen->isCosmetic()){
            m_activeStroker->m_clipRect = m_exDeviceRect;
        } else {
            m_activeStroker->m_clipRect  = pState->matrix.inverted().mapRect(m_exDeviceRect);
        }
    }

    const std::vector<LPainterPath::Element>& eles = path->elements();
    // Some engines might decide to optimize for the non-shape hint later on...
    uint32_t flags = path->hints();
    if (eles.size() > 2){
        flags |= LPainterPath::NonConvexShapeMask;
    }

    if (m_stroker->capStyle() == LPenCapStyle::Round
        || m_stroker->joinStyle() == LPenJoinStyle::Round){
        flags |= LPainterPath::CurvedShapeMask;
    }

    // ### Perspective Xforms are currently not supported...
    m_strokeHooker->begin();
    if (!pen->isCosmetic()) {
        // We include cosmetic pens in this case to avoid having to
        // change the current transform. Normal transformed,
        // non-cosmetic pens will be transformed as part of fill
        // later, so they are also covered here..
        flags |= m_activeStroker->strokePath(path, m_strokeHooker, LTransform2D());
        if (m_strokeHooker->isEmpty()){
            return;
        }
        m_strokeHooker->end();
        m_strokeHooker->setHints(flags);
        fill(m_strokeHooker->strokedPath(), pen->brush());
    } else {
        // For cosmetic pens we need a bit of trickery... We to process xform the input points
        if (pState->matrix.type() >= LTransform2D::TxProject) {
            std::shared_ptr<LPainterPath> painterPath = pState->matrix.map(path);
            flags |= m_activeStroker->strokePath(painterPath, m_strokeHooker, LTransform2D());
        } else {
            flags |= m_activeStroker->strokePath(path, m_strokeHooker, state()->matrix);
        }
        if (m_strokeHooker->isEmpty()){
            return;
        }
        m_strokeHooker->end();

        LTransform2D xform = state()->matrix;
        state()->matrix = LTransform2D();
        transformChanged();

        if (pen->brush()->m_style != LBrush::SolidPattern){
            pen->brush()->m_transform *= xform;
        }

        m_strokeHooker->setHints(flags);
        fill(m_strokeHooker->strokedPath(), pen->brush());

        state()->matrix = xform;
        transformChanged();
    }

}

void LPaintEngine::clip(const LRect<int> &rect, LClipOper op)
{
    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    path->addRect(rect.toRectF());
    clip(path, op);
}

void LPaintEngine::clip(const std::shared_ptr<LRegion> region, LClipOper op)
{
    if(region != nullptr){
        if(!region->isEmpty()){
            std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
            for(const LRect<int>& rect: *region){
                path->addRect(rect.toRectF());
            }
            clip(path, op);
        }
    }
}

void LPaintEngine::drawPath(const std::shared_ptr<LPainterPath> path)
{
    std::shared_ptr<LBrush> brush = state()->brush;
    if (brush->m_style != LBrush::NoPattern){
        fill(path, brush);
    }

    std::shared_ptr<LPen> pen = state()->pen;
    if (pen->m_style != LPenStyle::No && pen->brush()->m_style != LBrush::NoPattern){
        stroke(path, pen);
    }
}

void LPaintEngine::drawImage(const LPoint<double> &pos, const std::shared_ptr<LImage> image)
{
    drawImage(LRect<double>(pos, image->sizeF() / image->devicePixelRatio()), image, image->rectF());

}

void LPaintEngine::drawPixmap(const LPoint<double> &pos, const std::shared_ptr<LPixmap> pm)
{
    drawPixmap(LRect<double>(pos, pm->sizeF() / pm->devicePixelRatio()), pm, pm->rectF());
}

void LPaintEngine::drawTiledPixmap(const LRect<double> &r,
                                     const std::shared_ptr<LPixmap> pm,
                                     const LPoint<double> &s)
{
    std::shared_ptr<LBrush> brush
        = std::make_shared<LBrush>(state()->pen->color(), LBrush::TexturePattern);
    brush->setPixmap(pm);
    brush->m_transform = LTransform2D::fromTranslate(r.m_x - s.m_x, r.m_y - s.m_y);
    if (!lFuzzyCompare(pm->devicePixelRatioF(), 1.0)){
        brush->m_transform.scale(1.0/pm->devicePixelRatioF(), 1.0/pm->devicePixelRatioF());
    }

    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    path->addRect(r);
    fill(path, brush);
}

void LPaintEngine::drawPixmapFragments(const std::vector<LPixmapFragment> fragments, const std::shared_ptr<LPixmap> pm, bool isOpaque)
{
    if (pm == nullptr){
        return;
    }

    double oldOpacity = state()->opacity;
    LTransform2D oldTransform = state()->matrix;

    for (int i = 0; i < fragments.size(); ++i) {
        LTransform2D transform = oldTransform;
        transform.translate(fragments[i].x, fragments[i].y);
        transform.rotate(fragments[i].rotation);
        state()->opacity = oldOpacity * fragments[i].opacity;
        state()->matrix = transform;
        opacityChanged();
        transformChanged();

        double w = fragments[i].scaleX * fragments[i].width;
        double h = fragments[i].scaleY * fragments[i].height;
        LRect<double> sourceRect(fragments[i].sourceLeft, fragments[i].sourceTop,
                                 fragments[i].width, fragments[i].height);
        drawPixmap(LRect<double>(-0.5 * w, -0.5 * h, w, h, false), pm, sourceRect);
    }

    state()->opacity = oldOpacity;
    state()->matrix = oldTransform;
    opacityChanged();
    transformChanged();
}

void LPaintEngine::drawRects(const LRect<int> *rects, int rectCount)
{
    for (int i=0; i<rectCount; ++i) {
        std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
        path->addRect(rects[i].toRectF());
        if (path->isEmpty()){
            continue;
        }
        drawPath(path);
    }
}

void LPaintEngine::drawRects(const LRect<double> *rects, int rectCount)
{
    for (int i=0; i<rectCount; ++i) {
        std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
        path->addRect(rects[i]);
        if (path->isEmpty()){
            continue;
        }
        drawPath(path);
    }
}

void LPaintEngine::drawRoundedRect(const LRect<double> &rect, double xRadius, double yRadius, LSizeMode mode)
{
    if (xRadius <= 0 || yRadius <= 0) {             // draw normal rectangle
        drawRects(&rect, 1);
        return;
    }

    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    path->addRoundedRect(rect, xRadius, yRadius, mode);
    drawPath(path);
}

void LPaintEngine::drawLines(const LLine<int> *lines, int lineCount)
{
    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    for (int i=0; i<lineCount; ++i) {
        path->moveTo(lines[i].m_pt1.m_x, lines[i].m_pt1.m_y);
        path->lineTo(lines[i].m_pt2.m_x, lines[i].m_pt2.m_y);
    }
    stroke(path, state()->pen);
}

void LPaintEngine::drawLines(const LLine<double> *lines, int lineCount)
{
    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    for (int i=0; i<lineCount; ++i) {
        path->moveTo(lines[i].m_pt1.m_x, lines[i].m_pt1.m_y);
        path->lineTo(lines[i].m_pt2.m_x, lines[i].m_pt2.m_y);
    }
    stroke(path, state()->pen);

}

void LPaintEngine::drawArc(const LRect<double> &r, double a, double alen)
{
    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    path->arcMoveTo(r, a/16.0);
    path->arcTo(r, a/16.0, alen/16.0);
    stroke(path, state()->pen);
}

void LPaintEngine::drawEllipses(const LRect<int> *rects, int rectCount)
{
    for (int i=0; i<rectCount; ++i) {
        std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
        path->addEllipse(rects[i].toRectF());
        if (path->isEmpty()){
            continue;
        }
        drawPath(path);
    }
}

void LPaintEngine::drawEllipses(const LRect<double> *rects, int rectCount)
{
    for (int i=0; i<rectCount; ++i) {
        std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
        path->addEllipse(rects[i]);
        if (path->isEmpty()){
            continue;
        }
        drawPath(path);
    }
}

void LPaintEngine::fillRect(const LRect<double> &rect, const std::shared_ptr<LBrush> brush)
{
    std::shared_ptr<LPainterPath> p = std::make_shared<LPainterPath>();
    p->addRect(rect);
    fill(p, brush);
}

void LPaintEngine::fillRect(const LRect<double> &rect, const LColor &color)
{
    std::shared_ptr<LBrush> b = std::make_shared<LBrush>(color);
    fillRect(rect, b);
}

void LPaintEngine::drawPolygon(const LPoint<double> *points, int pointCount, PolygonDrawMode mode)
{
    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    path->addPolygon(points, pointCount);
    if (mode == PolylineMode){
        stroke(path, state()->pen);
    }
    else{
        drawPath(path);
    }
}

void LPaintEngine::drawPoints(const LPoint<double> *points, int pointCount)
{
    if(m_state == nullptr){
        return;
    }

    std::shared_ptr<LPen> pen = m_state->pen;
    if(pen != nullptr){
        if (pen->m_capStyle == LPenCapStyle::Flat){
            pen = pen->backPen();
            pen->m_capStyle = LPenCapStyle::Square;
        }

        if (pen->brush()->isOpaque()) {
            std::array<LLine<double>, 16> lines;
            while (pointCount > 0) {
                int count = std::min<int>(pointCount, 16);
                int oset = 0;

                for (int i=0; i<count; ++i) {
                    lines[oset].m_pt1.m_x = points[i].m_x;
                    lines[oset].m_pt1.m_y = points[i].m_y;
                    lines[oset].m_pt2.m_x = points[i].m_x + 1/63.;
                    lines[oset].m_pt2.m_y = points[i].m_y;
                }

                std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
                path->addLine(lines.data(), count);
                stroke(path, pen);
                pointCount -= 16;
                points += 16;
            }
        } else {
            for (int i=0; i<pointCount; ++i) {
                LLine<double> line = { points[i].m_x, points[i].m_y, points[i].m_x +1/63., points[i].m_y};

                std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
                path->addLine(line);
                stroke(path, pen);
            }
        }
    }
}

void LPaintEngine::drawPoints(const LPoint<int> *points, int pointCount)
{
    if(m_state == nullptr){
        return;
    }

    std::shared_ptr<LPen> pen = m_state->pen;
    if(pen != nullptr){
        if (pen->m_capStyle == LPenCapStyle::Flat){
            pen = pen->backPen();
            pen->m_capStyle = LPenCapStyle::Square;
        }

        if (pen->brush()->isOpaque()) {
            std::array<LLine<double>, 16> lines;
            while (pointCount > 0) {
                int count = std::min<int>(pointCount, 16);
                int oset = 0;

                for (int i=0; i<count; ++i) {
                    lines[oset].m_pt1.m_x = points[i].m_x;
                    lines[oset].m_pt1.m_y = points[i].m_y;
                    lines[oset].m_pt2.m_x = points[i].m_x + 1/63.;
                    lines[oset].m_pt2.m_y = points[i].m_y;
                }

                std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
                path->addLine(lines.data(), count);
                stroke(path, pen);
                pointCount -= 16;
                points += 16;
            }
        } else {
            for (int i=0; i<pointCount; ++i) {
                LLine<double> line = { double(points[i].m_x), double(points[i].m_y),
                               double(points[i].m_x +1/63.), double(points[i].m_y) };

                std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
                path->addLine(line);
                stroke(path, pen);
            }
        }
    }
}

bool LPaintEngine::requiresPretransformedGlyphPositions(std::shared_ptr<LFontEngine> fontEngine,
                                                          const LTransform2D &m) const
{
    return false;
}


void LPaintEngine::replayClipOperations()
{
    LPainter *p = painter();
    if (p == nullptr){
        return;
    }

    std::shared_ptr<LPainterState>  tState = state();
    const std::deque<LPainterClipInfo> &clipInfo = tState->clipInfo;
    LTransform2D transform = tState->matrix;

    for (const LPainterClipInfo &info : clipInfo) {

        if (info.matrix != tState->matrix) {
            tState->matrix = info.matrix;
            transformChanged();
        }

        switch (info.clipType) {
        case LPainterClipInfo::RegionClip:{
            clip(info.region, info.operation);
            break;
        }
        case LPainterClipInfo::PathClip:{
            clip(info.path, info.operation);
            break;
        }
        case LPainterClipInfo::RectClip:{
            std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
            path->addRect(info.rect);
            clip(path, info.operation);
            break;
        }
        }
    }

    if (transform != tState->matrix) {
        tState->matrix = transform;
        transformChanged();
    }
}

LK_NAMESPACE_END
