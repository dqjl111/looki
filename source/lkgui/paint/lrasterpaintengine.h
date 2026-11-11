#ifndef LRASTERPAINTENGINE_H
#define LRASTERPAINTENGINE_H

#include "lpaintdevice.h"
#include "lpaintengine.h"
#include "lkgui/paint/lrasterizer.h"
#include "lkgui/paint/ldrawhelper.h"
#ifdef _WIN32
#include "lkgui/osspec/win/lwindows.h"
#endif

LK_NAMESPACE_BEGIN
class LKGUIDEF LRasterBuffer
{
public:
    LRasterBuffer() : m_width(0), m_height(0), m_buffer(nullptr) { init(); }

    ~LRasterBuffer();

    void init();

    LImage::Format prepare(std::shared_ptr<LImage> image);

    uint8_t *scanLine(int y) {
        return m_buffer + y * lsizetype(bytes_per_line);
    }

    int width() const { return m_width; }
    int height() const { return m_height; }
    int bytesPerLine() const { return bytes_per_line; }
    int bytesPerPixel() const { return bytes_per_pixel; }
    template<typename T>
    int stride() { return bytes_per_line / sizeof(T); }

    uint8_t *buffer() const { return m_buffer; }

    bool monoDestinationWithClut;
    uint32_t destColor0;
    uint32_t destColor1;

    LCompositionMode compositionMode;
    LImage::Format format;
    std::shared_ptr<LImage> colorizeBitmap(const std::shared_ptr<LImage> image, const LColor &color);

private:
    int m_width;
    int m_height;
    int bytes_per_line;
    int bytes_per_pixel;
    uint8_t *m_buffer;
};

class LKGUIDEF LSpanClipData {
public:
    LSpanClipData(int height);
    ~LSpanClipData();

    int clipSpanHeight;
    struct ClipLine {
        int count;
        LSpan *spans;
    } *m_clipLines;

    void initialize();

    inline ClipLine *clipLines() {
        if (!m_clipLines){
            initialize();            
        }
        return m_clipLines;
    }

    inline LSpan *spans() {
        if (!m_spans){
            initialize();
        }
        return m_spans;
    }

    int allocated;
    int count;
    LSpan *m_spans;
    int xmin, xmax, ymin, ymax;

    LRect<int> clipRect;
    std::shared_ptr<LRegion> clipRegion;

    uint32_t enabled : 1;
    uint32_t hasRectClip : 1;
    uint32_t hasRegionClip : 1;

    void appendSpan(int x, int length, int y, int coverage);
    void appendSpans(const LSpan *s, int num);

    // ### Should optimize and actually kill the QSpans if the rect is
    // ### a subset of The current region. Thus the "fast" clipspan
    // ### callback can be used
    void setClipRect(const LRect<int> &rect);
    void setClipRegion(const std::shared_ptr<LRegion> region);
    void fixup();
};

class LRasterPaintEngine;
struct LKGUIDEF LSpanData
{
    LRasterBuffer *rasterBuffer = nullptr;
    ProcessSpans blend = nullptr;
    ProcessSpans unclippedBlend = nullptr;
    BitmapBlitFunc bitmapBlit = nullptr;
    AlphamapBlitFunc alphamapBlit = nullptr;
    AlphaRGBBlitFunc alphaRGBBlit = nullptr;
    RectFillFunc fillRect = nullptr;
    double m11=0, m12=0, m13=0, m21=0, m22=0, m23=0, m33=0, dx=0, dy=0;   // inverse xform matrix
    LSpanClipData *clip = nullptr;
    enum Type : uint8_t{
        None,
        Solid,
        Gradient,
        Texture
    } type = None;
    uint8_t txop = 0;
    bool fastMatrix = false;
    bool bilinear=false;
    std::shared_ptr<LImage> tempImage = nullptr;
    uint32_t solidColor=0;

    std::shared_ptr<LGradient> gradient = nullptr;
    std::shared_ptr<LTextureData> texture = nullptr;

    LSpanData(){}

    void init(LRasterBuffer *rb, const LRasterPaintEngine *pe);
    void setup(const std::shared_ptr<LBrush> brush, uint8_t alpha, LCompositionMode compositionMode);
    void setupMatrix(const LTransform2D &matrix, int bilinear);
    void initTexture(const std::shared_ptr<LImage> image,
                     uint8_t alpha,
                     LTextureData::Type = LTextureData::Plain,
                     const LRect<int> &sourceRect = LRect<int>());
    void adjustSpanMethods();
    void ensureTexture();
    void ensureGradient();
};




class LKGUIDEF LRasterPaintEngineState : public LPainterState
{
public:
    LRasterPaintEngineState(const LRasterPaintEngineState& s);
    LRasterPaintEngineState();
    ~LRasterPaintEngineState();


    std::shared_ptr<LPen> lastPen;
    LSpanData penData;
    std::shared_ptr<LStrokerOps> stroker;
    uint32_t strokeFlags;
    std::shared_ptr<LBrush> lastBrush;
    LSpanData brushData;
    uint32_t fillFlags;

    uint32_t pixmapFlags;
    uint8_t opacity;

    double txscale;

    std::shared_ptr<LSpanClipData> clip;
    uint32_t dirty;

    struct Flags {
        uint16_t nonComplexPen : 1;           // can use rasterizer, rather than stroker
        uint16_t antialiased : 1;
        uint16_t bilinear : 1;
        uint16_t fastText : 1;
        uint16_t txNoshear : 1;
        uint16_t fastImages : 1;
    };

    union {
        Flags flags;
        uint16_t flagBits;
    };
};


class LKGUIDEF LRasterPaintEngine : public LPaintEngine
{
public:
    LRasterPaintEngine(std::shared_ptr<LPaintDevice> device);
    virtual Type type() const override;

    virtual std::shared_ptr<LPainterState> createState(std::shared_ptr<LPainterState> org) const override;
    void setState(std::shared_ptr<LPainterState> s) override;
    inline std::shared_ptr<LRasterPaintEngineState> state() const{
        return std::dynamic_pointer_cast<LRasterPaintEngineState>(LPaintEngine::m_state);
    }

    virtual bool begin(std::shared_ptr<LPaintDevice> pdev) override;
    virtual bool end() override;
    virtual void systemStateChanged() override;

    virtual void penChanged() override;
    virtual void brushChanged() override;
    virtual void brushOriginChanged() override;
    virtual void opacityChanged() override;
    virtual void compositionModeChanged() override;
    virtual void transformChanged() override;
    virtual void renderHintsChanged() override;

    virtual void fill(const std::shared_ptr<LPainterPath> path, const std::shared_ptr<LBrush> brush) override;
    virtual void clip(const std::shared_ptr<LPainterPath> path, LClipOper op) override;
    virtual void clip(const LRect<int> &rect, LClipOper op);
    virtual void clip(const std::shared_ptr<LRegion> region, LClipOper op);

    virtual void clipEnabledChanged() override;
    virtual void fillPath(const std::shared_ptr<LPainterPath> path, LSpanData *fillData);

    virtual void drawImage(const LPoint<double> &pos, const std::shared_ptr<LImage> image) override;
    virtual void drawImage(const LRect<double> &r,
                           const std::shared_ptr<LImage> pm,
                           const LRect<double> &sr,
                           LFlags<LImageConvertFlag> flags = LImageConvertFlag::AutoColor) override;
    virtual void drawPixmap(const LRect<double> &r,
                            const std::shared_ptr<LPixmap> pm,
                            const LRect<double> &sr) override;
    virtual void drawPixmap(const LPoint<double> &pos, const std::shared_ptr<LPixmap> pm) override;


    virtual void drawPolygon(const LPoint<double> *points, int pointCount, PolygonDrawMode mode) override;


    void updateBrush(const std::shared_ptr<LBrush> brush);
    void updatePen(const std::shared_ptr<LPen> pen);

    LRect<int> clipBoundingRect() const;

#ifdef _WIN32
    void setDC(HDC hdc);
    HDC getDC() const;
    static bool clearTypeFontsEnabled();
#endif


    inline LSpanClipData *clip() const {
        std::shared_ptr<LRasterPaintEngineState> s = state();
        if(s != nullptr){
            if ((s->clip != nullptr) && s->clip->enabled){
                return s->clip.get();
            }
        }
        return m_baseClip.get();
    }

    LTransform2D brushMatrix() const {
        std::shared_ptr<LRasterPaintEngineState> s = state();
        if(s != nullptr){
            LTransform2D m(s->matrix);
            m.translate(s->brushOrigin.m_x, s->brushOrigin.m_y);
            return m;
        }
        return LTransform2D();
    }
protected:
    void recalculateFastImages();
    void updateRasterState();
    void init();
    void initializeRasterizer(LSpanData *data);

    void rasterize(std::shared_ptr<LPainterPath> path,
                   ProcessSpans callback,
                   LSpanData *spanData);

    inline void ensureRasterState() {
        if (state()->dirty){
            updateRasterState();
        }
    }
    void updateMatrix(const LTransform2D &matrix);
    inline void ensureBrush(const std::shared_ptr<LBrush> brush) {
        std::shared_ptr<LRasterPaintEngineState> s = state();
        if ((s->lastBrush!= brush) || s->fillFlags){
            updateBrush(brush);
        }
    }
    inline void ensureBrush() {
        std::shared_ptr<LRasterPaintEngineState> s = state();
        if ((s->lastBrush!= s->brush) || s->fillFlags){
            updateBrush(s->brush);
        }
    }

    inline void ensurePen(const std::shared_ptr<LPen> pen) {
        std::shared_ptr<LRasterPaintEngineState> s = state();
        if (s->lastPen!=pen
            || (pen->m_style != LPenStyle::No && s->strokeFlags)){
            updatePen(pen);
        }
    }
    inline void ensurePen() {
        std::shared_ptr<LRasterPaintEngineState> s = state();
        if (s->lastPen!=s->pen
            || (s->pen->m_style != LPenStyle::No && s->strokeFlags)){
            updatePen(s->pen);
        }
    }

    void updateMatrixData(LSpanData *spanData,
                          const std::shared_ptr<LBrush> brush,
                          const LTransform2D &brushMatrix);

    bool isUnclippedNormalized(const LRect<int> &rect) const;
    static void fillRectNormalized(const LRect<int> &r, LSpanData *data, LRasterPaintEngine* pe);
    void fillRect(const LRect<double> &rect, LSpanData *data);
    LRect<int> toNormalizedFillRect(const LRect<double> &rect);

    bool isUnclipped(const LRect<int> &rect, int penWidth) const;
    bool isUnclipped(const LRect<double> &rect, int penWidth) const;
    ProcessSpans getPenFunc(const LRect<double> &rect, const LSpanData *data) const;
    ProcessSpans getBrushFunc(const LRect<int> &rect, const LSpanData *data) const;
    ProcessSpans getBrushFunc(const LRect<double> &rect, const LSpanData *data) const;

    void rasterDirtyClip();
    bool setClipRectInDeviceCoords(const LRect<int> &r, LClipOper op);

    LRasterBuffer m_rasterBuffer;
    std::weak_ptr<LPaintDevice> m_device;
#ifdef _WIN32
    HDC m_hdc = NULL;
#endif

    uint32_t m_monoSurface : 1;

    std::shared_ptr<LSpanClipData> m_baseClip;
    // std::shared_ptr<LStroker> m_basicStroker;

    LRect<int> m_deviceRect;
    LRect<int> m_deviceRectUnclipped;

    LGlyphFormat m_glyphCacheFormat;

    std::shared_ptr<LRasterizer> m_rasterizer;

    LSpanData m_imageFiller;
    LSpanData m_imageFillerXform;
    LSpanData m_solidColorFiller;
};

LK_NAMESPACE_END

#endif // LRASTERPAINTENGINE_H
