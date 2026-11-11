#ifndef LPAINTER_H
#define LPAINTER_H
#include "lkgui/lrect.h"
#include "lkgui/paint/lpaintdevice.h"
#include "lkgui/paint/lbrush.h"
#include "lkgui/image/limage.h"
#include "lkgui/paint/lpen.h"
#include "lkgui/text/lfont.h"
#include "lkgui/text/ltextoption.h"
#include "lkgui/paint/lpolygon.h"
#include "lkgui/paint/lpaintengine.h"


LK_NAMESPACE_BEGIN

struct LPainterDummyState
{
    std::shared_ptr<LFont> font;
    std::shared_ptr<LPen> pen;
    std::shared_ptr<LBrush> brush;
    LTransform2D transform;
};


class LTextEngine;
class LKGUIDEF LPainter
{
public:
    LPainter();
    LPainter(std::shared_ptr<LPaintDevice> device);
    ~LPainter();

    void fillRect(const LRect<double> &rect, const LColor &color);
    void fillRect(const LRect<double> &rect, const std::shared_ptr<LBrush> brush);
    void eraseRect(const LRect<double> &rect);

    std::shared_ptr<LPaintDevice> device() const;

    bool begin(std::shared_ptr<LPaintDevice> device);
    bool end();
    bool isActive() const;

    void save();
    void restore();

    LFlags<LRenderHint> renderHints() const;
    void setRenderHints(LFlags<LRenderHint> hints, bool on = true);
    void setTransform(const LTransform2D &transform, bool combine = false);
    const LTransform2D &transform() const;
    void resetTransform();

    void drawImage(const LPoint<double> &p, const std::shared_ptr<LImage> image);
    void drawImage(const LRect<double> &targetRect,
                   const std::shared_ptr<LImage> image,
                   const LRect<double> &sourceRect,
                   LFlags<LImageConvertFlag> flags = LImageConvertFlag::AutoColor);

    inline void drawImage(const LPoint<double> &p,
                   const std::shared_ptr<LImage> image,
                   const LRect<double> &sr){
        drawImage(LRect<double>(p, -1, -1), image, sr);
    }

    void drawPixmap(const LPoint<double> &p, const std::shared_ptr<LPixmap> image);
    void drawPixmap(const LRect<double> &r,
                    const std::shared_ptr<LPixmap> pm,
                    const LRect<double> &sr);

    inline void drawPixmap(const LPoint<double> &p,
                           const std::shared_ptr<LPixmap> pm,
                           const LRect<double> &sr){
        drawPixmap(LRect<double>(p, -1, -1), pm, sr);
    }

    void drawTiledPixmap(const LRect<double> &rect,
                         const std::shared_ptr<LPixmap> pm,
                         const LPoint<double> &offset = LPoint<double>());


    void drawPixmapFragments(const std::vector<LPixmapFragment> fragments,
                             const std::shared_ptr<LPixmap> pm,
                             bool isOpaque = false);

    void setCompositionMode(LCompositionMode mode);
    LCompositionMode compositionMode() const;

    void setWorldTransform(const LTransform2D &matrix, bool combine = false);
    const LTransform2D &worldTransform() const;

    void setClipRegion(std::shared_ptr<LRegion> r, LClipOper op = LClipOper::Replace);
    void setClipRect(const LRect<double>& r, LClipOper op = LClipOper::Replace);
    void setClipRect(const LRect<int> &r, LClipOper op = LClipOper::Replace);
    void setClipPath(const std::shared_ptr<LPainterPath> path, LClipOper op = LClipOper::Replace);


    void setPen(const LColor &color);
    void setPen(const std::shared_ptr<LPen> pen, bool useBack = false);
    void setPen(LPenStyle style);
    void setPen(const LColor &color, LPenStyle style);
    void setPen(const LColor &color, LPenStyle style, int width);
    std::shared_ptr<LPen> pen() const;

    void setBrush(const LColor &color);
    void setBrush(const std::shared_ptr<LBrush> brush, bool useBack = false);
    void setBrush(LBrush::Style style);
    void setBrush(const LColor &color, LBrush::Style style);
    std::shared_ptr<LBrush> brush() const;

    std::shared_ptr<LFont> font() const;
    void setFont(std::shared_ptr<LFont> f);

    void translate(const LPoint<double> &offset);

    void fillPath(const std::shared_ptr<LPainterPath> path, LColor color);
    void fillPath(const std::shared_ptr<LPainterPath> path, const std::shared_ptr<LBrush> brush);
    void drawPath(const std::shared_ptr<LPainterPath> path);

    void drawRects(const LRect<double> *rects, int rectCount);
    void drawRect(const LRect<double>& rect);
    void drawRects(const LRect<int>* rects, int rectCount);
    void drawRect(const LRect<int>& rect);

    void drawRoundedRect(const LRect<double> &rect,
                         double xRadius,
                         double yRadius,
                         LSizeMode mode = LSizeMode::Absolute);

    void drawLines(const LLine<double> *lines, int lineCount);
    void drawLine(const LLine<double>& line);
    void drawLines(const LLine<int> *lines, int lineCount);
    void drawLine(const LLine<int>& line);

    void drawArc(const LRect<double> &r, int a, int alen);

    void drawEllipses(const LRect<double> *rects, int rectCount);
    void drawEllipse(const LRect<double>& rect);
    void drawEllipses(const LRect<int>* rects, int rectCount);
    void drawEllipse(const LRect<int>& rect);

    void drawPolyline(const LPoint<double> *points, int pointCount);
    inline void drawPolyline(const LPolygon<double> &polygon){
        drawPolyline(polygon.data(), polygon.size());
    }

    void drawPolygon(const LPoint<double> *points, int pointCount, LFillRule rule = LFillRule::OddEven);
    inline void drawPolygon(const LPolygon<double> &polygon, LFillRule rule = LFillRule::OddEven){
        drawPolygon(polygon.data(), polygon.size(), rule);
    }

    void drawConvexPolygon(const LPoint<double> *points, int pointCount);
    inline void drawConvexPolygon(const LPolygon<double> &polygon)
    {
        drawConvexPolygon(polygon.data(), int(polygon.size()));
    }


    void drawTextItem(const LPoint<double> &p,
                      const LTextItem &ti,
                      std::shared_ptr<LTextEngine> textEngine = nullptr);
    void drawText(const LPoint<double> &p, const std::wstring &s);
    void drawText(const LPoint<double> &p, const std::wstring &str, LFlags<LTextFlag> tf, int justificationPadding);
    void drawText(const LRect<double> &r,
                  const std::wstring &text,
                  LFlags<LTextFlag> tf = 0,
                  const LTextOption &o = LTextOption());
    void drawText(const LRect<double> &r, int flags, const std::wstring &text, LRect<double> *br = nullptr);

    void drawPoints(const LPoint<double> *points, int pointCount);
    void drawPoints(const LPoint<int> *points, int pointCount);

    inline void drawPoint(const LPoint<double> &pt){
        drawPoints(&pt, 1);
    }
    inline void drawPoint(const LPoint<int> &pt){
        drawPoints(&pt, 1);
    }
    inline void drawPoint(int x, int y){
        LPoint p(x, y);
        drawPoints(&p, 1);
    }


    void setLayoutDirection(LLayoutDirection direction);
    LLayoutDirection layoutDirection() const;

    void setClipping(bool enable);
    bool hasClipping() const;
    std::shared_ptr<LPainterPath> clipPath() const;
    std::shared_ptr<LRegion> clipRegion() const;

    void setBackground(const std::shared_ptr<LBrush> bg);
    void setBackgroundMode(LBGMode mode);
    void setBrushOrigin(const LPoint<double> &p);

    const std::shared_ptr<LBrush> background() const;
    const LBGMode backgroundMode() const;
    const LPoint<double> brushOrigin() const;

    double opacity() const;
    void setOpacity(double opacity);
    void scale(double sx, double sy);
    void shear(double sh, double sv);
    void rotate(double a);

    LRect<int> window() const;
    void setWindow(const LRect<int> &window);

    inline std::shared_ptr<LPainterState> state() const{
        return m_state;
    }

    inline std::shared_ptr<LPaintEngine> engine() const{
        return m_engine;
    }

    static void formatText(const std::shared_ptr<LFont> fnt, const LRect<double> &rIn,
                      LFlags<LTextFlag> tf, const LTextOption *option,
                      const std::wstring& str, LRect<double> *brect,
                      int tabstops, int *ta, int tabarraylen,
                      LPainter *painter = nullptr);

    static void formatText(const std::shared_ptr<LFont> fnt, const LRect<double> &r,
                      uint32_t tf, const std::wstring& str, LRect<double> *brect,
                      int tabstops, int *ta, int tabarraylen,
                      LPainter *painter = nullptr);

protected:
    enum DrawOper {
        StrokeDraw        = 0x1,
        FillDraw          = 0x2,
        StrokeAndFillDraw = 0x3
    };

    void updateState(std::shared_ptr<LPainterState>  newState);
    void updateStateImpl(std::shared_ptr<LPainterState> state);
    double effectiveDevicePixelRatio() const;

    void updateMatrix();
    void updateInvMatrix();
    LTransform2D viewTransform() const;
    LTransform2D hidpiScaleTransform() const;

    static inline void cleanupPainterState(LPainter *d);

    static std::shared_ptr<LPainterDummyState> fakeState();

    std::shared_ptr<LPaintDevice> m_device = nullptr;
    std::shared_ptr<LPaintEngine> m_engine = nullptr;
    std::shared_ptr<LPainterState> m_state = nullptr;
    std::vector<std::shared_ptr<LPainterState> > m_states;

    LTransform2D m_invMatrix;
    uint32_t m_txinv:1;

    static std::shared_ptr<LPainterDummyState> m_dummyState;
};

LK_NAMESPACE_END
#endif // LPAINTER_H
