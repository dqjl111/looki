#ifndef LPAINTENGINE_H
#define LPAINTENGINE_H
#include "lkgui/paint/lstroker.h"
#include "lkgui/paint/lregion.h"
#include "lkgui/text/ltextitem.h"
#include <deque>

LK_NAMESPACE_BEGIN

enum class LDirty : uint16_t{
    Pen                = 0x0001,
    Brush              = 0x0002,
    BrushOrigin        = 0x0004,
    Font               = 0x0008,
    Background         = 0x0010,
    BackgroundMode     = 0x0020,
    Transform          = 0x0040,
    ClipRegion         = 0x0080,
    ClipPath           = 0x0100,
    Hints              = 0x0200,
    CompositionMode    = 0x0400,
    ClipEnabled        = 0x0800,
    Opacity            = 0x1000,
    All                = 0xffff
};
L_EN_FLAGS(LDirty)

enum class LPaintEngineFeature : uint32_t {
    PrimitiveTransform          = 0x00000001, // Can transform primitives brushes
    PatternTransform            = 0x00000002, // Can transform pattern brushes
    PixmapTransform             = 0x00000004, // Can transform pixmaps
    PatternBrush                = 0x00000008, // Can fill with pixmaps and standard patterns
    LinearGradientFill          = 0x00000010, // Can fill gradient areas
    RadialGradientFill          = 0x00000020, // Can render radial gradients
    ConicalGradientFill         = 0x00000040, // Can render conical gradients
    AlphaBlend                  = 0x00000080, // Can do source over alpha blend
    PorterDuff                  = 0x00000100, // Can do general porter duff compositions
    PainterPaths                = 0x00000200, // Can fill, outline and clip paths
    Antialiasing                = 0x00000400, // Can antialias lines
    BrushStroke                 = 0x00000800, // Can render brush based pens
    ConstantOpacity             = 0x00001000, // Can render at constant opacity
    MaskedBrush                 = 0x00002000, // Can fill with textures that has an alpha channel or mask
    PerspectiveTransform        = 0x00004000, // Can do perspective transformations
    BlendModes                  = 0x00008000, // Can do extended Porter&Duff composition
    ObjectBoundingModeGradients = 0x00010000, // Can do object bounding mode gradients
    RasterOpModes               = 0x00020000, // Can do logical raster operations
    StretchToDevice             = 0x10000000, //
    PaintOutsidePaintEvent      = 0x20000000, // Engine is capable of painting outside paint events
    OpaqueBackground            = 0x40000000,
    AllFeatures               = 0xffffffff  // For convenience
};
L_EN_FLAGS(LPaintEngineFeature)


enum class LCompositionMode {
    SourceOver,
    DestinationOver,
    Clear,
    Source,
    Destination,
    SourceIn,
    DestinationIn,
    SourceOut,
    DestinationOut,
    SourceAtop,
    DestinationAtop,
    Xor,

    //svg 1.2 blend modes
    SvgPlus,
    SvgMultiply,
    SvgScreen,
    SvgOverlay,
    SvgDarken,
    SvgLighten,
    SvgColorDodge,
    SvgColorBurn,
    SvgHardLight,
    SvgSoftLight,
    SvgDifference,
    SvgExclusion,

    // ROPs
    RasterOpSourceOrDestination,
    RasterOpSourceAndDestination,
    RasterOpSourceXorDestination,
    RasterOpNotSourceAndNotDestination,
    RasterOpNotSourceOrNotDestination,
    RasterOpNotSourceXorDestination,
    RasterOpNotSource,
    RasterOpNotSourceAndDestination,
    RasterOpSourceAndNotDestination,
    RasterOpNotSourceOrDestination,
    RasterOpSourceOrNotDestination,
    RasterOpClearDestination,
    RasterOpSetDestination,
    RasterOpNotDestination
};


class LPainterClipInfo
{
public:
    LPainterClipInfo() {} // for QVector, don't use
    enum ClipType { RegionClip, PathClip, RectClip };

    LPainterClipInfo(const std::shared_ptr<LPainterPath> p, LClipOper op, const LTransform2D &m) :
        clipType(PathClip), matrix(m), operation(op), path(p) { }

    LPainterClipInfo(const std::shared_ptr<LRegion> r, LClipOper op, const LTransform2D &m) :
        clipType(RegionClip), matrix(m), operation(op), region(r) { }

    LPainterClipInfo(const LRect<int>& r, LClipOper op, const LTransform2D &m) :
        clipType(RectClip), matrix(m), operation(op), rect(r.toRectF()) { }

    LPainterClipInfo(const LRect<double> &r, LClipOper op, const LTransform2D &m) :
        clipType(RectClip), matrix(m), operation(op), rect(r) { }

    ClipType clipType;
    LTransform2D matrix;
    LClipOper operation;
    std::shared_ptr<LPainterPath> path;
    std::shared_ptr<LRegion> region;
    LRect<double> rect;
};

class LPainter;
class LKGUIDEF LPainterState
{
public:
    LPainterState();
    LPainterState(const LPainterState& s);
    virtual ~LPainterState();

    LFlags<LDirty> dirtyFlags;
    LPainter* painter = nullptr;

    LPoint<double> brushOrigin;
    std::shared_ptr<LFont> font;
    std::shared_ptr<LPen> pen;
    std::shared_ptr<LBrush> brush;
    std::shared_ptr<LBrush> bgBrush; // background brush
    std::shared_ptr<LRegion> clipRegion;
    std::shared_ptr<LPainterPath> clipPath;
    LClipOper clipOperation = LClipOper::No;
    std::deque<LPainterClipInfo> clipInfo; // ### Make me smaller and faster to copy around...
    LFlags<LRenderHint> renderHints;
    LTransform2D worldMatrix;       // World transformation matrix, not window and viewport
    LTransform2D matrix;            // Complete transformation matrix,
    LTransform2D redirectionMatrix;
    int wx = 0, wy = 0, ww = 0, wh = 0; // window rectangle
    int vx = 0, vy = 0, vw = 0, vh = 0; // viewport rectangle
    double opacity = 1;

    uint32_t transWorld:1;                 // World transformation
    uint32_t transView:1;                 // View transformation
    uint32_t clipEnabled:1;

    LBGMode bgMode = LBGMode::Transparent;
    LLayoutDirection layoutDirection = LLayoutDirection::Auto;
    LCompositionMode compositionMode = LCompositionMode::SourceOver;
    uint32_t emulationSpecifier = 0;
    uint32_t changeFlags = 0;
};


struct LKGUIDEF LPixmapFragment {
    double x = 0;
    double y = 0;
    double sourceLeft = 0;
    double sourceTop = 0;
    double width = 0;
    double height = 0;
    double scaleX = 1;
    double scaleY = 1;
    double rotation = 0;
    double opacity = 0;
    static LPixmapFragment create(const LPoint<double> &pos, const LRect<double> &sourceRect,
                                  double scaleX = 1, double scaleY = 1,
                                  double rotation = 0, double opacity = 1);
};

class LImage;
class LPixmap;
class LKGUIDEF LPaintEngine
{
public:
    LPaintEngine();

    enum Type {
        Wayland,
        Raster,
        Direct2D,
        WMF,
        User = 50,    // first user type id
        MaxUser = 100 // last user type id
    };

    enum PolygonDrawMode {
        OddEvenMode,
        WindingMode,
        ConvexMode,
        PolylineMode
    };

    virtual Type type() const = 0;
    virtual bool begin(std::shared_ptr<LPaintDevice> pdev) = 0;
    virtual bool end() =0;

    inline bool testDirty(LFlags<LDirty> df) {
        return m_state->dirtyFlags & df;
    }

    inline void setDirty(LFlags<LDirty> df) {
        m_state->dirtyFlags |= df;
    }

    inline void clearDirty(LFlags<LDirty> df){
        m_state->dirtyFlags &= ~static_cast<uint32_t>(df);
    }


    void setSystemTransform(const LTransform2D &xform);
    void updateSystemClip();
    virtual void systemStateChanged() { }


    virtual LPoint<int> coordinateOffset() const;
    virtual void drawTextItem(const LPoint<double> &p, const LTextItem &textItem);

    bool hasFeature(LFlags<LPaintEngineFeature> feature) const { return m_features & feature; }
    LPainter *painter() const;


    virtual std::shared_ptr<LPainterState> createState(std::shared_ptr<LPainterState> org) const;
    virtual void updateState(const std::shared_ptr<LPainterState> state) ;

    virtual void setState(std::shared_ptr<LPainterState> s);
    inline std::shared_ptr<LPainterState> state() const{
        return m_state;
    }

    virtual void clipEnabledChanged() = 0;
    virtual void fill(const std::shared_ptr<LPainterPath> path, const std::shared_ptr<LBrush> brush) = 0;
    virtual void stroke(const std::shared_ptr<LPainterPath> path, const std::shared_ptr<LPen> pen);

    virtual void clip(const std::shared_ptr<LPainterPath> path, LClipOper op) = 0;
    virtual void clip(const LRect<int> &rect, LClipOper op);
    virtual void clip(const std::shared_ptr<LRegion> region, LClipOper op);

    virtual void drawPath(const std::shared_ptr<LPainterPath> path) ;
    virtual void drawImage(const LRect<double> &r,
                           const std::shared_ptr<LImage> pm,
                           const LRect<double> &sr,
                           LFlags<LImageConvertFlag> flags = LImageConvertFlag::AutoColor)  = 0;
    virtual void drawImage(const LPoint<double> &pos, const std::shared_ptr<LImage> image);

    virtual void drawPixmap(const LRect<double> &r,
                            const std::shared_ptr<LPixmap> pm,
                            const LRect<double> &sr)  = 0;
    virtual void drawPixmap(const LPoint<double> &pos, const std::shared_ptr<LPixmap> pm);

    virtual void drawTiledPixmap(const LRect<double> &rect,
                                 const std::shared_ptr<LPixmap> pm,
                                 const LPoint<double> &offset = LPoint<double>());

    virtual void drawPixmapFragments(const std::vector<LPixmapFragment> fragments,
                                     const std::shared_ptr<LPixmap> pm,
                                     bool isOpaque);

    virtual void drawRects(const LRect<int> *rects, int rectCount);
    virtual void drawRects(const LRect<double> *rects, int rectCount);

    virtual void drawRoundedRect(const LRect<double> &rect,
                                 double xRadius,
                                 double yRadius,
                                 LSizeMode mode = LSizeMode::Absolute);

    virtual void drawLines(const LLine<int> *lines, int lineCount);
    virtual void drawLines(const LLine<double> *lines, int lineCount);

    virtual void drawArc(const LRect<double> &r, double a, double alen);
    virtual void drawEllipses(const LRect<int> *rects, int rectCount);
    virtual void drawEllipses(const LRect<double> *rects, int rectCount);

    virtual void fillRect(const LRect<double> &rect, const std::shared_ptr<LBrush> brush);
    virtual void fillRect(const LRect<double> &rect, const LColor &color);

    virtual void drawPolygon(const LPoint<double> *points, int pointCount, PolygonDrawMode mode);

    virtual void drawPoints(const LPoint<double> *points, int pointCount);
    virtual void drawPoints(const LPoint<int> *points, int pointCount);

    virtual void penChanged() = 0;
    virtual void brushChanged() = 0;
    virtual void brushOriginChanged() = 0;
    virtual void opacityChanged() = 0;
    virtual void compositionModeChanged() = 0;
    virtual void transformChanged() = 0;
    virtual void renderHintsChanged() = 0;

    virtual bool requiresPretransformedGlyphPositions(std::shared_ptr<LFontEngine> fontEngine,
                                                      const LTransform2D &m) const;

    std::shared_ptr<LRegion> m_baseSystemClip;
    LRect<int> m_systemRect;
protected:
    void replayClipOperations();

    std::shared_ptr<LStroker> m_stroker;
    std::shared_ptr<LDashStroker> m_dasher;
    std::shared_ptr<LStrokeHooker> m_strokeHooker;
    std::shared_ptr<LStrokerOps> m_activeStroker;
    std::shared_ptr<LPen> m_strokerPen;

    LRect<double> m_exDeviceRect;
    std::shared_ptr<LRegion> m_systemClip;

    std::shared_ptr<LRegion>  m_systemViewport;

    LTransform2D m_systemTransform;

    union{
        uint8_t m_flags = 0;
        struct{
            uint8_t m_hasSystemTransform : 1;
            uint8_t m_hasSystemViewport : 1;
            uint8_t m_active : 1;
        };
    };

    std::shared_ptr<LPainterState> m_state;
    LFlags<LPaintEngineFeature> m_features;

    friend class LPainter;
};


LK_NAMESPACE_END

#endif // LPAINTENGINE_H
