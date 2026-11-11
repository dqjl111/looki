#include "lrasterpaintengine.h"
#include "lkcore/ldebug.h"
#include "lkgui/text/lfontengine.h"
#include "lkgui/paint/lglyphmetrics.h"
#include <shared_mutex>

LK_NAMESPACE_BEGIN
static const int LK_RASTER_COORD_LIMIT = 32767;
static inline int fastCeilPositive(const double &v)
{
    const int iv = int(v);
    if (v - iv == 0){
        return iv;
    }else{
        return iv + 1;
    }
}

static inline const LRect<int> toAlignedRectPositive(const LRect<double> &rect)
{
    const int xmin = int(rect.m_x);
    const int xmax = int(fastCeilPositive(rect.x2()));
    const int ymin = int(rect.m_y);
    const int ymax = int(fastCeilPositive(rect.y2()));
    return LRect<int>(xmin, ymin, xmax - xmin, ymax - ymin, false);
}

static int lIntersectSpans(LSpan *&spans, int numSpans, const LRect<int> &clip)
{
    const short minx = clip.m_x;
    const short miny = clip.m_y;
    const short maxx = clip.x2();
    const short maxy = clip.y2();

    LSpan *end = spans + numSpans;
    while (spans < end) {
        if (spans->y >= miny){
            break;
        }
        ++spans;
    }

    LSpan *s = spans;
    while (s < end) {
        if (s->y > maxy){
            break;
        }
        if (s->x > maxx || s->x + s->len <= minx) {
            s->len = 0;
            ++s;
            continue;
        }
        if (s->x < minx) {
            s->len = std::min<int>(s->len - (minx - s->x), maxx - minx + 1);
            s->x = minx;
        } else {
            s->len = std::min<int>(s->len, uint16_t(maxx - s->x + 1));
        }
        ++s;
    }

    return s - spans;
}


 static void lSpanFillClipRect(int count, const LSpan *spans,
                          void *userData)
{
    LSpanData *fillData = reinterpret_cast<LSpanData *>(userData);
    LSpan *s = const_cast<LSpan *>(spans);
    // hw: check if this const_cast<> is safe!!!
    count = lIntersectSpans(s, count, fillData->clip->clipRect);
    if (count > 0){
        fillData->unclippedBlend(count, s, fillData);
    }
}

static std::pair<int, int> visibleGlyphRange(const LRect<double> &clip,
                                             std::shared_ptr<LFontEngine> fontEngine,
                                             uint32_t *glyphs,
                                             LPoint<double> *positions,
                                             int numGlyphs)
{
    double clipLeft = clip.m_x;
    double clipRight = clip.x2();
    double clipTop = clip.m_y;
    double clipBottom = clip.y2();

    int first = 0;
    while (first < numGlyphs) {
        LGlyphMetrics<double> metrics = fontEngine->boundingBox(glyphs[first]);
        double left = metrics.m_x + positions[first].m_x;
        double top = metrics.m_y + positions[first].m_y;
        double right = left + metrics.m_width;
        double bottom = top + metrics.m_height;
        if (left < clipRight && right > clipLeft && top < clipBottom && bottom > clipTop){
            break;
        }
        ++first;
    }
    int last = numGlyphs - 1;
    while (last > first) {
        LGlyphMetrics<double> metrics = fontEngine->boundingBox(glyphs[last]);
        double left = metrics.m_x + positions[last].m_x;
        double top = metrics.m_y + positions[last].m_y;
        double right = left + metrics.m_width;
        double bottom = top + metrics.m_height;
        if (left < clipRight && right > clipLeft && top < clipBottom && bottom > clipTop){
            break;
        }
        --last;
    }
    return std::pair<int, int>(first, last + 1);
}

inline LRect<int> mapFillRect(const LRect<double> &rect, const LTransform2D &xf)
{
    LPoint<int> p1 = LPoint<double>(xf.map(rect.topLeft())).toPoint();
    LPoint<int> p2 = LPoint<double>(xf.map(rect.bottomRight())).toPoint();
    if (L_UNLIKELY(p1.m_x == p2.m_x || p1.m_y == p2.m_y)){
        return LRect<int>();
    }

    return LRect<int>(LPoint<int>(std::min<int>(p1.m_x, p2.m_x), std::min<int>(p1.m_y, p2.m_y)),
                 LPoint<int>(std::min<int>(p1.m_x, p2.m_x), std::min<int>(p1.m_y, p2.m_y)));
}

LSpanClipData::LSpanClipData(int height)
{
    clipSpanHeight = height;
    m_clipLines = nullptr;

    allocated = 0;
    m_spans = nullptr;
    xmin = xmax = ymin = ymax = 0;
    count = 0;

    enabled = true;
    hasRectClip = hasRegionClip = false;
}

LSpanClipData::~LSpanClipData()
{
    if (m_clipLines){
        free(m_clipLines);
    }
    if (m_spans){
        free(m_spans);
    }
}

void LSpanClipData::initialize()
{
    if (m_spans){
        return;
    }

    if (!m_clipLines){
        m_clipLines = (ClipLine *)calloc(sizeof(ClipLine), clipSpanHeight);
    }

    L_CHECK_PTR(m_clipLines);
    try {
        allocated = clipSpanHeight;
        try {
            if (hasRegionClip) {
                if(clipRegion == nullptr){
                    clipRegion = std::make_shared<LRegion>();
                }

                const auto rects = clipRegion->begin();
                const int numRects = clipRegion->rectCount();
                const int maxSpans = (ymax - ymin) * numRects;
                allocated = std::max<int>(allocated, maxSpans);
                m_spans = (LSpan *)malloc(allocated * sizeof(LSpan));
                L_CHECK_PTR(m_spans);

                int y = 0;
                int firstInBand = 0;
                count = 0;
                while (firstInBand < numRects) {
                    const int currMinY = rects[firstInBand].m_y;
                    const int currMaxY = currMinY + rects[firstInBand].height();

                    while (y < currMinY) {
                        m_clipLines[y].spans = nullptr;
                        m_clipLines[y].count = 0;
                        ++y;
                    }

                    int lastInBand = firstInBand;
                    while (lastInBand + 1 < numRects && rects[lastInBand+1].m_y == y){
                        ++lastInBand;
                    }

                    while (y < currMaxY) {

                        m_clipLines[y].spans = m_spans + count;
                        m_clipLines[y].count = lastInBand - firstInBand + 1;

                        for (int r = firstInBand; r <= lastInBand; ++r) {
                            const LRect<int> &currRect = rects[r];
                            LSpan *span = m_spans + count;
                            span->x = currRect.m_x;
                            span->len = currRect.width();
                            span->y = y;
                            span->coverage = 255;
                            ++count;
                        }
                        ++y;
                    }

                    firstInBand = lastInBand + 1;
                }

                //std::assert(count <= allocated);

                while (y < clipSpanHeight) {
                    m_clipLines[y].spans = nullptr;
                    m_clipLines[y].count = 0;
                    ++y;
                }

                return;
            }

            m_spans = (LSpan *)malloc(allocated * sizeof(LSpan));
            L_CHECK_PTR(m_spans);

            if (hasRectClip) {
                int y = 0;
                while (y < ymin) {
                    m_clipLines[y].spans = nullptr;
                    m_clipLines[y].count = 0;
                    ++y;
                }

                const int len = clipRect.width();
                count = 0;
                while (y < ymax) {
                    LSpan *span = m_spans + count;
                    span->x = xmin;
                    span->len = len;
                    span->y = y;
                    span->coverage = 255;
                    ++count;

                    m_clipLines[y].spans = span;
                    m_clipLines[y].count = 1;
                    ++y;
                }

                while (y < clipSpanHeight) {
                    m_clipLines[y].spans = nullptr;
                    m_clipLines[y].count = 0;
                    ++y;
                }
            }
        } catch(...) {
            free(m_spans); // have to free m_spans again or someone might think that we were successfully initialized.
            m_spans = nullptr;
        }
    } catch(...) {
        free(m_clipLines); // same for clipLines
        m_clipLines = nullptr;
    }
}

void LSpanClipData::appendSpan(int x, int length, int y, int coverage)
{
    if (count == allocated) {
        allocated *= 2;
        m_spans = (LSpan *)realloc(m_spans, allocated*sizeof(LSpan));
    }
    m_spans[count].x = x;
    m_spans[count].len = length;
    m_spans[count].y = y;
    m_spans[count].coverage = coverage;
    ++count;
}

void LSpanClipData::appendSpans(const LSpan *s, int num)
{
    if (count + num > allocated) {
        do {
            allocated *= 2;
        } while (count + num > allocated);
        m_spans = (LSpan *)realloc(m_spans, allocated*sizeof(LSpan));
    }
    memcpy(m_spans+count, s, num*sizeof(LSpan));
    count += num;
}

void LSpanClipData::setClipRect(const LRect<int> &rect)
{
    if (hasRectClip && rect == clipRect){
        return;
    }

    hasRectClip = true;
    hasRegionClip = false;
    clipRect = rect;

    xmin = rect.m_x;
    xmax = rect.m_x + rect.width();
    ymin = std::min<int>(rect.m_y, clipSpanHeight);
    ymax = std::min<int>(rect.m_y + rect.height(), clipSpanHeight);

    if (m_spans) {
        free(m_spans);
        m_spans = nullptr;
    }
}

void LSpanClipData::setClipRegion(const std::shared_ptr<LRegion> region)
{
    if (region->rectCount() == 1) {
        setClipRect(region->boundingRect());
        return;
    }

    hasRegionClip = true;
    hasRectClip = false;
    clipRegion = region;

    { // set bounding rect
        const LRect<int> rect = region->boundingRect();
        xmin = rect.m_x;
        xmax = rect.m_x + rect.width();
        ymin = rect.m_y;
        ymax = rect.m_y + rect.height();
    }

    if (m_spans) {
        free(m_spans);
        m_spans = nullptr;
    }
}

void LSpanClipData::fixup()
{
    if (count == 0) {
        ymin = ymax = xmin = xmax = 0;
        return;
    }

    int y = -1;
    ymin = m_spans[0].y;
    ymax = m_spans[count-1].y + 1;
    xmin = INT_MAX;
    xmax = 0;

    const int firstLeft = m_spans[0].x;
    const int firstRight = m_spans[0].x + m_spans[0].len;
    bool isRect = true;

    for (int i = 0; i < count; ++i) {
        LSpan& span = m_spans[i];

        if (span.y != y) {
            if (span.y != y + 1 && y != -1){
                isRect = false;
            }
            y = span.y;
            m_clipLines[y].spans = &span;
            m_clipLines[y].count = 1;
        } else{
            ++m_clipLines[y].count;
        }

        const int spanLeft = span.x;
        const int spanRight = spanLeft + span.len;

        if (spanLeft < xmin){
            xmin = spanLeft;
        }

        if (spanRight > xmax){
            xmax = spanRight;
        }

        if (spanLeft != firstLeft || spanRight != firstRight){
            isRect = false;
        }
    }

    if (isRect) {
        hasRectClip = true;
        clipRect.setRect(xmin, ymin, xmax - xmin, ymax - ymin);
    }
}

void LSpanData::init(LRasterBuffer *rb, const LRasterPaintEngine *pe)
{
    rasterBuffer = rb;
    type = None;
    txop = 0;
    bilinear = false;
    m11 = m22 = m33 = 1.;
    m12 = m13 = m21 = m23 = dx = dy = 0.0;
    clip = pe ? pe->clip() : nullptr;
}



void LSpanData::setup(const std::shared_ptr<LBrush> brush, uint8_t opacity, LCompositionMode compositionMode)
{
    if (brush == nullptr) {
        type = None;
        adjustSpanMethods();
        return;
    }

    switch (brush->m_style) {
    case LBrush::SolidPattern: {
        type = Solid;
        LColor c = brush->m_color;
        c.m_alpha = std::lround(c.m_alpha * (opacity/255.0));
        solidColor = LColor::premultiply(c.toUInt());
        if (LColor::isTransparent(solidColor)
            && compositionMode == LCompositionMode::SourceOver){
            type = None;
        }
        adjustSpanMethods();
        break;
    }
    case LBrush::TexturePattern:{
        tempImage = brush->textureImage();
        if (tempImage != nullptr){
            type = Texture;
            LColor c = brush->m_color;
            c.m_alpha = std::lround(c.m_alpha * (opacity/255.0));
            initTexture(tempImage, c.m_alpha, LTextureData::Tiled, tempImage->rect());
        }else{
            type = None;
            adjustSpanMethods();
        }
        break;
    }
    case LBrush::GradientPattern:
    {
        gradient = brush->gradient();
        if(gradient != nullptr){
            type = Gradient;
            bool hasAlpha = !brush->isOpaque() || opacity != (uint8_t)255;
            if(gradient->m_hasAlpha != hasAlpha){
                gradient->m_hasAlpha = hasAlpha;
                gradient->m_dirty = true;
            }
            gradient->ensureColorTable();
            adjustSpanMethods();
        }else{
            type = None;
            adjustSpanMethods();
        }
        break;
    }
    default:{
        type = None;
        adjustSpanMethods();
        break;
    }
    }

}

void LSpanData::setupMatrix(const LTransform2D &matrix, int bilin)
{
    LTransform2D delta;
    // make sure we round off correctly in qdrawhelper.cpp
    delta.translate(1.0 / 65536, 1.0 / 65536);

    LTransform2D inv = (delta * matrix).inverted();
    m11 = inv.m11();
    m12 = inv.m12();
    m13 = inv.m13();
    m21 = inv.m21();
    m22 = inv.m22();
    m23 = inv.m23();
    m33 = inv.m33();
    dx = inv.dx();
    dy = inv.dy();
    txop = inv.type();
    bilinear = bilin;

    const bool affine = inv.isAffine();
    const double f1 = m11 * m11 + m21 * m21;
    const double f2 = m12 * m12 + m22 * m22;
    fastMatrix = affine
                  && f1 < 1e4
                  && f2 < 1e4
                  && f1 > (1.0 / 65536)
                  && f2 > (1.0 / 65536)
                  && std::abs(dx) < 1e4
                  && std::abs(dy) < 1e4;

    adjustSpanMethods();
}

void LSpanData::initTexture(const std::shared_ptr<LImage> image,
                            uint8_t alpha,
                            LTextureData::Type type,
                            const LRect<int> &sourceRect)
{
    ensureTexture();
    if (image==nullptr || image->height() == 0) {
        texture->imageData = nullptr;
        texture->width = 0;
        texture->height = 0;
        texture->x1 = 0;
        texture->y1 = 0;
        texture->x2 = 0;
        texture->y2 = 0;
        texture->bytesPerLine = 0;
        texture->format = LImage::Format::Invalid;
        texture->colorTable = nullptr;
        texture->hasAlpha = alpha != 255;
    } else {
        texture->imageData = image->bits();
        texture->width = image->width();
        texture->height = image->height();

        if (sourceRect.isNull()) {
            texture->x1 = 0;
            texture->y1 = 0;
            texture->x2 = texture->width;
            texture->y2 = texture->height;
        } else {
            texture->x1 = sourceRect.m_x;
            texture->y1 = sourceRect.m_y;
            texture->x2 = std::min<int>(texture->x1 + sourceRect.width(), image->width());
            texture->y2 = std::min<int>(texture->y1 + sourceRect.height(), image->height());
        }

        texture->bytesPerLine = image->bytesPerLine();
        texture->format = image->format();
        texture->colorTable = (image->format() <= LImage::Format::Indexed8 && !image->colorTable().empty()) ? &image->colorTable(): nullptr;
        texture->hasAlpha = image->hasAlphaChannel() || alpha != 255;
    }
    texture->constAlpha = alpha;
    texture->type = type;

    adjustSpanMethods();
}

void LSpanData::adjustSpanMethods()
{
    bitmapBlit = nullptr;
    alphamapBlit = nullptr;
    alphaRGBBlit = nullptr;

    fillRect = nullptr;

    switch(type) {
    case None:{
        unclippedBlend = nullptr;
        break;
    }
    case Solid: {
        const LDrawHelper &drawHelper = lDrawHelper[(int)rasterBuffer->format];
        unclippedBlend = drawHelper.blendColor;
        bitmapBlit = drawHelper.bitmapBlit;
        alphamapBlit = drawHelper.alphamapBlit;
        alphaRGBBlit = drawHelper.alphaRGBBlit;
        fillRect = drawHelper.fillRect;
        break;
    }
    case Gradient:{
        unclippedBlend = lBlendGradient;
        break;
    }
    case Texture:{
        unclippedBlend = lBlendTexture;
        if (!texture->imageData){
            unclippedBlend = nullptr;
        }
        break;
    }
    }
    // setup clipping
    if (!unclippedBlend) {
        blend = nullptr;
    } else if (!clip) {
        blend = unclippedBlend;
    } else if (clip->hasRectClip) {
        blend = clip->clipRect.isEmpty() ? nullptr : lSpanFillClipRect;
    } else {
        blend = lSpanFillClipRect;
    }
}

void LSpanData::ensureTexture()
{
    if(texture == nullptr){
        texture = std::make_shared<LTextureData>();
    }
}

void LSpanData::ensureGradient()
{
    if(gradient == nullptr){
        gradient = std::make_shared<LGradient>();
    }
}


LRasterPaintEngineState::LRasterPaintEngineState(const LRasterPaintEngineState &s)
    : LPainterState(s)
    , lastPen(s.lastPen)
    , penData(s.penData)
    , stroker(s.stroker)
    , strokeFlags(s.strokeFlags)
    , lastBrush(s.lastBrush)
    , brushData(s.brushData)
    , fillFlags(s.fillFlags)
    , pixmapFlags(s.pixmapFlags)
    , opacity(s.opacity)
    , txscale(s.txscale)
    , clip(s.clip)
    , dirty(s.dirty)
    , flagBits(s.flagBits)
{
    brushData.tempImage = nullptr;
    penData.tempImage = nullptr;
}

LRasterPaintEngineState::LRasterPaintEngineState()
{
    stroker = nullptr;

    fillFlags = 0;
    strokeFlags = 0;
    pixmapFlags = 0;

    opacity = 255;

    txscale = 1.;

    flags.nonComplexPen = false;
    flags.antialiased = false;
    flags.bilinear = false;
    flags.fastText = true;
    flags.txNoshear = true;
    flags.fastImages = true;

    clip = nullptr;
    dirty = 0;
}

LRasterPaintEngineState::~LRasterPaintEngineState()
{

}


LRasterBuffer::~LRasterBuffer()
{

}

void LRasterBuffer::init()
{
    compositionMode = LCompositionMode::SourceOver;
    monoDestinationWithClut = false;
    destColor0 = 0;
    destColor1 = 0;
}

LImage::Format LRasterBuffer::prepare(std::shared_ptr<LImage> image)
{
    m_buffer = image->bits();
    m_width = std::min<int>(32767, image->width());
    m_height = std::min<int>(32767, image->height());
    bytes_per_pixel = image->depth()/8;
    bytes_per_line = image->bytesPerLine();

    format = image->format();
    const std::vector<uint32_t>& colorTable = image->colorTable();
    if (image->depth() == 1 && colorTable.size() == 2) {
        monoDestinationWithClut = true;
        destColor0 = LColor::premultiply(colorTable[0]);
        destColor1 = LColor::premultiply(colorTable[1]);
    }
    return format;
}

std::shared_ptr<LImage> LRasterBuffer::colorizeBitmap(const std::shared_ptr<LImage> image, const LColor &color)
{
    std::shared_ptr<LImage> sourceImage;
    if(image->format() == LImage::Format::MonoLSB){
        sourceImage = image;
    }else{
        sourceImage = image->convertToFormat(LImage::Format::MonoLSB);
    }
    std::shared_ptr<LImage> dest =
        std::make_shared<LImage>(sourceImage->size(), LImage::Format::ARGB32Premultiplied);

    uint32_t fg = LColor::premultiply(color.toUInt());
    uint32_t bg = 0;

    int height = sourceImage->height();
    int width = sourceImage->width();
    for (int y=0; y<height; ++y) {
        const uint8_t *source = sourceImage->scanLine(y);
        uint8_t *target = dest->scanLine(y);
        if (source == nullptr || target == nullptr){
            break; // we must have run out of memory
        }
        for (int x=0; x < width; ++x){
            target[x] = (source[x>>3] >> (x&7)) & 1 ? fg : bg;
        }
    }
    return dest;
}


LRasterPaintEngine::LRasterPaintEngine(std::shared_ptr<LPaintDevice> device): m_device(device)
{
    init();
}

void LRasterPaintEngine::init()
{
#ifdef _WIN32
    m_hdc = 0;
#endif
    std::shared_ptr<LPaintDevice> device = m_device.lock();
    if(device == nullptr){
        return;
    }


    m_rasterizer = std::make_shared<LRasterizer>();
    m_rasterBuffer.init();

    m_baseClip = std::make_shared<LSpanClipData>(device->height());
    m_baseClip->setClipRect(LRect<int>(0, 0, device->width(), device->height(), false));

    m_imageFiller.init(&m_rasterBuffer, this);
    m_imageFiller.type = LSpanData::Texture;

    m_imageFillerXform.init(&m_rasterBuffer, this);
    m_imageFillerXform.type = LSpanData::Texture;

    m_solidColorFiller.init(&m_rasterBuffer, this);
    m_solidColorFiller.type = LSpanData::Solid;

//    m_deviceDepth = m_device->depth();

    m_monoSurface = false;
    m_features &= ~uint32_t(LPaintEngineFeature::PorterDuff);

    LImage::Format format = LImage::Format::Invalid;

    switch (device->devType()) {
    case LPaintDevice::PDtPixmap:{
        lWarn() << "LRasterPaintEngine: unsupported for pixmaps...";
        return;
    }
    case LPaintDevice::PDtImage:{
        std::shared_ptr<LImage> image = std::dynamic_pointer_cast<LImage>(device);
        format = m_rasterBuffer.prepare(image);
        break;
    }
    default:{
        lWarn() << "LRasterPaintEngine: unsupported target device type:" << device->devType();
        m_device.reset();
        return;
    }
    }

    switch (format) {
    case LImage::Format::MonoLSB:
    case LImage::Format::Mono:{
        m_monoSurface = true;
        break;
    }
    default:{
        if (LImage::toPixelFormat(format).m_alphaUsage){
            m_features |= LPaintEngineFeature::PorterDuff;
        }
        break;
    }
    }
}

void LRasterPaintEngine::initializeRasterizer(LSpanData *data)
{
    std::shared_ptr<LRasterPaintEngineState> s = state();

    m_rasterizer->setAntialiased(s->flags.antialiased);

    LRect<int> clipRect(m_deviceRect);
    ProcessSpans blend;
    // ### get from optimized rectbased LClipData

    const LSpanClipData *c = clip();
    if (c) {
        const LRect<int> r(c->xmin, c->ymin, c->xmax, c->ymax, true);
        clipRect = clipRect.intersected(r);
        blend = data->blend;
    } else {
        blend = data->unclippedBlend;
    }

    m_rasterizer->setClipRect(clipRect);
    m_rasterizer->initialize(blend, data);
}

void LRasterPaintEngine::rasterize(std::shared_ptr<LPainterPath> path,
                                   ProcessSpans callback,
                                   LSpanData *spanData)
{
    if (!callback && path == nullptr){
        return;
    }

    initializeRasterizer(spanData);
    m_rasterizer->rasterize(path);
}


LPaintEngine::Type LRasterPaintEngine::type() const
{
    return Raster;
}

#ifdef _WIN32
static inline bool winClearTypeFontsEnabled()
{
    uint32_t result = 0;
#if !defined(SPI_GETFONTSMOOTHINGTYPE) // MinGW
#    define SPI_GETFONTSMOOTHINGTYPE  0x200A
#    define FE_FONTSMOOTHINGCLEARTYPE 0x002
#endif
    SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &result, 0);
    return result == FE_FONTSMOOTHINGCLEARTYPE;
}

void LRasterPaintEngine::setDC(HDC hdc){
    m_hdc = hdc;
}

HDC LRasterPaintEngine::getDC() const{
    return m_hdc;
}

bool LRasterPaintEngine::clearTypeFontsEnabled(){
    static const bool result = winClearTypeFontsEnabled();
    return result;
}

void LRasterPaintEngine::recalculateFastImages()
{
    std::shared_ptr<LRasterPaintEngineState> s = state();
    if (s != nullptr) {
        s->flags.fastImages = !(s->renderHints & LRenderHint::SmoothPixmapTransform)
                               && (s->matrix.type() <= LTransform2D::TxShear);
    }
}

void LRasterPaintEngine::updateRasterState()
{
    std::shared_ptr<LRasterPaintEngineState> s = state();

    if (s->dirty & (uint32_t)LDirty::Transform){
        updateMatrix(s->matrix);
    }

    if (s->dirty & uint32_t(LDirty::Pen|LDirty::CompositionMode|LDirty::Opacity)) {
        const LCompositionMode mode = s->compositionMode;
        s->flags.fastText = (s->penData.type == LSpanData::Solid)
                             && s->opacity == 255
                             && (mode == LCompositionMode::SourceOver
                                 || (mode == LCompositionMode::Source
                                    && LColor::isOpaque(s->penData.solidColor)));
    }

    s->dirty = 0;
}

void LRasterPaintEngine::updateMatrix(const LTransform2D &matrix)
{
    std::shared_ptr<LRasterPaintEngineState> s = state();
    s->matrix = matrix;
    s->flags.txNoshear = LTransform2D::scaleForTransform(s->matrix, &s->txscale);
}


void LRasterPaintEngine::updateMatrixData(LSpanData *spanData,
                                          const std::shared_ptr<LBrush> b,
                                          const LTransform2D &m)
{
    if (b->m_style == LBrush::NoPattern || b->m_style == LBrush::SolidPattern){
        return;
    }

    std::shared_ptr<LRasterPaintEngineState> s = state();
    bool bilinear = s->flags.bilinear;

    if (b->m_transform.type() > LTransform2D::TxNone) { // FALCON: optimize
        spanData->setupMatrix(b->m_transform * m, bilinear);
    } else {
        if (m.type() <= LTransform2D::TxTranslate) {
            // specialize setupMatrix for translation matrices
            // to avoid needless matrix inversion
            spanData->m11 = 1;
            spanData->m12 = 0;
            spanData->m13 = 0;
            spanData->m21 = 0;
            spanData->m22 = 1;
            spanData->m23 = 0;
            spanData->m33 = 1;
            spanData->dx = -m.dx();
            spanData->dy = -m.dy();
            spanData->txop = m.type();
            spanData->bilinear = bilinear;
            spanData->fastMatrix = std::abs(m.dx()) < 1e4 && std::abs(m.dy()) < 1e4;
            spanData->adjustSpanMethods();
        } else {
            spanData->setupMatrix(m, bilinear);
        }
    }
}

bool LRasterPaintEngine::isUnclippedNormalized(const LRect<int> &r) const
{
    const LSpanClipData *cl = clip();
    if (cl != nullptr) {
        // inline contains() for performance (we know the rects are normalized)
        const LRect<int> &r1 = m_deviceRect;
        return (r.m_x >= r1.m_x && r.x2() <= r1.x2()
                && r.m_y >= r1.m_y && r.y2() <= r1.y2());
    }


    if (cl->hasRectClip) {
        // currently all painting functions clips to deviceRect internally
        if (cl->clipRect == m_deviceRect){
            return true;
        }

        // inline contains() for performance (we know the rects are normalized)
        const LRect<int> &r1 = cl->clipRect;
        return (r.m_x >= r1.m_x && r.x2() <= r1.x2()
                && r.m_y >= r1.m_y && r.y2() <= r1.y2());
    } else {
        return LRegion::regionStrictContains(cl->clipRegion.get(), r);
    }
}

std::shared_ptr<LPainterState> LRasterPaintEngine::createState(std::shared_ptr<LPainterState> org) const
{
    std::shared_ptr<LRasterPaintEngineState> s;
    std::shared_ptr<LRasterPaintEngineState> t = std::dynamic_pointer_cast<LRasterPaintEngineState>(org);
    if (t == nullptr){
        s = std::make_shared<LRasterPaintEngineState>();
    }else{        
        s = std::make_shared<LRasterPaintEngineState>(*t);
    }
    return s;
}

void LRasterPaintEngine::setState(std::shared_ptr<LPainterState> s)
{
    LPaintEngine::setState(s);
    std::shared_ptr<LRasterPaintEngineState> t = state();
    if (t->clip && t->clip->enabled != t->clipEnabled) {
        // Since we do not "detach" clipdata when changing only enabled state, we need to resync state here
        t->clip->enabled = t->clipEnabled;
    }
    m_rasterBuffer.compositionMode = s->compositionMode;
}

bool LRasterPaintEngine::begin(std::shared_ptr<LPaintDevice> device)
{
    if (device->devType() == LPaintDevice::PDtPixmap) {
        return false;
    } else {
        m_device = device;
    }

    systemStateChanged();

    std::shared_ptr<LRasterPaintEngineState> s = state();
    m_rasterizer->setClipRect(m_deviceRect);

    s->penData.init(&m_rasterBuffer, this);
    s->penData.setup(s->pen->brush(), s->opacity, s->compositionMode);

    s->brushData.init(&m_rasterBuffer, this);
    s->brushData.setup(s->brush, s->opacity, s->compositionMode);

    m_rasterBuffer.compositionMode = LCompositionMode::SourceOver;

    setDirty(LDirty::BrushOrigin);
    if (m_monoSurface){
        m_glyphCacheFormat = LGlyphFormat::Mono;
    }
    else if (clearTypeFontsEnabled())
    {
        std::shared_ptr<LImage> image = std::dynamic_pointer_cast<LImage>(device);
        if(image != nullptr){
            LImage::Format format = image->format();
            if (format == LImage::Format::ARGB32Premultiplied || format == LImage::Format::RGB32){
                m_glyphCacheFormat = LGlyphFormat::A32;
            }else{
                m_glyphCacheFormat = LGlyphFormat::A8;
            }
        }
    } else{
        m_glyphCacheFormat = LGlyphFormat::A8;
    }

    m_active = true;
    return true;
}

bool LRasterPaintEngine::end()
{
    m_active = false;
    return true;
}

void LRasterPaintEngine::systemStateChanged()
{
    std::shared_ptr<LPaintDevice> dev = m_device.lock();
    if(dev != nullptr){
        m_deviceRectUnclipped = LRect<int>(0, 0,
                                           std::min<int>(LK_RASTER_COORD_LIMIT, dev->width()),
                                           std::min<int>(LK_RASTER_COORD_LIMIT, dev->height()),false);

        if ((m_systemClip!= nullptr) && (!m_systemClip->isEmpty())) {
            std::shared_ptr<LRegion> clippedDeviceRgn = m_systemClip->intersected(m_deviceRectUnclipped);
            m_deviceRect = clippedDeviceRgn->boundingRect();
            m_baseClip->setClipRegion(clippedDeviceRgn);
        } else {
            m_deviceRect = m_deviceRectUnclipped;
            m_baseClip->setClipRect(m_deviceRect);
        }
    }

    std::shared_ptr<LRasterPaintEngineState> pState = state();
    if (pState != nullptr) {
        pState->strokeFlags |= (uint32_t)LDirty::ClipRegion;
        pState->fillFlags |= (uint32_t)LDirty::ClipRegion;
        pState->pixmapFlags |= (uint32_t)LDirty::ClipRegion;
    }
}

void LRasterPaintEngine::penChanged()
{
    std::shared_ptr<LRasterPaintEngineState> pState = state();
    if (pState != nullptr) {
        pState->strokeFlags |= (uint32_t)LDirty::Pen;
        pState->dirty |= (uint32_t)LDirty::Pen;
    }
}

void LRasterPaintEngine::brushChanged()
{
    std::shared_ptr<LRasterPaintEngineState> pState = state();
    if (pState != nullptr) {
        pState->fillFlags |= (uint32_t)LDirty::Brush;
    }
}

void LRasterPaintEngine::brushOriginChanged()
{
    std::shared_ptr<LRasterPaintEngineState> pState = state();
    if (pState != nullptr) {
        pState->fillFlags |= (uint32_t)LDirty::BrushOrigin;
    }
}

void LRasterPaintEngine::opacityChanged()
{
    std::shared_ptr<LRasterPaintEngineState> pState = state();
    if (pState != nullptr) {
        pState->fillFlags |= (uint32_t)LDirty::Opacity;
        pState->strokeFlags |= (uint32_t)LDirty::Opacity;
        pState->pixmapFlags |= (uint32_t)LDirty::Opacity;
        pState->dirty |= (uint32_t)LDirty::Opacity;
    }
}

void LRasterPaintEngine::compositionModeChanged()
{
    std::shared_ptr<LRasterPaintEngineState> pState = state();
    if (pState != nullptr) {
        pState->fillFlags |= (uint32_t)LDirty::CompositionMode;
        pState->dirty |= (uint32_t)LDirty::CompositionMode;

        pState->strokeFlags |= (uint32_t)LDirty::CompositionMode;
        m_rasterBuffer.compositionMode = pState->compositionMode;

        recalculateFastImages();
    }
}

void LRasterPaintEngine::transformChanged()
{
    std::shared_ptr<LRasterPaintEngineState> pState = state();
    if (pState != nullptr) {
        pState->fillFlags |= (uint32_t)LDirty::Transform;
        pState->strokeFlags |= (uint32_t)LDirty::Transform;
        pState->dirty |= (uint32_t)LDirty::Transform;
        recalculateFastImages();
    }
}

void LRasterPaintEngine::renderHintsChanged()
{
    std::shared_ptr<LRasterPaintEngineState> pState = state();
    if (pState != nullptr) {
        bool was_aa = pState->flags.antialiased;
        bool was_bilinear = pState->flags.bilinear;

        pState->flags.antialiased = bool(pState->renderHints & LRenderHint::Antialiasing);
        pState->flags.bilinear = bool(pState->renderHints & LRenderHint::SmoothPixmapTransform);

        if (was_aa != pState->flags.antialiased)
            pState->strokeFlags |= (uint32_t)LDirty::Hints;

        if (was_bilinear != pState->flags.bilinear) {
            pState->strokeFlags |= (uint32_t)LDirty::Pen;
            pState->fillFlags |= (uint32_t)LDirty::Brush;
        }

        recalculateFastImages();
    }
}

void LRasterPaintEngine::fill(const std::shared_ptr<LPainterPath> path, const std::shared_ptr<LBrush> brush)
{
    if (path->isEmpty()){
        return;
    }

    std::shared_ptr<LRasterPaintEngineState> s = state();
    ensureBrush(brush);
    if (!s->brushData.blend){
        return;
    }

    if (path->isRect()) {
        if (!s->flags.antialiased && s->matrix.type() <= LTransform2D::TxScale) {
            const std::vector<LPainterPath::Element>& p = path->elements();
            LPoint<double> tl = LPoint<double>(p[0].m_x, p[0].m_y) * s->matrix;
            LPoint<double> br = LPoint<double>(p[2].m_x, p[2].m_y) * s->matrix;
            fillRectNormalized(LRect<double>(tl, br).toRect(), &s->brushData, this);
            return;
        }
        ensureRasterState();
        if (s->flags.txNoshear) {
            initializeRasterizer(&s->brushData);
            // ### Is normalizing really necessary here?
            const std::vector<LPainterPath::Element>& p = path->elements();
            LRect<double> r = LRect<double>(p[0].m_x, p[0].m_y, p[2].m_x, p[2].m_y, true).normalized();
            if (!r.isEmpty()) {
                const LPoint<double> a = s->matrix.map((r.topLeft() + r.bottomLeft()) * 0.5f);
                const LPoint<double> b = s->matrix.map((r.topRight() + r.bottomRight()) * 0.5f);
                m_rasterizer->rasterizeLine(a, b, r.height() / r.width());
            }
            return;
        }
    }

    // ### Optimize for non transformed ellipses and rectangles...
    LRect<double> cpRect = path->controlPointRect();
    const LRect<double> pathDeviceRect = s->matrix.mapRect(cpRect);
    // Skip paths that by conservative estimates are completely outside the paint device.
    if (!pathDeviceRect.isIntersects(m_deviceRect.toRectF())){
        return;
    }

    ProcessSpans blend = getBrushFunc(pathDeviceRect, &s->brushData);
    std::shared_ptr<LPainterPath> tPath = s->matrix.map(path, true, false);
    //std::shared_ptr<LPainterPath> tPath = s->matrix.map(path, false);
    s->flags.antialiased = true;
    rasterize(tPath, blend, &s->brushData);
}

struct ClipData
{
    LSpanClipData *oldClip;
    LSpanClipData *newClip;
    LClipOper operation;
};

static const LSpan *lIntersectSpans(const LSpanClipData *clip, int *currentClip,
                                            const LSpan *spans, const LSpan *end,
                                            LSpan **outSpans, int available)
{
    const_cast<LSpanClipData *>(clip)->initialize();

    LSpan *out = *outSpans;

    const LSpan *clipSpans = clip->m_spans + *currentClip;
    const LSpan *clipEnd = clip->m_spans + clip->count;

    while (available && spans < end ) {
        if (clipSpans >= clipEnd) {
            spans = end;
            break;
        }
        if (clipSpans->y > spans->y) {
            ++spans;
            continue;
        }
        if (spans->y != clipSpans->y) {
            if (spans->y < clip->count && clip->m_clipLines[spans->y].spans){
                clipSpans = clip->m_clipLines[spans->y].spans;
            }else{
                ++clipSpans;
            }
            continue;
        }
        assert(spans->y == clipSpans->y);

        int sx1 = spans->x;
        int sx2 = sx1 + spans->len;
        int cx1 = clipSpans->x;
        int cx2 = cx1 + clipSpans->len;

        if (cx1 < sx1 && cx2 < sx1) {
            ++clipSpans;
            continue;
        } else if (sx1 < cx1 && sx2 < cx1) {
            ++spans;
            continue;
        }
        int x = std::max<int>(sx1, cx1);
        int len = std::min<int>(sx2, cx2) - x;
        if (len) {
            out->x = std::max<int>(sx1, cx1);
            out->len = std::min<int>(sx2, cx2) - out->x;
            out->y = spans->y;

            x= spans->coverage * clipSpans->coverage;
            out->coverage = (x + (x>>8) + 0x80) >> 8;;
            ++out;
            --available;
        }
        if (sx2 < cx2) {
            ++spans;
        } else {
            ++clipSpans;
        }
    }

    *outSpans = out;
    *currentClip = clipSpans - clip->m_spans;
    return spans;
}


static void lkSpanClip(int count, const LSpan *spans, void *userData)
{
    ClipData *clipData = reinterpret_cast<ClipData *>(userData);
    switch (clipData->operation) {

    case LClipOper::Intersect:
    {
        LSpanClipData *newClip = clipData->newClip;
        newClip->initialize();

        int currentClip = 0;
        const LSpan *end = spans + count;
        while (spans < end) {
            LSpan *newspans = newClip->m_spans + newClip->count;
            spans = lIntersectSpans(clipData->oldClip, &currentClip, spans, end,
                                       &newspans, newClip->allocated - newClip->count);
            newClip->count = newspans - newClip->m_spans;
            if (spans < end) {
                newClip->m_spans = l_check_ptr((LSpan *)realloc(newClip->m_spans, newClip->allocated * 2 * sizeof(LSpan)));
                newClip->allocated *= 2;
            }
        }
        break;
    }
    case LClipOper::Replace:{
        clipData->newClip->appendSpans(spans, count);
        break;
    }
    default:{
        break;
    }
    }
}

void LRasterPaintEngine::clip(const std::shared_ptr<LPainterPath> path, LClipOper op)
{
    std::shared_ptr<LRasterPaintEngineState> s = state();

    // There are some cases that are not supported by clip(QRect)
    if (op != LClipOper::Intersect || !s->clip || s->clip->hasRectClip || s->clip->hasRegionClip) {
        if (s->matrix.type() <= LTransform2D::TxScale
            && path->isRect()) {
            const std::vector<LPainterPath::Element>& points = path->elements();
            LRect<double> r(points[0].m_x, points[0].m_y, points[2].m_x, points[2].m_y, true);
            if (setClipRectInDeviceCoords(mapFillRect(r, s->matrix), op)){
                return;
            }
        }
    }

    if (op == LClipOper::No) {
        s->clip = nullptr;
    } else {
        std::shared_ptr<LSpanClipData> base = m_baseClip;
        // Intersect with current clip when available...
        if (op == LClipOper::Intersect && (s->clip != nullptr)){
            base = s->clip;
        }

        // We always intersect, except when there is nothing to
        // intersect with, in which case we simplify the operation to
        // a replace...
        LClipOper isectOp = LClipOper::Intersect;
        if (base == nullptr){
            isectOp = LClipOper::Replace;
        }

        std::shared_ptr<LSpanClipData> newClip = std::make_shared<LSpanClipData>(m_rasterBuffer.height());
        newClip->initialize();
        ClipData clipData = { base.get(), newClip.get(), isectOp };
        std::shared_ptr<LPainterPath> tPath = s->matrix.map(path, true, false);
        m_rasterizer->setAntialiased(s->flags.antialiased);
        m_rasterizer->setClipRect(m_deviceRect);
        m_rasterizer->initialize(lkSpanClip, &clipData);
        m_rasterizer->rasterize(tPath);

        newClip->fixup();
        s->clip = newClip;
    }
    rasterDirtyClip();
}

void LRasterPaintEngine::clip(const LRect<int> &rect, LClipOper op)
{

    std::shared_ptr<LRasterPaintEngineState> s = state();

    if (op == LClipOper::No) {
        s->clip = nullptr;

    } else if (s->matrix.type() > LTransform2D::TxScale) {
        LPaintEngine::clip(rect, op);
        return;

    } else if (!setClipRectInDeviceCoords(mapFillRect(rect.toRectF(), s->matrix), op)) {
        LPaintEngine::clip(rect, op);
        return;
    }
}

void LRasterPaintEngine::clip(const std::shared_ptr<LRegion> region, LClipOper op)
{
    if (region->rectCount() == 1) {
        clip(region->boundingRect(), op);
        return;
    }

    std::shared_ptr<LRasterPaintEngineState> s = state();
    const LSpanClipData* tclip = clip();
    const LSpanClipData* baseClip = m_baseClip.get();

    if (op == LClipOper::No) {
        s->clip = nullptr;
    } else if (s->matrix.type() > LTransform2D::TxScale
               || (op == LClipOper::Intersect && !tclip->hasRectClip && !tclip->hasRegionClip)
               || (op == LClipOper::Replace && !baseClip->hasRectClip && !baseClip->hasRegionClip)) {
        LPaintEngine::clip(region, op);
    } else {
        const LSpanClipData *curClip;
        std::shared_ptr<LSpanClipData> newClip;

        if (op == LClipOper::Intersect){
            curClip = tclip;
        }else{
            curClip = baseClip;
        }

        if (s->clip != nullptr) {
            newClip = s->clip;
        } else {
            newClip = std::make_shared<LSpanClipData>(m_rasterBuffer.height());
            s->clip = newClip;
        }

        std::shared_ptr<LRegion> r = s->matrix.map(region);
        if (curClip->hasRectClip){
            r = r->intersected(curClip->clipRect);
            newClip->setClipRegion(r);
        }else if (curClip->hasRegionClip){
            r= r->intersected(curClip->clipRegion);
            newClip->setClipRegion(r);
        }
        rasterDirtyClip();
    }
}

void LRasterPaintEngine::clipEnabledChanged()
{
    std::shared_ptr<LRasterPaintEngineState> s = state();

    if (s->clip) {
        s->clip->enabled = s->clipEnabled;
        s->fillFlags |=  (uint32_t)LDirty::ClipEnabled;
        s->strokeFlags |=  (uint32_t)LDirty::ClipEnabled;
        s->pixmapFlags |=  (uint32_t)LDirty::ClipEnabled;
    }
}

void LRasterPaintEngine::fillPath(const std::shared_ptr<LPainterPath> path, LSpanData *fillData)
{
    if (!fillData->blend){
        return;
    }

    const LRect<double> controlPointRect = path->controlPointRect();

    std::shared_ptr<LRasterPaintEngineState> s = state();
    const LRect<int> deviceRect = s->matrix.mapRect(controlPointRect).toRect();
    ProcessSpans blend = getBrushFunc(deviceRect, fillData);

    std::shared_ptr<LPainterPath> tPath = s->matrix.map(path, true, false);
    //std::shared_ptr<LPainterPath> tPath = s->matrix.map(path, false);
    rasterize(tPath, blend, fillData);
}

void LRasterPaintEngine::drawImage(const LPoint<double> &p, const std::shared_ptr<LImage> img)
{
    std::shared_ptr<LRasterPaintEngineState> s = state();
    double scale = img->devicePixelRatio();

    if (scale > 1.0 ||  s->matrix.type() > LTransform2D::TxTranslate) {
        drawImage(LRect<double>(p.m_x, p.m_y, img->width() / scale, img->height() / scale, false),
                  img,
                  LRect<double>(0, 0, img->width(), img->height(), false));
    } else {
        LPoint<double> pt(p.m_x + s->matrix.dx(), p.m_y + s->matrix.dy());

        m_imageFiller.clip = clip();
        m_imageFiller.initTexture(img, s->opacity, LTextureData::Plain, img->rect());
        if (m_imageFiller.blend == nullptr){
            return;
        }
        m_imageFiller.dx = -pt.m_x;
        m_imageFiller.dy = -pt.m_y;
        LRect<int> rr = img->rect().translated(std::lround(pt.m_x), std::lround(pt.m_y));

        fillRectNormalized(rr, &m_imageFiller, this);
    }

}


void LRasterPaintEngine::drawImage(const LRect<double> &r,
                                   const std::shared_ptr<LImage> img,
                                   const LRect<double> &sr,
                                   LFlags<LImageConvertFlag> flags)
{
    if (r.isEmpty()){
        return;
    }

    std::shared_ptr<LRasterPaintEngineState> s = state();
    int sr_l = std::floor(sr.m_x);
    int sr_r = std::ceil(sr.x2()) - 1;
    int sr_t = std::floor(sr.m_y);
    int sr_b = std::ceil(sr.y2()) - 1;

    if (s->matrix.type() <= LTransform2D::TxScale && !s->flags.antialiased && sr_l == sr_r && sr_t == sr_b) {
        // as fillRect will apply the aliased coordinate delta we need to
        // subtract it here as we don't use it for image drawing
        LTransform2D old = s->matrix;

        // Do whatever fillRect() does, but without premultiplying the color if it's already premultiplied.
        uint32_t color = img->pixel(sr_l, sr_t);
        switch (img->format()) {
        case LImage::Format::ARGB32Premultiplied:
        case LImage::Format::ARGB8565Premultiplied:
        case LImage::Format::ARGB6666Premultiplied:
        case LImage::Format::ARGB8555Premultiplied:
        case LImage::Format::ARGB4444Premultiplied:
        case LImage::Format::ABGR32Premultiplied:
        case LImage::Format::A2BGR30Premultiplied:
        case LImage::Format::A2RGB30Premultiplied:{
            // Combine premultiplied color with the opacity set on the painter.
            m_solidColorFiller.solidColor = LColor::uintToColor(color).multiAlpha(s->opacity).toUInt();
            break;
        }
        default:{
            m_solidColorFiller.solidColor =
                LColor::premultiply(LColor::uintToColor(color).multiAlpha(s->opacity).toUInt());
            break;
        }
        }

        if (LColor::isTransparent(m_solidColorFiller.solidColor)
            && s->compositionMode == LCompositionMode::SourceOver){
            return;
        }

        m_solidColorFiller.clip = clip();
        m_solidColorFiller.adjustSpanMethods();
        fillRect(r, &m_solidColorFiller);
        s->matrix = old;
        return;
    }

    bool stretch_sr = r.width() != sr.width() || r.height() != sr.height();
    LSpanClipData *clipData = clip();
    if (s->matrix.type() > LTransform2D::TxTranslate || stretch_sr) {        
        LTransform2D copy = s->matrix;
        copy.translate(r.m_x, r.m_y);
        if (stretch_sr){
            copy.scale(r.width() / sr.width(), r.height() / sr.height());
        }
        copy.translate(-sr.m_x, -sr.m_y);

        m_imageFillerXform.clip = clipData;
        m_imageFillerXform.initTexture(img, s->opacity, LTextureData::Plain, toAlignedRectPositive(sr));
        if (!m_imageFillerXform.blend){
            return;
        }
        m_imageFillerXform.setupMatrix(copy, s->flags.bilinear);

        if (!s->flags.antialiased && s->matrix.type() == LTransform2D::TxScale) {
            LRect<double> rr = s->matrix.mapRect(r);

            const int x1 = std::lround(rr.m_x);
            const int y1 = std::lround(rr.m_y);
            const int x2 = std::lround(rr.x2());
            const int y2 = std::lround(rr.y2());

            fillRectNormalized(LRect<int>(x1, y1, x2-x1, y2-y1, false), &m_imageFillerXform, this);
            return;
        }

        std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
        path->addRect(r);
        fillPath(path, &m_imageFillerXform);
    } else {
        //LPoint<double> pt(r.x() + s->matrix.dx(), r.y() + s->matrix.dy());

        m_imageFiller.clip = clipData;
        m_imageFiller.initTexture(img, s->opacity, LTextureData::Plain, toAlignedRectPositive(sr));
        if (!m_imageFiller.blend){
            return;
        }
        m_imageFiller.dx = -(r.m_x + s->matrix.dx()) + sr.m_x;
        m_imageFiller.dy = -(r.m_y + s->matrix.dy()) + sr.m_y;

        LRect<double> rr = r;
        rr.translate(s->matrix.dx(), s->matrix.dy());

        const int x1 = std::lround(rr.m_x);
        const int y1 = std::lround(rr.m_y);
        const int x2 = std::lround(rr.x2());
        const int y2 = std::lround(rr.y2());

        fillRectNormalized(LRect<int>(x1, y1, x2-x1, y2-y1, false), &m_imageFiller, this);
    }

}

void LRasterPaintEngine::drawPixmap(const LRect<double> &r, const std::shared_ptr<LPixmap> pm, const LRect<double> &sr)
{
    std::shared_ptr<LImage> image = pm->toImage();
    drawImage(r, image, sr);
}

void LRasterPaintEngine::drawPixmap(const LPoint<double> &pos, const std::shared_ptr<LPixmap> pm)
{
    std::shared_ptr<LImage> image = pm->toImage();
    drawImage(pos, image);
}

void LRasterPaintEngine::fillRectNormalized(const LRect<int> &r, LSpanData *data, LRasterPaintEngine* pe)
{
    int x1, x2, y1, y2;

    bool rectClipped = true;

    if (data->clip) {
        x1 = std::max<int>(r.m_x, data->clip->xmin);
        x2 = std::min<int>(r.m_x + r.width(), data->clip->xmax);
        y1 = std::max<int>(r.m_y, data->clip->ymin);
        y2 = std::min<int>(r.m_y + r.height(), data->clip->ymax);
        rectClipped = data->clip->hasRectClip;
    } else if (pe) {
        x1 = std::max<int>(r.m_x, pe->m_deviceRect.m_x);
        x2 = std::min<int>(r.m_x + r.width(), pe->m_deviceRect.m_x + pe->m_deviceRect.width());
        y1 = std::max<int>(r.m_y, pe->m_deviceRect.m_y);
        y2 = std::min<int>(r.m_y + r.height(), pe->m_deviceRect.m_y + pe->m_deviceRect.height());
    } else {
        x1 = std::max<int>(r.m_x, 0);
        x2 = std::min<int>(r.m_x + r.width(), data->rasterBuffer->width());
        y1 = std::max<int>(r.m_y, 0);
        y2 = std::min<int>(r.m_y + r.height(), data->rasterBuffer->height());
    }

    if (x2 <= x1 || y2 <= y1){
        return;
    }

    const int width = x2 - x1;
    const int height = y2 - y1;

    bool isUnclipped = rectClipped
                       || ((pe!=nullptr) && pe->isUnclippedNormalized(LRect<int>(x1, y1, width, height, false)));

    if ((pe!=nullptr) && isUnclipped) {
        const LCompositionMode mode = pe->m_rasterBuffer.compositionMode;

        if (data->fillRect && (mode == LCompositionMode::Source
                               || (mode == LCompositionMode::SourceOver
                                && LColor::isOpaque(data->solidColor))))
        {
            data->fillRect(data->rasterBuffer, x1, y1, width, height, data->solidColor);
            return;
        }
    }

    ProcessSpans blend = isUnclipped ? data->unclippedBlend : data->blend;

    const int nspans = 256;
    LSpan spans[nspans];

    int y = y1;
    while (y < y2) {
        int n = std::min<int>(nspans, y2 - y);
        int i = 0;
        while (i < n) {
            spans[i].x = x1;
            spans[i].len = width;
            spans[i].y = y + i;
            spans[i].coverage = 255;
            ++i;
        }

        blend(n, spans, data);
        y += n;
    }
}

void LRasterPaintEngine::fillRect(const LRect<double> &r, LSpanData *data)
{
    std::shared_ptr<LRasterPaintEngineState> s = state();
    if (!s->flags.antialiased) {
        uint8_t txop = s->matrix.type();
        if (txop == LTransform2D::TxNone) {
            fillRectNormalized(toNormalizedFillRect(r), data, this);
            return;
        } else if (txop == LTransform2D::TxTranslate) {
            const LRect<int> rr = toNormalizedFillRect(r.translated(s->matrix.dx(), s->matrix.dy()));
            fillRectNormalized(rr, data, this);
            return;
        } else if (txop == LTransform2D::TxScale) {
            const LRect<int> rr = toNormalizedFillRect(s->matrix.mapRect(r));
            fillRectNormalized(rr, data, this);
            return;
        }
    }
    ensureRasterState();
    if (s->flags.txNoshear) {
        initializeRasterizer(data);
        LRect<double> nr = r.normalized();
        if (!nr.isEmpty()) {
            const LPoint<double> a = s->matrix.map((nr.topLeft() + nr.bottomLeft()) * 0.5f);
            const LPoint<double> b = s->matrix.map((nr.topRight() + nr.bottomRight()) * 0.5f);
            m_rasterizer->rasterizeLine(a, b, nr.height() / nr.width());
        }
        return;
    }

    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    path->addRect(r);
    fillPath(path, data);
}

LRect<int> LRasterPaintEngine::toNormalizedFillRect(const LRect<double> &rect)
{
    int x1 = std::lround(rect.m_x);
    int y1 = std::lround(rect.m_y);
    int x2 = std::lround(rect.y2());
    int y2 = std::lround(rect.x2());

    if (x2 < x1){
        std::swap(x1, x2);
    }
    if (y2 < y1){
        std::swap(y1, y2);
    }

    return LRect<int>(x1, y1, x2, y2, true);
}

bool LRasterPaintEngine::isUnclipped(const LRect<int> &rect, int penWidth) const
{
    const std::shared_ptr<LRasterPaintEngineState> s = state();
    const LSpanClipData *cl = clip();
    if (!cl) {
        LRect<int> r = rect.normalized();
        // inline contains() for performance (we know the rects are normalized)
        const LRect<int> &r1 = m_deviceRect;
        return (r.m_x >= r1.m_x && r.x2() <= r1.x2()
                && r.m_y >= r1.m_y && r.y2() <= r1.y2());
    }


    // currently all painting functions that call this function clip to deviceRect internally
    if (cl->hasRectClip && cl->clipRect == m_deviceRect){
        return true;
    }

    if (s->flags.antialiased){
        ++penWidth;
    }

    LRect<int> r = rect.normalized();
    if (penWidth > 0) {
        r.m_x = (r.m_x - penWidth);
        r.m_y = (r.m_y - penWidth);
        r.setWidth(r.width() + 2 * penWidth);
        r.setHeight(r.height() + 2 * penWidth);
    }

    if (cl->hasRectClip) {
        // inline contains() for performance (we know the rects are normalized)
        const LRect<int> &r1 = cl->clipRect;
        return (r.m_x >= r1.m_x && r.x2() <= r1.x2()
                && r.m_y >= r1.m_y && r.y2() <= r1.y2());
    } else {
        return LRegion::regionStrictContains(cl->clipRegion.get(), r);
    }

}

bool LRasterPaintEngine::isUnclipped(const LRect<double> &rect, int penWidth) const
{
    const LRect<double> norm = rect.normalized();
    if ((norm.m_x < INT_MIN) || (norm.m_y < INT_MIN)
        || (norm.x2() > INT_MAX) || (norm.y2() > INT_MAX)
        || (norm.width() > INT_MAX) || (norm.height() > INT_MAX)){
        return false;
    }
    return isUnclipped(norm.toAlignedRect(), penWidth);
}

ProcessSpans LRasterPaintEngine::getPenFunc(const LRect<double> &rect, const LSpanData *data) const
{
    const std::shared_ptr<LRasterPaintEngineState> s = state();

    if (s->matrix.type() > LTransform2D::TxTranslate){
        return data->blend;
    }
    const int penWidth = std::ceil(s->lastPen->m_width);
    return isUnclipped(rect, penWidth) ? data->unclippedBlend : data->blend;
}

ProcessSpans LRasterPaintEngine::getBrushFunc(const LRect<int> &rect, const LSpanData *data) const
{
    return isUnclipped(rect, 0) ? data->unclippedBlend : data->blend;
}

ProcessSpans LRasterPaintEngine::getBrushFunc(const LRect<double> &rect, const LSpanData *data) const
{
    return isUnclipped(rect, 0) ? data->unclippedBlend : data->blend;
}

void LRasterPaintEngine::rasterDirtyClip()
{
    std::shared_ptr<LRasterPaintEngineState> s = state();
    s->fillFlags |= (uint32_t)LDirty::ClipPath;
    s->strokeFlags |= (uint32_t)LDirty::ClipPath;
    s->pixmapFlags |= (uint32_t)LDirty::ClipPath;

    m_solidColorFiller.clip = clip();
    m_solidColorFiller.adjustSpanMethods();
}

bool LRasterPaintEngine::setClipRectInDeviceCoords(const LRect<int> &r, LClipOper op)
{
    LRect<int> clipRect = r.intersected(m_deviceRect);
    std::shared_ptr<LRasterPaintEngineState> s = state();

    if (op == LClipOper::Replace || s->clip == nullptr) {

        // No current clip, hence we intersect with sysclip and be
        // done with it...
        std::shared_ptr<LRegion> clipRegion = m_systemClip;
        std::shared_ptr<LSpanClipData> clip = std::make_shared<LSpanClipData>(m_rasterBuffer.height());

        if (clipRegion == nullptr || clipRegion->isEmpty()){
            clip->setClipRect(clipRect);
        }else{
            clipRegion = clipRegion->intersected(clipRect);
            clip->setClipRegion(clipRegion);
        }

        s->clip = clip;
        s->clip->enabled = true;

    } else if (op == LClipOper::Intersect){ // intersect clip with current clip
        std::shared_ptr<LSpanClipData>  base = s->clip;

        if (base->hasRectClip || base->hasRegionClip) {
            if (s->clip == nullptr) {
                s->clip = std::make_shared<LSpanClipData>(m_rasterBuffer.height());
            }
            if (base->hasRectClip){
                s->clip->setClipRect(base->clipRect & clipRect);
            }else{
                std::shared_ptr<LRegion> clipRegion = base->clipRegion->intersected(clipRect);
                s->clip->setClipRegion(clipRegion);
            }
            s->clip->enabled = true;
        } else {
            return false;
        }
    } else {
        return false;
    }

    rasterDirtyClip();
    return true;
}

void LRasterPaintEngine::drawPolygon(const LPoint<double> *points, int pointCount, PolygonDrawMode mode)
{
    std::shared_ptr<LRasterPaintEngineState> s = state();

    if (mode != PolylineMode && LPainterPath::isRect(points, pointCount)) {
        LRect<double> r(points[0], points[2]);
        drawRects(&r, 1);
        return;
    }

    std::shared_ptr<LPainterPath> p = std::make_shared<LPainterPath>();
    p->addPolygon(points, pointCount);

    ensurePen();
    if (mode != PolylineMode) {
        // Do the fill...
        ensureBrush();
        fillPath(p, &(s->brushData));
    }

    // Do the outline...
    if (s->penData.blend) {
        LPaintEngine::stroke(p, s->lastPen);
    }
}

void LRasterPaintEngine::updateBrush(const std::shared_ptr<LBrush> brush)
{
    if(brush != nullptr){
        std::shared_ptr<LRasterPaintEngineState> s = state();
        // must set clip prior to setup, as setup uses it...
        s->brushData.clip = clip();
        s->brushData.setup(brush, s->opacity, s->compositionMode);
        if (s->fillFlags & (uint32_t)LDirty::Transform
            || brush->m_transform.type() >= LTransform2D::TxNone){
            updateMatrixData(&s->brushData, brush, brushMatrix());
        }
        s->lastBrush = brush;
        s->fillFlags = 0;
    }
}

void LRasterPaintEngine::updatePen(const std::shared_ptr<LPen> pen)
{
    if(pen != nullptr){
        std::shared_ptr<LRasterPaintEngineState> s = state();
        LPenStyle penStyle = pen->m_style;

        s->lastPen = pen;
        s->strokeFlags = 0;

        s->penData.clip = clip();
        std::shared_ptr<LBrush> penBrush = pen->brush();
        s->penData.setup(penStyle == LPenStyle::No ? nullptr : penBrush, s->opacity, s->compositionMode);

        if (s->strokeFlags & (uint32_t)LDirty::Transform
            || penBrush->m_transform.type() >= LTransform2D::TxNone) {
            updateMatrixData(&s->penData, penBrush, s->matrix);
        }

        // Slightly ugly handling of an uncommon case... We need to change
        // the pen because it is reused in draw_midpoint to decide dashed
        // or non-dashed.
        if (penStyle == LPenStyle::CustomDash && pen->customDashPattern().empty()) {
            penStyle = LPenStyle::Solid;
            s->lastPen->resetStyle(LPenStyle::Solid);
        }


        m_stroker->setJoinStyle(pen->m_joinStyle);
        m_stroker->setCapStyle(pen->m_capStyle);
        m_stroker->m_miterLimit = pen->m_miterLimit;

        double penWidth = pen->m_width;
        if (lFuzzyIsZero(penWidth) == 0){
            m_stroker->m_strokeWidth = 1;
        }else{
            m_stroker->m_strokeWidth = penWidth;
        }

        if (penStyle == LPenStyle::Solid) {
            s->stroker = m_stroker;
        } else if (penStyle != LPenStyle::No) {
            if (pen->isCosmetic()) {
                m_dasher->m_clipRect = m_deviceRect.toRectF();
            } else {
                m_dasher->m_clipRect = s->matrix.inverted().mapRect(m_deviceRect.toRectF());
            }
            m_dasher->m_dashPattern = pen->dashPattern();
            m_dasher->m_dashOffset = pen->m_dashOffset;
            s->stroker = m_dasher;
        }

        ensureRasterState(); // needed because of tx_noshear...
        s->flags.nonComplexPen = (pen->m_capStyle <= LPenCapStyle::Square) && s->flags.txNoshear;
        s->strokeFlags = 0;
    }
}

LRect<int> LRasterPaintEngine::clipBoundingRect() const
{
    const LSpanClipData *clipData = clip();

    if (clipData == nullptr){
        return m_deviceRect;
    }

    if (clipData->hasRectClip){
        return clipData->clipRect;
    }

    return LRect<int>(clipData->xmin,
                      clipData->ymin,
                      clipData->xmax,
                      clipData->ymax,
                      true);
}

LK_NAMESPACE_END

#endif

