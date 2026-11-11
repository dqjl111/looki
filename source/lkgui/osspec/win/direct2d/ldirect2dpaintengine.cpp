#include "ldirect2dpaintengine.h"
#include "ldirect2dpixmap.h"
#include "ldirect2dcontext.h"
#include "lkgui/osspec/win/lwindowsfontdatabase.h"
#include "lkcore/osspec/win/lwinregistry.h"
#include "lkgui/image/lpixmap.h"
#include "lkgui/paint/lpainter.h"

LK_NAMESPACE_BEGIN
static inline D2D1_MATRIX_3X2_F transformFromLine(const LLine<double> &line,
                                                  double penWidth,
                                                  double dashOffset)
{
    const double halfWidth = penWidth / 2;
    const double angle = -line.angle();
    const double sinA = std::sin(angle);
    const double cosA = std::cos(angle);
    LTransform2D transform = LTransform2D::fromTranslate(line.m_pt1.m_x + dashOffset * cosA + sinA * halfWidth,
                                                     line.m_pt1.m_y + dashOffset * sinA - cosA * halfWidth);
    transform.rotate(angle);
    return toD2dMatrix3x2F(transform);
}


LDirect2DPaintEngine::LDirect2DPaintEngine(uint32_t dxEngineFlags)
{

}

LPaintEngine::Type LDirect2DPaintEngine::type() const
{
    return Direct2D;
}

bool LDirect2DPaintEngine::begin(std::shared_ptr<LPaintDevice> pdev)
{
    std::shared_ptr<LPaintDevice> org = m_pdev.lock();
    if(pdev == nullptr){
        pdev = org;
    }else if(org != pdev){
        m_pdev = pdev;
    }
    std::shared_ptr<LPixmap> pm = std::dynamic_pointer_cast<LPixmap>(pdev);
    if(pm != nullptr){
        std::shared_ptr<LDirect2DPixmap> dp =
            std::dynamic_pointer_cast<LDirect2DPixmap>(pm->handle());
        if(dp!= nullptr){
            dp->beginPaint();

            ID2D1DeviceContext *thisCtx = dp->deviceContext();
            if(thisCtx != nullptr){
                thisCtx->SetTransform(D2D1::Matrix3x2F::Identity());

                if (m_systemClip != nullptr && m_systemClip->rectCount() > 1) {
                    std::shared_ptr<LPainterPath> p = std::make_shared<LPainterPath>();
                    p->addRegion(m_systemClip);

                    Microsoft::WRL::ComPtr<ID2D1PathGeometry1> geometry = painterPathToDxPath(p);
                    if (!geometry){
                        return false;
                    }

                    thisCtx->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(),
                                                              geometry.Get(),
                                                              antialiasMode(),
                                                              D2D1::IdentityMatrix(),
                                                              1.0,
                                                              nullptr,
                                                              layerOptions()),
                                       nullptr);
                }else{
                    LRect<double> clip(0, 0, pdev->width(), pdev->height(), false);
                    if (m_systemClip != nullptr){
                        clip &= m_systemClip->boundingRect().toRectF();
                    }
                    thisCtx->PushAxisAlignedClip(toD2dRectF(clip), D2D1_ANTIALIAS_MODE_ALIASED);
                    m_clipFlags |= SimpleSystemClip;
                }
            }
            m_active = true;
            return true;
        }           
    }
    return false;
}

bool LDirect2DPaintEngine::end()
{
    // Pop any user-applied clipping
    clearClips();
    std::shared_ptr<LPixmap> pm = std::dynamic_pointer_cast<LPixmap>(m_pdev.lock());
    if(pm != nullptr){
        std::shared_ptr<LDirect2DPixmap> dp =
            std::dynamic_pointer_cast<LDirect2DPixmap>(pm->handle());        
        if(dp!= nullptr){
            ID2D1DeviceContext *thisCtx = dp->deviceContext();
            if(thisCtx != nullptr){
                // Now the system clip from begin() above
                if (m_clipFlags & SimpleSystemClip) {
                    thisCtx->PopAxisAlignedClip();
                    m_clipFlags &= ~SimpleSystemClip;
                } else {
                    thisCtx->PopLayer();
                }
            }
            dp->endPaint();
        }

        m_active = false;
        return true;
    }
    m_active = false;
    return true;
}

void LDirect2DPaintEngine::setState(std::shared_ptr<LPainterState> s)
{
    LPaintEngine::setState(s);
    clearClips();

    clipEnabledChanged();
    penChanged();
    brushChanged();
    brushOriginChanged();
    opacityChanged();
    compositionModeChanged();
    renderHintsChanged();
    transformChanged();
}

void LDirect2DPaintEngine::clip(const std::shared_ptr<LPainterPath> path, LClipOper op)
{
    switch (op) {
    case LClipOper::No:{
        clearClips();
        break;
    }
    case LClipOper::Replace:{
        clearClips();
        pushClip(path);
        break;
    }
    case LClipOper::Intersect:{
        pushClip(path);
        break;
    }
    default:{
        break;
    }
    }
}

void LDirect2DPaintEngine::clipEnabledChanged()
{
    std::shared_ptr<LPainterState> s  = state();
    if(s!=nullptr){
        updateClipEnabled(s->clipEnabled);
    }
}

void LDirect2DPaintEngine::fill(const std::shared_ptr<LPainterPath> path, const std::shared_ptr<LBrush> brush)
{
    if (path==nullptr || path->isEmpty()){
        return;
    }

    ID2D1DeviceContext *thisCtx = dc();
    if(thisCtx != nullptr){
        ensureBrush(brush);
        if (m_brush.brush == nullptr){
            return;
        }

        Microsoft::WRL::ComPtr<ID2D1Geometry> geometry = painterPathToDxPath(path);
        if (!geometry) {
            lWarn() << "Could not convert path to d2d geometry";
            return;
        }

        thisCtx->FillGeometry(geometry.Get(), m_brush.brush.Get());
    }

}

void LDirect2DPaintEngine::stroke(const std::shared_ptr<LPainterPath> path,
                                  const std::shared_ptr<LPen> pen)
{
    if (path == nullptr || path->isEmpty()){
        return;
    }

    ensurePen(pen);
    if (!m_pen.brush){
        return;
    }

    ID2D1DeviceContext*  ctx = dc();
    if(ctx == nullptr){
        return;
    }

    std::shared_ptr<LPainterState> s =state();
    LPainterPath::Hint shape = path->shape();
    if (!(shape == LPainterPath::LinesHint || shape == LPainterPath::PolygonHint)
        || (m_pen.dashBrush == nullptr)
        || (s== nullptr)
        || (s->renderHints.testFlag(LRenderHint::Antialiasing))){
        Microsoft::WRL::ComPtr<ID2D1Geometry> geometry = painterPathToDxPath(path);
        if (!geometry) {
            lWarn() << "Could not convert path to d2d geometry";
            return;
        }
        ctx->DrawGeometry(geometry.Get(), m_pen.brush.Get(),
                          FLOAT(m_pen.lpen->m_width), m_pen.strokeStyle.Get());
        return;
    }

    // Optimized dash line drawing
    const bool isPolygon = shape == LPainterPath::PolygonHint && path->elementCount() >= 3;
    bool skipJoin = !isPolygon; // Non-polygons don't require joins
    if(m_pen.lpen->m_joinStyle == LPenJoinStyle::Miter && lFuzzyIsZero(m_pen.lpen->m_miterLimit)){
        skipJoin = true;
    }
    const std::vector<LPainterPath::Element>&  eles = path->elements();
    const int lastElement = path->elementCount() - 1;
    double dashOffset = 0;
    LPoint<double> jointStart;
    ID2D1Brush *brush = m_pen.dashBrush ? m_pen.dashBrush.Get() : m_pen.brush.Get();
    for (int i = 0; i <= lastElement; ++i) {
        const LPainterPath::Element& ele1 = eles.at(i);
        const LPainterPath::Element& ele2 = eles.at(i+1);

        LPoint<double> p1(ele1.m_x, ele1.m_y);
        LPoint<double> p2(ele2.m_x, ele2.m_y);

        if (!isPolygon){ // Advance the count for lines
            ++i;
        }

        // Match raster engine output
        if (p1 == p2 && m_pen.lpen->m_width <= 1.0) {
            fillRect(LRect<double>(p1, LSize<double>(m_pen.lpen->m_width, m_pen.lpen->m_width)), m_pen.lpen->brush());
            continue;
        }


        const LLine<double> line(p1, p2);
        const double lineLength = line.length();
        if (m_pen.dashBrush) {
            m_pen.dashBrush->SetTransform(transformFromLine(line, m_pen.lpen->m_width, dashOffset));
            dashOffset = m_pen.dashLength - fmod(lineLength - dashOffset, m_pen.dashLength);
        }
        ctx->DrawLine(toD2dPoint2F(p1), toD2dPoint2F(p2), brush, FLOAT(m_pen.lpen->m_width), nullptr);

        if (skipJoin){
            continue;
        }

        // Patch the join with the original brush
        const double patchSegment = m_pen.dashBrush ? lBound(0.0, (m_pen.dashLength - dashOffset) / lineLength, 1.0)
                                                    : m_pen.lpen->m_width;
        if (i > 0) {
            LPoint<double> sp = line.pointAt(patchSegment);
            Microsoft::WRL::ComPtr<ID2D1PathGeometry1> geo = joinPath(jointStart, p1, sp);
            ctx->DrawGeometry(geo.Get(), m_pen.brush.Get(), FLOAT(m_pen.lpen->m_width), m_pen.strokeStyle.Get());
        }
        // Record the start position of the next joint
        jointStart = line.pointAt(1 - patchSegment);
    }
}

void LDirect2DPaintEngine::fillRect(const LRect<double> &rect, const std::shared_ptr<LBrush> brush)
{
    ID2D1DeviceContext*  ctx = dc();
    if(ctx == nullptr){
        ensureBrush(brush);
        if (m_brush.brush){
            ctx->FillRectangle(toD2dRectF(rect.normalized()), m_brush.brush.Get());
        }
    }
}

void LDirect2DPaintEngine::penChanged()
{
    ensurePen();
}

void LDirect2DPaintEngine::brushChanged()
{
    ensureBrush();
}

void LDirect2DPaintEngine::brushOriginChanged()
{
    std::shared_ptr<LPainterState> s  = state();
    if(s!=nullptr){
        updateBrushOrigin(s->brushOrigin);
    }
}

void LDirect2DPaintEngine::opacityChanged()
{
    std::shared_ptr<LPainterState> s  = state();
    if(s!=nullptr){
        updateOpacity(s->opacity);
    }
}

void LDirect2DPaintEngine::compositionModeChanged()
{
    std::shared_ptr<LPainterState> s  = state();
    if(s!=nullptr){
        updateCompositionMode(s->compositionMode);
    }
}

void LDirect2DPaintEngine::transformChanged()
{
    std::shared_ptr<LPainterState> s  = state();
    if(s!=nullptr){
        updateTransform(s->matrix);
    }
}

void LDirect2DPaintEngine::renderHintsChanged()
{
    updateHints();
}

void LDirect2DPaintEngine::drawPath(const std::shared_ptr<LPainterPath> path)
{
    const std::shared_ptr<LBrush> brush = state()->brush;
    if(brush != nullptr){
        if (brush->m_style != LBrush::NoPattern) {
            fill(path, brush);
        }
    }

    const std::shared_ptr<LPen> &pen = state()->pen;
    if(pen != nullptr){
        if (pen->m_style != LPenStyle::No
            && pen->brush()->m_style != LBrush::NoPattern) {
            stroke(path, pen);
        }
    }

}

void LDirect2DPaintEngine::drawTextItem(const LPoint<double> &p, const LTextItem &ti)
{
    if (ti.glyphs.numGlyphs == 0){
        return;
    }

    //lWarn() << "direct draw text color:" << m_pen.qpen->color().name();

    ensurePen();
    Microsoft::WRL::ComPtr<IDWriteFontFace> fontFace = fontFaceFromFontEngine(ti.fontEngine);
    if (!fontFace) {
        lWarn() << "Could not find font - falling back to slow text rendering path.";
        LPaintEngine::drawTextItem(p, ti);
        return;
    }

    std::vector<UINT16> glyphIndices(ti.glyphs.numGlyphs);
    std::vector<FLOAT> glyphAdvances(ti.glyphs.numGlyphs);
    std::vector<DWRITE_GLYPH_OFFSET> glyphOffsets(ti.glyphs.numGlyphs);

    for (int i = 0; i < ti.glyphs.numGlyphs; i++) {
        glyphIndices[i] = UINT16(ti.glyphs.glyphs[i]); // Imperfect conversion here
        glyphAdvances[i] = FLOAT(ti.glyphs.effectiveAdvance(i));
        glyphOffsets[i].advanceOffset = FLOAT(ti.glyphs.offsets[i].m_x);
        // XXX Should we negate the y value like for static text items?
        glyphOffsets[i].ascenderOffset = FLOAT(ti.glyphs.offsets[i].m_y);
    }

    const bool rtl = (ti.flags & LTextRenderHint::RightToLeft);
    if(rtl){
        drawGlyphRun(D2D1::Point2F(p.m_x + ti.width, p.m_y),
                     fontFace.Get(),
                     ti.fontEngine->m_fontDef,
                     ti.glyphs.numGlyphs,
                     glyphIndices.data(),
                     glyphAdvances.data(),
                     glyphOffsets.data(),
                     rtl);
    }else{
        drawGlyphRun(D2D1::Point2F(p.m_x, p.m_y),
                     fontFace.Get(),
                     ti.fontEngine->m_fontDef,
                     ti.glyphs.numGlyphs,
                     glyphIndices.data(),
                     glyphAdvances.data(),
                     glyphOffsets.data(),
                     rtl);
    }

}

void LDirect2DPaintEngine::drawRects(const LRect<int> *rects, int rectCount)
{
    ID2D1DeviceContext *thisCtx = dc();
    if(thisCtx != nullptr){
        ensureBrush();
        ensurePen();
        for (int i = 0; i < rectCount; i++) {
            if(!rects[i].isEmpty()){
                D2D1_RECT_F d2d_rect = toD2dRectF(rects[i]);

                if (m_brush.brush){
                    thisCtx->FillRectangle(d2d_rect, m_brush.brush.Get());
                }

                if (m_pen.brush){
                    thisCtx->DrawRectangle(d2d_rect, m_pen.brush.Get(),
                                           FLOAT(m_pen.lpen->m_width), m_pen.strokeStyle.Get());
                }
            }
        }
    }
}

void LDirect2DPaintEngine::drawRects(const LRect<double> *rects, int rectCount)
{
    ID2D1DeviceContext *thisCtx = dc();
    if(thisCtx != nullptr){
        ensureBrush();
        ensurePen();
        for (int i = 0; i < rectCount; i++) {
            if(!rects[i].isEmpty()){
                D2D1_RECT_F d2d_rect = toD2dRectF(rects[i]);

                if (m_brush.brush){
                    thisCtx->FillRectangle(d2d_rect, m_brush.brush.Get());
                }

                if (m_pen.brush){
                    thisCtx->DrawRectangle(d2d_rect, m_pen.brush.Get(),
                                           FLOAT(m_pen.lpen->m_width), m_pen.strokeStyle.Get());
                }
            }
        }
    }
}

void LDirect2DPaintEngine::drawRoundedRect(const LRect<double> &rect, double xRadius, double yRadius, LSizeMode mode)
{
    if(LPainterPath::ajustRounedRectRadius(rect, xRadius, yRadius, mode)){
        if (xRadius <= 0 || yRadius <= 0) {
            drawRects(&rect, 1);
            return;
        }


        ID2D1DeviceContext *thisCtx = dc();
        if(thisCtx != nullptr){
            ensureBrush();
            ensurePen();

            D2D1_ROUNDED_RECT d2d_rouned_rect;
            d2d_rouned_rect.rect = toD2dRectF(rect);
            // ajustRounedRectRadius 输出的是相对尺寸的百分比（相对半径*2），
            // 这里还原为像素半径，与 LPainterPath::addRoundedRect 保持一致
            d2d_rouned_rect.radiusX = FLOAT(rect.m_width * xRadius / 200.0);
            d2d_rouned_rect.radiusY = FLOAT(rect.m_height * yRadius / 200.0);
            if (m_brush.brush){
                thisCtx->FillRoundedRectangle(d2d_rouned_rect, m_brush.brush.Get());
            }

            if (m_pen.brush){
                thisCtx->DrawRoundedRectangle(d2d_rouned_rect, m_pen.brush.Get(),
                                       FLOAT(m_pen.lpen->m_width), m_pen.strokeStyle.Get());
            }
        }
    }
}

void LDirect2DPaintEngine::drawPixmap(const LRect<double> &r, const std::shared_ptr<LPixmap> pm, const LRect<double> &sr)
{
    if (pm == nullptr){
        return;
    }

    std::shared_ptr<LDirect2DPixmap> pp = std::dynamic_pointer_cast<LDirect2DPixmap>(pm->handle());
    if(pp == nullptr){
        return;
    }


    ID2D1DeviceContext *thisCtx = dc();
    if(thisCtx != nullptr){
        ensurePen();

        if (pp != directPixmap()) {
            // Good, src bitmap != dst bitmap
            if (sr.isValid()){
                thisCtx->DrawBitmap(pp->bitmap(),
                                    toD2dRectF(r), FLOAT(state()->opacity),
                                    interpolationMode(),
                                    toD2dRectF(sr));
            }
            else{
                thisCtx->DrawBitmap(pp->bitmap(),
                                    toD2dRectF(r), FLOAT(state()->opacity),
                                    interpolationMode());
            }
        } else {
            // Ok, so the source pixmap and destination pixmap is the same.
            // D2D is not fond of this scenario, deal with it through
            // an intermediate bitmap
            std::shared_ptr<LDirect2DPixmap> intermediate = std::make_shared<LDirect2DPixmap>();

            if (sr.isValid()) {
                if (!intermediate->resize(int(sr.width()), int(sr.height()))) {
                    lWarn() << "Could not resize intermediate bitmap to source rect size";
                    return;
                }

                D2D1_RECT_U d2dSr =  toD2dRectU(sr.toRect());
                HRESULT hr = intermediate->bitmap()->CopyFromBitmap(nullptr,
                                                                   pp->bitmap(),
                                                                   &d2dSr);
                if (FAILED(hr)) {
                    lWarn() << "Could not copy source rect area from source bitmap to intermediate bitmap";
                    return;
                }
            } else {
                if (!intermediate->resize(pp->width(), pp->height())) {
                    lWarn() << "Could not resize intermediate bitmap to source bitmap size";
                    return;
                }

                HRESULT hr = intermediate->bitmap()->CopyFromBitmap(nullptr,
                                                                   pp->bitmap(),
                                                                   nullptr);
                if (FAILED(hr)) {
                    lWarn() << "Could not copy source bitmap to intermediate bitmap";
                    return;
                }
            }

            thisCtx->DrawBitmap(intermediate->bitmap(),
                                toD2dRectF(r), FLOAT(state()->opacity),
                                interpolationMode());
        }
    }

}

void LDirect2DPaintEngine::drawImage(const LRect<double> &r, const std::shared_ptr<LImage> image, const LRect<double> &sr, LFlags<LImageConvertFlag> flags)
{
    std::shared_ptr<LPixmap> pixmap = LPixmap::fromImage(image, flags);
    drawPixmap(r, pixmap, sr);
}


void LDirect2DPaintEngine::drawEllipses(const LRect<int>* rects , int rectCount)
{
    ID2D1DeviceContext *thisCtx = dc();
    if(thisCtx != nullptr){
        ensureBrush();
        ensurePen();
        for (int i = 0; i < rectCount; i++) {
            if(!rects[i].isEmpty()){
                D2D1_ELLIPSE d2d_ellipse = D2D1::Ellipse(toD2dPoint2F(rects[i].center()), rects[i].m_width/2, rects[i].m_height/2);

                if (m_brush.brush){
                    thisCtx->FillEllipse(d2d_ellipse, m_brush.brush.Get());
                }

                if (m_pen.brush){
                    thisCtx->DrawEllipse(d2d_ellipse, m_pen.brush.Get(),
                                           FLOAT(m_pen.lpen->m_width), m_pen.strokeStyle.Get());
                }
            }
        }
    }
}

void LDirect2DPaintEngine::drawEllipses(const LRect<double> *rects, int rectCount)
{
    ID2D1DeviceContext *thisCtx = dc();
    if(thisCtx != nullptr){
        ensureBrush();
        ensurePen();
        for (int i = 0; i < rectCount; i++) {
            if(!rects[i].isEmpty()){
                D2D1_ELLIPSE d2d_ellipse = D2D1::Ellipse(toD2dPoint2F(rects[i].center()), rects[i].m_width/2, rects[i].m_height/2);

                if (m_brush.brush){
                    thisCtx->FillEllipse(d2d_ellipse, m_brush.brush.Get());
                }

                if (m_pen.brush){
                    thisCtx->DrawEllipse(d2d_ellipse, m_pen.brush.Get(),
                                           FLOAT(m_pen.lpen->m_width), m_pen.strokeStyle.Get());
                }
            }
        }
    }
}

void LDirect2DPaintEngine::drawArc(const LRect<double> &r, double a, double alen)
{
    ID2D1DeviceContext *thisCtx = dc();
    if(thisCtx != nullptr){
        ensureBrush();
        ensurePen();

        ID2D1Factory1 *factory = LDirect2DContext::instance()->d2dFactory();
        if(factory ==nullptr){
            return;
        }


        Microsoft::WRL::ComPtr<ID2D1PathGeometry1> pPathGeometry;
        HRESULT hr = factory->CreatePathGeometry(&pPathGeometry);
        if (FAILED(hr)) {
            lWarn() << "Could not create arc geometry";
            return;
        }

        // Open the geometry
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> pSink;
        hr = pPathGeometry->Open(&pSink);
        if (FAILED(hr)) {
            lWarn() <<"Could not create arc sink";
            return;
        }

        double xRadius =  r.m_width/2;
        double yRadius =  r.m_height/2;
        LPoint<double> startPos = LPainterPath::findArcPos(r.center(), xRadius, yRadius, a);

        D2D1_ARC_SEGMENT arcSegment = {
            D2D1::Point2F(startPos.m_x, startPos.m_y), // End point of the arc
            D2D1::SizeF(xRadius, yRadius), // X-radius and Y-radius
            alen, // Rotation angle
            D2D1_SWEEP_DIRECTION_CLOCKWISE,
            D2D1_ARC_SIZE_SMALL
        };

        // Begin the figure and add the arc
        pSink->BeginFigure(D2D1::Point2F(200, 200), D2D1_FIGURE_BEGIN_HOLLOW);
        pSink->AddArc(arcSegment);
        pSink->EndFigure(D2D1_FIGURE_END_OPEN);

        // Close the sink
        hr = pSink->Close();
        thisCtx->DrawGeometry(pPathGeometry.Get(), m_pen.brush.Get(),
                          FLOAT(m_pen.lpen->m_width), m_pen.strokeStyle.Get());
    }
}


void LDirect2DPaintEngine::suspend()
{
    if(m_active){
        end();
        std::shared_ptr<LPaintDevice> p = m_pdev.lock();
        std::shared_ptr<LPixmap> pm = std::dynamic_pointer_cast<LPixmap>(p);
        if(pm != nullptr){
            std::shared_ptr<LDirect2DPixmap> dp =
                std::dynamic_pointer_cast<LDirect2DPixmap>(pm->handle());
            if(dp!= nullptr){
                dp->suspendPaint();
            }
        }
        m_isSuspend = true;
    }
}

void LDirect2DPaintEngine::resume()
{
    if(m_isSuspend){
        std::shared_ptr<LPaintDevice> p = m_pdev.lock();
        std::shared_ptr<LPixmap> pm = std::dynamic_pointer_cast<LPixmap>(p);
        if(pm != nullptr){
            std::shared_ptr<LDirect2DPixmap> dp =
                std::dynamic_pointer_cast<LDirect2DPixmap>(pm->handle());
            if(dp!= nullptr){
                dp->resumePaint();
            }
        }

        begin(p);
        clipEnabledChanged();
        penChanged();
        brushChanged();
        brushOriginChanged();
        opacityChanged();
        compositionModeChanged();
        renderHintsChanged();
        transformChanged();
        m_isSuspend = false;
    }
}

std::shared_ptr<LDirect2DPixmap> LDirect2DPaintEngine::directPixmap()
{
    std::shared_ptr<LPaintDevice> p = m_pdev.lock();
    std::shared_ptr<LPixmap> pm = std::dynamic_pointer_cast<LPixmap>(p);
    if(pm != nullptr){
        std::shared_ptr<LDirect2DPixmap> ret =
            std::dynamic_pointer_cast<LDirect2DPixmap>(pm->handle());
        return ret;
    }
    return nullptr;
}

ID2D1DeviceContext* LDirect2DPaintEngine::dc() const
{
    std::shared_ptr<LPaintDevice> p = m_pdev.lock();
    std::shared_ptr<LPixmap> pm = std::dynamic_pointer_cast<LPixmap>(p);
    if(pm != nullptr){
        std::shared_ptr<LDirect2DPixmap> t =
            std::dynamic_pointer_cast<LDirect2DPixmap>(pm->handle());
        if(t!= nullptr){
            return t->deviceContext();
        }
    }
    return nullptr;
}

static std::vector<D2D1_GRADIENT_STOP> lGradientStopsToD2DStops(const LGradientStops &gstops)
{
    std::vector<D2D1_GRADIENT_STOP> stops(gstops.size());
    LGradientStops::const_iterator iter = gstops.cbegin();
    for(int i = 0; iter != gstops.cend(); ++iter, ++i){
        stops[i].position = iter->first;
        stops[i].color =  toD2dColorF(iter->second);;
    }
    return stops;
}

Microsoft::WRL::ComPtr<ID2D1Brush> LDirect2DPaintEngine::toD2dBrush(const std::shared_ptr<LBrush> newBrush)
{
    HRESULT hr;
    Microsoft::WRL::ComPtr<ID2D1Brush> result;


    bool usedTexture = false;
    switch (newBrush->m_style) {
    case LBrush::NoPattern:{
        break;
    }
    case LBrush::SolidPattern:
    {
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> solid;
        ID2D1DeviceContext* ctx  = dc();
        if(ctx != nullptr){
            hr = dc()->CreateSolidColorBrush(toD2dColorF(newBrush->m_color), &solid);
            if (FAILED(hr)) {
                lWarn() << "Could not create d2d solid color brush";
                break;
            }

            hr = solid.As(&result);
            if (FAILED(hr)){
                lWarn() << "Could not convert d2d solid color brush";
            }
        }
        break;
    }
    case LBrush::TexturePattern:{
        usedTexture = true;
        break;
    }
    case LBrush::GradientPattern:{
        std::shared_ptr<LGradient> g = newBrush->gradient();
        if(g != nullptr){
            if(g->m_spread == LGradient::Spread::Pad){
                switch (g->m_type) {
                case LGradient::Type::Linear:{

                    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> linear;

                    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES linearGradientBrushProperties;
                    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> gradientStopCollection;

                    linearGradientBrushProperties.startPoint.x = g->m_linear.origin.x;
                    linearGradientBrushProperties.startPoint.y = g->m_linear.origin.y;
                    linearGradientBrushProperties.endPoint.x = g->m_linear.end.x;
                    linearGradientBrushProperties.endPoint.y = g->m_linear.end.y;

                    const std::vector<D2D1_GRADIENT_STOP> stops = lGradientStopsToD2DStops(g->m_stops);

                    hr = dc()->CreateGradientStopCollection(stops.data(),
                                                            UINT32(stops.size()),
                                                            &gradientStopCollection);
                    if (FAILED(hr)) {
                        lWarn() << "Could not create gradient stop collection for linear gradient";
                        break;
                    }

                    hr = dc()->CreateLinearGradientBrush(linearGradientBrushProperties,
                                                         gradientStopCollection.Get(),
                                                         &linear);
                    if (FAILED(hr)) {
                        lWarn() << "Could not create Direct2D linear gradient brush";
                        break;
                    }

                    hr = linear.As(&result);
                    if (FAILED(hr)) {
                        lWarn() << "Could not convert Direct2D linear gradient brush";
                        break;
                    }
                    break;
                }
                case LGradient::Type::Radial:{
                    Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush> radial;

                    D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES radialGradientBrushProperties;
                    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> gradientStopCollection;

                    radialGradientBrushProperties.center.x = g->m_radial.center.x;
                    radialGradientBrushProperties.center.y = g->m_radial.center.y;
                    radialGradientBrushProperties.gradientOriginOffset.x = g->m_radial.focal.x - g->m_radial.center.x;
                    radialGradientBrushProperties.gradientOriginOffset.y = g->m_radial.focal.y - g->m_radial.center.y;
                    radialGradientBrushProperties.radiusX = g->m_radial.center.radius;
                    radialGradientBrushProperties.radiusY = g->m_radial.center.radius;

                    const std::vector<D2D1_GRADIENT_STOP> stops = lGradientStopsToD2DStops(g->m_stops);

                    hr = dc()->CreateGradientStopCollection(stops.data(), stops.size(), &gradientStopCollection);
                    if (FAILED(hr)) {
                        lWarn() << "Could not create gradient stop collection for radial gradient";
                        break;
                    }

                    hr = dc()->CreateRadialGradientBrush(radialGradientBrushProperties, gradientStopCollection.Get(),
                                                         &radial);
                    if (FAILED(hr)) {
                        lWarn() << "Could not create Direct2D radial gradient brush";
                        break;
                    }

                    radial.As(&result);
                    if (FAILED(hr)) {
                        lWarn() << "Could not convert Direct2D radial gradient brush";
                        break;
                    }
                    break;
                }
                default:{
                    usedTexture = g->m_type == LGradient::Type::Conical;
                    break;
                }
                }
            }else{
                usedTexture = g->m_type != LGradient::Type::No;
            }
        }
        break;
    }
    default:{
        break;
    }
    }

    if(usedTexture){
        std::shared_ptr<LPixmap> pattern;
        if(newBrush->m_style == LBrush::TexturePattern){
            pattern = newBrush->texture();
        }else{
            std::shared_ptr<LGradient> g = newBrush->gradient();
            std::shared_ptr<LDirect2DPixmap> dp = directPixmap();
            if(dp != nullptr){
                std::shared_ptr<LImage> i= g->toImage(LSize<int>(dp->width(), dp->height()));
                if(i!= nullptr){
                    pattern = LPixmap::fromImage(i);
                }
            }
        }

        if(pattern != nullptr){
            Microsoft::WRL::ComPtr<ID2D1BitmapBrush1> bitmapBrush;
            D2D1_BITMAP_BRUSH_PROPERTIES1 bitmapBrushProperties = {
                D2D1_EXTEND_MODE_WRAP,
                D2D1_EXTEND_MODE_WRAP,
                interpolationMode()
            };

            std::shared_ptr<LDirect2DPixmap> pp = std::dynamic_pointer_cast<LDirect2DPixmap>(pattern->handle());
            if(pp !=  nullptr){
                hr = dc()->CreateBitmapBrush(pp->bitmap(),
                                             bitmapBrushProperties,
                                             &bitmapBrush);

                if (FAILED(hr)) {
                    lWarn() << "Could not create texture brush";
                }else{
                    hr = bitmapBrush.As(&result);
                    if (FAILED(hr)){
                        lWarn() <<"%Could not convert texture brush";
                    }
                }
            }
        }
    }

    if (result && !newBrush->m_transform.isIdentity()){
        result->SetTransform(toD2dMatrix3x2F(newBrush->m_transform));
    }

    return result;
}

D2D1_INTERPOLATION_MODE LDirect2DPaintEngine::interpolationMode() const
{
    std::shared_ptr<LPainterState> s  = state();
    if(s!=nullptr){
        return (s->renderHints & LRenderHint::SmoothPixmapTransform) ? D2D1_INTERPOLATION_MODE_LINEAR
                                                                     : D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
    }else{
        return D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
    }

}

D2D1_ANTIALIAS_MODE LDirect2DPaintEngine::antialiasMode() const
{
    std::shared_ptr<LPainterState> s  = state();
    if(s!=nullptr){
        // 开启 Antialiasing 时应使用逐图元抗锯齿，否则线条/圆角会出现明显锯齿
        if (s->renderHints.testFlag(LRenderHint::Antialiasing)) {
            return D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
        }
        // 完全未设置渲染提示时保持默认抗锯齿，避免整体绘制变粗糙
        if (!s->renderHints.testFlag(LRenderHint::TextAntialiasing)
            && !s->renderHints.testFlag(LRenderHint::SmoothPixmapTransform)
            && !s->renderHints.testFlag(LRenderHint::LosslessImageRendering)) {
            return D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
        }
        return D2D1_ANTIALIAS_MODE_ALIASED;
    }else{
        return D2D1_ANTIALIAS_MODE_ALIASED;
    }
}

D2D1_LAYER_OPTIONS1 LDirect2DPaintEngine::layerOptions() const
{
    if (m_dxEngineFlags & LDirect2DPaintEngine::TranslucentTopLevelWindow){
        return D2D1_LAYER_OPTIONS1_NONE;
    }else{
        return D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND;
    }
}

void LDirect2DPaintEngine::ensureBrush()
{
    std::shared_ptr<LPainterState> s  = state();
    if(s!=nullptr){
        ensureBrush(s->brush);
    }
}

void LDirect2DPaintEngine::ensureBrush(const std::shared_ptr<LBrush> newBrush)
{
    if (m_brush.brush!=nullptr ){
        if(newBrush == m_brush.lbrush){
#ifdef _DX2_CHECK_BACK_BRUSH_PEN
            if(*m_brush.backBrush == *newBrush){
                return;
            }else{
                lWarn() << "Brush change unexpect!";
            }
#else
            return;
#endif
        }else{
            if(newBrush !=nullptr && m_brush.lbrush!= nullptr){
                if(*m_brush.lbrush == *newBrush){
                    return;
                }
            }
        }
    }

    m_brush.reset();
    if(newBrush==nullptr){
        return;
    }else{
        if(newBrush->m_style == LBrush::NoPattern){
            return;
        }

        m_brush.lbrush = newBrush;
#ifdef _DX2_CHECK_BACK_BRUSH_PEN
        m_brush.backBrush= newBrush->backBrush(); //复制一份，保证之后数据更改会更新
#endif
    }

    m_brush.brush = toD2dBrush(m_brush.lbrush);
    if (m_brush.brush) {
        std::shared_ptr<LPainterState> s  = state();
        if(s!=nullptr){
            m_brush.brush->SetOpacity(FLOAT(s->opacity));
        }
        applyBrushOrigin(m_currentBrushOrigin);
    }
}

void LDirect2DPaintEngine::ensurePen()
{
    std::shared_ptr<LPainterState> s  = state();
    if(s!=nullptr){
        ensurePen(s->pen);
    }
}

void LDirect2DPaintEngine::ensurePen(const std::shared_ptr<LPen> newPen)
{
    if (m_pen.lpen!=nullptr){
        if(newPen == m_pen.lpen){
#ifdef _DX2_CHECK_BACK_BRUSH_PEN
            if(*m_pen.backPen == *newPen){
                return;
            }else{
                lWarn() << "Pen change unexpect! back:"
                        << m_pen.backPen->color().name()
                        << ", new: " << newPen->color().name();
            }
#else
            return;
#endif
        }else{
            if(newPen !=nullptr && m_pen.lpen!= nullptr){
                if(*m_pen.lpen == *newPen){
                    return;
                }
            }
        }
    }

    m_pen.reset();
    if(newPen==nullptr){
        return;
    }else{
        if (newPen->m_style ==LPenStyle::No){
            return;
        }
        m_pen.lpen = newPen;
#ifdef _DX2_CHECK_BACK_BRUSH_PEN
        m_pen.backPen= newPen->backPen(); //复制一份，保证之后数据更改会更新
#endif
    }

    m_pen.brush = toD2dBrush(m_pen.lpen->brush());
    if (m_pen.brush == nullptr){
        return;
    }

    std::shared_ptr<LPainterState> s  = state();
    if(s!=nullptr){
        m_pen.brush->SetOpacity(FLOAT(s->opacity));
    }

    D2D1_STROKE_STYLE_PROPERTIES1 props = {};

    switch (m_pen.lpen->m_capStyle) {
    case LPenCapStyle::Square:{
        props.startCap = props.endCap = props.dashCap = D2D1_CAP_STYLE_SQUARE;
        break;
    }
    case LPenCapStyle::Round:{
        props.startCap = props.endCap = props.dashCap = D2D1_CAP_STYLE_ROUND;
        break;
    }
    case LPenCapStyle::Flat:{
    default:
        props.startCap = props.endCap = props.dashCap = D2D1_CAP_STYLE_FLAT;
        break;
    }
    }

    switch (m_pen.lpen->m_joinStyle) {
    case LPenJoinStyle::Bevel:
        props.lineJoin = D2D1_LINE_JOIN_BEVEL;
        break;
    case LPenJoinStyle::Round:
        props.lineJoin = D2D1_LINE_JOIN_ROUND;
        break;
    case LPenJoinStyle::Miter:
    default:
        props.lineJoin = D2D1_LINE_JOIN_MITER;
        break;
    }

    props.miterLimit = FLOAT(m_pen.lpen->m_miterLimit * 2.0); // D2D and Qt miter specs differ
    props.dashOffset = FLOAT(m_pen.lpen->m_dashOffset);

    if (lFuzzyIsZero(m_pen.lpen->m_width)){
        props.transformType = D2D1_STROKE_TRANSFORM_TYPE_HAIRLINE;
    }else if (m_pen.lpen->isCosmetic()){
        props.transformType = D2D1_STROKE_TRANSFORM_TYPE_FIXED;
    }else{
        props.transformType = D2D1_STROKE_TRANSFORM_TYPE_NORMAL;
    }

    switch (m_pen.lpen->m_style) {
    case LPenStyle::Solid:{
        props.dashStyle = D2D1_DASH_STYLE_SOLID;
        break;
    }
    case LPenStyle::Dot:
    case LPenStyle::DashDot:
    case LPenStyle::DashDotDot:{
        if (m_pen.lpen->m_width <= 1.0){
            props.startCap = props.endCap = props.dashCap = D2D1_CAP_STYLE_FLAT;
        }
        props.dashStyle = D2D1_DASH_STYLE_CUSTOM;
        break;
    }
    default:{
        props.dashStyle = D2D1_DASH_STYLE_CUSTOM;
        break;
    }
    }

    HRESULT hr;

    if (props.dashStyle == D2D1_DASH_STYLE_CUSTOM) {
        ID2D1Factory1 *factory = LDirect2DContext::instance()->d2dFactory();
        ID2D1DeviceContext *thisCtx = dc();
        if(factory != nullptr && thisCtx != nullptr){

            std::vector<double> dashes = m_pen.lpen->dashPattern();
            std::vector<FLOAT> converted(dashes.size());
            double penWidth = m_pen.lpen->m_width;
            double brushWidth = 0;
            for (uint32_t i = 0; i < dashes.size(); i++) {
                converted[i] = FLOAT(dashes[i]);
                brushWidth += penWidth * dashes[i];
            }

            hr = factory->CreateStrokeStyle(props, converted.data(), UINT32(converted.size()), &m_pen.strokeStyle);

            // Create a combined brush/dash pattern for optimized line drawing
            LDirect2DPixmap pixmap;
            pixmap.resize(int(ceil(brushWidth)), int(ceil(penWidth)));
            //
            pixmap.beginPaint();
            ID2D1DeviceContext *ctx = pixmap.deviceContext();
            ctx->SetAntialiasMode(antialiasMode());
            ctx->SetTransform(D2D1::IdentityMatrix());
            ctx->Clear();
            const double offsetX = (pixmap.width() - brushWidth) / 2.0;
            const double offsetY = pixmap.height() / 2.0;
            ctx->DrawLine(D2D1::Point2F(FLOAT(offsetX), FLOAT(offsetY)),
                                        D2D1::Point2F(FLOAT(brushWidth), FLOAT(offsetY)),
                                        m_pen.brush.Get(), FLOAT(penWidth), m_pen.strokeStyle.Get());
            pixmap.endPaint();
            D2D1_BITMAP_BRUSH_PROPERTIES1 bitmapBrushProperties = D2D1::BitmapBrushProperties1(
                D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE_CLAMP, D2D1_INTERPOLATION_MODE_LINEAR);
            hr = thisCtx->CreateBitmapBrush(pixmap.bitmap(), bitmapBrushProperties, &m_pen.dashBrush);
            m_pen.dashLength = pixmap.width();
        }
    } else {
        ID2D1Factory1 *factory = LDirect2DContext::instance()->d2dFactory();
        if(factory){
            hr = factory->CreateStrokeStyle(props, nullptr, 0, &m_pen.strokeStyle);
        }
    }

    if (FAILED(hr)){
        lWarn() << "Could not create stroke style";
    }

}

void LDirect2DPaintEngine::updateBrushOrigin(const LPoint<double> &brushOrigin)
{
    negateCurrentBrushOrigin();
    applyBrushOrigin(brushOrigin);
}

void LDirect2DPaintEngine::applyBrushOrigin(const LPoint<double> &origin)
{
    if (m_brush.brush && !origin.isNull()) {
        D2D1_MATRIX_3X2_F transform;
        m_brush.brush->GetTransform(&transform);
        m_brush.brush->SetTransform(*(D2D1::Matrix3x2F::ReinterpretBaseType(&transform))
                                  * D2D1::Matrix3x2F::Translation(FLOAT(origin.m_x), FLOAT(origin.m_y)));
    }
    m_currentBrushOrigin = origin;
}

void LDirect2DPaintEngine::negateCurrentBrushOrigin()
{
    if (m_brush.brush && !m_currentBrushOrigin.isNull()) {
        D2D1_MATRIX_3X2_F transform;
        m_brush.brush->GetTransform(&transform);

        m_brush.brush->SetTransform(*(D2D1::Matrix3x2F::ReinterpretBaseType(&transform))
                                  * D2D1::Matrix3x2F::Translation(FLOAT(-m_currentBrushOrigin.m_x),
                                                                  FLOAT(-m_currentBrushOrigin.m_y)));
    }
}

void LDirect2DPaintEngine::clearClips()
{
    ID2D1DeviceContext *thisCtx = dc();
    if(thisCtx != nullptr){
        while (!m_pushedClips.empty()) {
            switch (m_pushedClips.top()) {
            case AxisAlignedClip:
                thisCtx->PopAxisAlignedClip();
                break;
            case LayerClip:
                thisCtx->PopLayer();
                break;
            }

            m_pushedClips.pop();
        }
    }else{
        while (!m_pushedClips.empty()) {
            m_pushedClips.pop();
        }
    }
}

void LDirect2DPaintEngine::pushClip(const std::shared_ptr<LPainterPath> path)
{
    ID2D1DeviceContext *thisCtx = dc();
    if(thisCtx != nullptr){
        if (path == nullptr || path->isEmpty()) {
            D2D_RECT_F rect = {0, 0, 0, 0};
            thisCtx->PushAxisAlignedClip(rect, antialiasMode());
            m_pushedClips.push(AxisAlignedClip);
            return;
        }

        std::shared_ptr<LPainterState> s= state();
        if (path->isRect() && (s== nullptr || s->matrix.type() <= LTransform2D::TxScale)) {
            const std::vector<LPainterPath::Element>& eles = path->elements();
            D2D_RECT_F rect = {
                FLOAT(eles[0].m_x), // left
                FLOAT(eles[0].m_y), // top
                FLOAT(eles[2].m_x), // right,
                FLOAT(eles[2].m_y)  // bottom
            };

            thisCtx->PushAxisAlignedClip(rect, antialiasMode());
            m_pushedClips.push(AxisAlignedClip);
            return;
        }


        Microsoft::WRL::ComPtr<ID2D1PathGeometry1> geometry = painterPathToDxPath(path);
        if (!geometry) {
            lWarn() <<"Could not convert painter path to dx path!";
            return;
        }

        thisCtx->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(),
                                               geometry.Get(),
                                               antialiasMode(),
                                               D2D1::IdentityMatrix(),
                                               1.0,
                                               nullptr,
                                               layerOptions()),
                        nullptr);
        m_pushedClips.push(LayerClip);
    }
}


Microsoft::WRL::ComPtr<ID2D1PathGeometry1> LDirect2DPaintEngine::painterPathToDxPath(const std::shared_ptr<LPainterPath> path)
{

    Microsoft::WRL::ComPtr<ID2D1PathGeometry1> geometry;

    if (path != nullptr) {
        ID2D1Factory1 *factory = LDirect2DContext::instance()->d2dFactory();
        if(factory ==nullptr){
            return geometry;
        }

        HRESULT hr = factory->CreatePathGeometry(&geometry);
        if (FAILED(hr)) {
            lWarn() << "Could not create path geometry";
            return geometry;
        }

        Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
        hr = geometry->Open(&sink);
        if (FAILED(hr)) {
            lWarn() <<"Could not create geometry sink";
            return geometry;
        }

        if(path->fillRule() == LFillRule::Winding){
            sink->SetFillMode(D2D1_FILL_MODE_WINDING);
        }else{
            sink->SetFillMode(D2D1_FILL_MODE_ALTERNATE);
        }

        const std::vector<LPainterPath::Element>& eles = path->elements();
        bool inFigure = false;
        for (uint32_t i = 0; i < eles.size(); ++i) {
            const LPainterPath::Element& ele = eles.at(i);
            switch (ele.m_type) {
            case LPainterPath::MoveToElement:{
                if (inFigure){ //路径转为轮廓，需要关闭曲线，以保证填充奇偶正确
                    sink->EndFigure(D2D1_FIGURE_END_OPEN);
                }

                sink->BeginFigure(D2D1::Point2F(ele.m_x, ele.m_y), D2D1_FIGURE_BEGIN_FILLED);
                inFigure = true;
                break;
            }
            case LPainterPath::LineToElement:{
                sink->AddLine(D2D1::Point2F(ele.m_x, ele.m_y));
                break;
            }
            case LPainterPath::CurveToElement:
            {
                if(i+2 < eles.size()){
                    D2D1_POINT_2F p1 = D2D1::Point2F(ele.m_x, ele.m_y);
                    const LPainterPath::Element& ele2 = eles.at(++i);
                    const LPainterPath::Element& ele3 = eles.at(++i);

                    D2D1_POINT_2F p2 = D2D1::Point2F(ele2.m_x, ele2.m_y);
                    D2D1_POINT_2F p3 = D2D1::Point2F(ele3.m_x, ele3.m_y);

                    D2D1_BEZIER_SEGMENT segment = {
                        p1,
                        p2,
                        p3
                    };
                    sink->AddBezier(segment);
                }
                break;
            }
            case LPainterPath::CurveToDataElement:{
                break;
            }
            }
        }

        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();
        return geometry;
    }
    return nullptr;
}

Microsoft::WRL::ComPtr<ID2D1PathGeometry1> LDirect2DPaintEngine::joinPath(LPoint<double> &p1, LPoint<double> &p2, LPoint<double> &p3)
{
    Microsoft::WRL::ComPtr<ID2D1PathGeometry1> geometry;
    ID2D1Factory1 *factory = LDirect2DContext::instance()->d2dFactory();
    if(factory ==nullptr){
        return geometry;
    }

    HRESULT hr = factory->CreatePathGeometry(&geometry);
    if (FAILED(hr)) {
        lWarn() << "Could not create path geometry";
        return geometry;
    }

    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    hr = geometry->Open(&sink);
    if (FAILED(hr)) {
        lWarn() <<"Could not create geometry sink";
        return geometry;
    }

    sink->BeginFigure(D2D1::Point2F(p1.m_x, p1.m_y), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(p2.m_x, p2.m_y));
    sink->AddLine(D2D1::Point2F(p3.m_x, p3.m_y));
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    sink->Close();

    return geometry;
}

void LDirect2DPaintEngine::updateClipEnabled(bool enabled)
{
    if (!enabled){
        clearClips();
    }
    else if (m_pushedClips.empty()){
        replayClipOperations();
    }
}

void LDirect2DPaintEngine::updateOpacity(double opacity)
{
    if (m_brush.brush){
        m_brush.brush->SetOpacity(FLOAT(opacity));
    }
    if (m_pen.brush){
        m_pen.brush->SetOpacity(FLOAT(opacity));
    }
}

void LDirect2DPaintEngine::updateCompositionMode(LCompositionMode mode)
{
    ID2D1DeviceContext *thisCtx = dc();
    if(thisCtx != nullptr){
        switch (mode) {
        case LCompositionMode::Source:{
            thisCtx->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
            break;
        }
        case LCompositionMode::SourceOver:{
            thisCtx->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
            break;
        }
        default:
            thisCtx->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
            // m_dxEngineFlags |= LDirect2DPaintEngine::EmulateComposition;
            break;
        }
    }
}

void LDirect2DPaintEngine::updateTransform(const LTransform2D &transform)
{
    ID2D1DeviceContext *thisCtx = dc();
    if(thisCtx != nullptr){
        thisCtx->SetTransform(toD2dMatrix3x2F(transform));
    }
}

void LDirect2DPaintEngine::updateHints()
{
    ID2D1DeviceContext *thisCtx = dc();
    if(thisCtx != nullptr){
        thisCtx->SetAntialiasMode(antialiasMode());
    }
}

Microsoft::WRL::ComPtr<IDWriteFontFace> LDirect2DPaintEngine::fontFaceFromFontEngine(std::shared_ptr<LFontEngine> fe)
{
    const LFont::Def& fontDef = fe->m_fontDef;
    auto it = m_fontCache.find(fontDef);
    if (it != m_fontCache.end()){
        return it->second;
    }

    Microsoft::WRL::ComPtr<IDWriteFontFace> fontFace;
    LOGFONTW lf = LWindowsFontDatabase::fontDefToLOGFONT(fontDef);

    const std::wstring familyName = lf.lfFaceName;
    LWinRegistry reg;
    const std::wstring nameSubstitute = reg.getString(HKEY_LOCAL_MACHINE,
                                                      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\FontSubstitutes",
                                                      familyName);
    if(!nameSubstitute.empty()){
        if (nameSubstitute != familyName) {
            const int nameSubstituteLength = std::min<int>(nameSubstitute.length(), LF_FACESIZE - 1);
            memcpy(lf.lfFaceName, nameSubstitute.data(), size_t(nameSubstituteLength) * sizeof(wchar_t));
            lf.lfFaceName[nameSubstituteLength] = 0;
        }
    }

    Microsoft::WRL::ComPtr<IDWriteFont> dwriteFont;
    HRESULT hr = LDirect2DContext::instance()->dwriteGdiInterop()->CreateFontFromLOGFONT(&lf, &dwriteFont);
    if (FAILED(hr)) {
        lWarn() << "CreateFontFromLOGFONT failed";
        return fontFace;
    }

    hr = dwriteFont->CreateFontFace(&fontFace);
    if (FAILED(hr)) {
        lWarn() << "CreateFontFace failed";
        return fontFace;
    }

    if (fontFace!= nullptr){
        m_fontCache[fontDef] =  fontFace;
    }

    return fontFace;
}

void LDirect2DPaintEngine::drawGlyphRun(const D2D1_POINT_2F &pos,
                                        IDWriteFontFace *fontFace,
                                        const LFont::Def &fontDef,
                                        int numGlyphs,
                                        const UINT16 *glyphIndices,
                                        const FLOAT *glyphAdvances,
                                        const DWRITE_GLYPH_OFFSET *glyphOffsets,
                                        bool rtl)
{

    ID2D1DeviceContext *thisCtx = dc();
    if(thisCtx != nullptr){
        DWRITE_GLYPH_RUN glyphRun = {
            fontFace,          //    IDWriteFontFace           *fontFace;
            FLOAT(fontDef.pixelSize), // FLOAT                 fontEmSize;
            UINT32(numGlyphs), //    UINT32                    glyphCount;
            glyphIndices,      //    const UINT16              *glyphIndices;
            glyphAdvances,     //    const FLOAT               *glyphAdvances;
            glyphOffsets,      //    const DWRITE_GLYPH_OFFSET *glyphOffsets;
            FALSE,             //    BOOL                      isSideways;
            rtl ? 1u : 0u      //    UINT32                    bidiLevel;
        };

        bool antiAlias = !(fontDef.styleStrategy & LFont::NoAntialias);
        if(antiAlias){
            std::shared_ptr<LPainterState> s  = state();
            if(s!=nullptr){
                if(!(s->renderHints  & LRenderHint::TextAntialiasing) ){
                    antiAlias = false;
                }
            }
        }
        const D2D1_TEXT_ANTIALIAS_MODE antialiasMode = (m_dxEngineFlags & LDirect2DPaintEngine::TranslucentTopLevelWindow)
                                                           ? D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE : D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE;
        thisCtx->SetTextAntialiasMode(antiAlias ? antialiasMode : D2D1_TEXT_ANTIALIAS_MODE_ALIASED);
        thisCtx->DrawGlyphRun(pos,
                           &glyphRun,
                           nullptr,
                           m_pen.brush.Get(),
                           DWRITE_MEASURING_MODE_GDI_CLASSIC);
    }
}



LK_NAMESPACE_END


