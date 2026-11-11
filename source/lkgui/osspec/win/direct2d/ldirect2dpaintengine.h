#ifndef LDIRECT2DPAINTENGINE_H
#define LDIRECT2DPAINTENGINE_H

#include "lkgui/paint/lpaintengine.h"
#include <d3d11_1.h>
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <dxgi1_2.h>
#include <wrl.h>
#include <dwrite.h>
#include <stack>
#include "lkgui/text/lfontengine.h"

//#define _DX2_CHECK_BACK_BRUSH_PEN

LK_NAMESPACE_BEGIN

class LDirect2DPixmap;
class LDirect2DPaintEngine : public LPaintEngine
{
public:
    enum Flag {
        NoFlag = 0,
        TranslucentTopLevelWindow = 1,
        // EmulateComposition = 2,
    };

    enum ClipType {
        AxisAlignedClip,
        LayerClip
    };

    enum : unsigned {
        SimpleSystemClip = 0x1
    };


    LDirect2DPaintEngine(uint32_t dxEngineFlags = NoFlag);


    virtual Type type() const override;
    virtual bool begin(std::shared_ptr<LPaintDevice> pdev) override;
    virtual bool end() override;

    virtual void setState(std::shared_ptr<LPainterState> s) override;

    virtual void clip(const std::shared_ptr<LPainterPath> path, LClipOper op) override;
    virtual void clipEnabledChanged() override;
    virtual void fill(const std::shared_ptr<LPainterPath> path, const std::shared_ptr<LBrush> brush) override;
    virtual void stroke(const std::shared_ptr<LPainterPath> path,
                        const std::shared_ptr<LPen> pen) override;

    virtual void fillRect(const LRect<double> &rect, const std::shared_ptr<LBrush> brush) override;

    virtual void penChanged() override;
    virtual void brushChanged() override;
    virtual void brushOriginChanged() override;
    virtual void opacityChanged() override;
    virtual void compositionModeChanged() override;
    virtual void transformChanged() override;
    virtual void renderHintsChanged() override;

    virtual void drawPath(const std::shared_ptr<LPainterPath> path) override;
    virtual void drawTextItem(const LPoint<double> &p, const LTextItem &textItem) override;
    virtual void drawRects(const LRect<int> *rects, int rectCount) override;
    virtual void drawRects(const LRect<double> *rects, int rectCount) override;

    virtual void drawRoundedRect(const LRect<double> &rect,
                                 double xRadius,
                                 double yRadius,
                                 LSizeMode mode = LSizeMode::Absolute) override;

    virtual void drawPixmap(const LRect<double> &r,
                            const std::shared_ptr<LPixmap> pm,
                            const LRect<double> &sr) override;
    virtual void drawImage(const LRect<double> &r,
                           const std::shared_ptr<LImage> image,
                           const LRect<double> &sr,
                           LFlags<LImageConvertFlag> flags = LImageConvertFlag::AutoColor) override;

    virtual void drawEllipses(const LRect<int> *rects, int rectCount) override;
    virtual void drawEllipses(const LRect<double> *rects, int rectCount) override;
    virtual void drawArc(const LRect<double> &r, double a, double alen) override;

    void suspend();
    void resume();

    std::shared_ptr<LDirect2DPixmap> directPixmap();

private:
    ID2D1DeviceContext* dc() const;
    Microsoft::WRL::ComPtr<ID2D1Brush> toD2dBrush(const std::shared_ptr<LBrush> newBrush);
    D2D1_INTERPOLATION_MODE interpolationMode() const;
    D2D1_ANTIALIAS_MODE antialiasMode() const;
    D2D1_LAYER_OPTIONS1 layerOptions() const;

    void ensureBrush();
    void ensureBrush(const std::shared_ptr<LBrush> brush);
    void ensurePen();
    void ensurePen(const std::shared_ptr<LPen> pen);

    void updateBrushOrigin(const LPoint<double> &brushOrigin);
    void applyBrushOrigin(const LPoint<double> &origin);
    void negateCurrentBrushOrigin();

    void clearClips();
    void pushClip(const std::shared_ptr<LPainterPath> path);
    Microsoft::WRL::ComPtr<ID2D1PathGeometry1> painterPathToDxPath(const std::shared_ptr<LPainterPath> path);
    Microsoft::WRL::ComPtr<ID2D1PathGeometry1> joinPath(LPoint<double>& p1, LPoint<double>& p2, LPoint<double>& p3);

    void updateClipEnabled(bool enabled);
    void updateOpacity(double opacity);
    void updateCompositionMode(LCompositionMode mode);
    void updateTransform(const LTransform2D &transform);
    void updateHints();

    Microsoft::WRL::ComPtr<IDWriteFontFace> fontFaceFromFontEngine(std::shared_ptr<LFontEngine> fe);
    void drawGlyphRun(const D2D1_POINT_2F &pos,
                      IDWriteFontFace *fontFace,
                      const LFont::Def &fontDef,
                      int numGlyphs,
                      const UINT16 *glyphIndices,
                      const FLOAT *glyphAdvances,
                      const DWRITE_GLYPH_OFFSET *glyphOffsets,
                      bool rtl);

    std::weak_ptr<LPaintDevice> m_pdev;
    bool m_isSuspend = false;

    // std::shared_ptr<LImage> m_fallbackImage;

    unsigned int m_clipFlags = 0;
    std::stack<ClipType> m_pushedClips;
    uint32_t m_dxEngineFlags;

    LPoint<double> m_currentBrushOrigin;

    std::unordered_map<LFont::Def, Microsoft::WRL::ComPtr<IDWriteFontFace> > m_fontCache;

    struct {
        std::shared_ptr<LPen> lpen;
#ifdef _DX2_CHECK_BACK_BRUSH_PEN
        std::shared_ptr<LPen> backPen;
#endif
        Microsoft::WRL::ComPtr<ID2D1Brush> brush;
        Microsoft::WRL::ComPtr<ID2D1StrokeStyle1> strokeStyle;
        Microsoft::WRL::ComPtr<ID2D1BitmapBrush1> dashBrush;
        int dashLength;

        inline void reset() {
            lpen = nullptr;
            brush.Reset();
            strokeStyle.Reset();
            dashBrush.Reset();
            dashLength = 0;
        }
    } m_pen;

    struct {
        std::shared_ptr<LBrush> lbrush;
#ifdef _DX2_CHECK_BACK_BRUSH_PEN
        std::shared_ptr<LBrush> backBrush;
#endif
        Microsoft::WRL::ComPtr<ID2D1Brush> brush;

        inline void reset() {
            lbrush = nullptr;
            brush.Reset();
        }
    } m_brush;
};

class LDirect2DPaintEngineSuspender
{
    std::shared_ptr<LDirect2DPaintEngine> m_engine;
public:
    LDirect2DPaintEngineSuspender(std::shared_ptr<LDirect2DPaintEngine> engine)
        : m_engine(engine)
    {
        if(m_engine!= nullptr){
            m_engine->suspend();
        }
    }

    ~LDirect2DPaintEngineSuspender()
    {
        if(m_engine!= nullptr){
            m_engine->resume();
        }
    }
};

LK_NAMESPACE_END

#endif // LDIRECT2DPAINTENGINE_H
