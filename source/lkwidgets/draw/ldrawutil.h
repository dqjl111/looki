#ifndef LDRAWUTIL_H
#define LDRAWUTIL_H
#include "lkgui/paint/lpainter.h"
#include "lkgui/text/lcssparser.h"
#include "lkgui/text/lcssbuilder.h"
#include "lkgui/lguiapplication.h"

LK_NAMESPACE_BEGIN
struct LTileRules
{
    inline LTileRules(LTileRule horizontalRule, LTileRule verticalRule)
            : horizontal(horizontalRule), vertical(verticalRule) {}
    inline LTileRules(LTileRule rule = LTileRule::Stretch)
            : horizontal(rule), vertical(rule) {}
    LTileRule horizontal;
    LTileRule vertical;
};

enum class LDrawBorderHint
{
    OpaqueTopLeft = 0x0001,
    OpaqueTop = 0x0002,
    OpaqueTopRight = 0x0004,
    OpaqueLeft = 0x0008,
    OpaqueCenter = 0x0010,
    OpaqueRight = 0x0020,
    OpaqueBottomLeft = 0x0040,
    OpaqueBottom = 0x0080,
    OpaqueBottomRight = 0x0100,
    OpaqueCorners = OpaqueTopLeft | OpaqueTopRight | OpaqueBottomLeft | OpaqueBottomRight,
    OpaqueEdges = OpaqueTop | OpaqueLeft | OpaqueRight | OpaqueBottom,
    OpaqueFrame = OpaqueCorners | OpaqueEdges,
    OpaqueAll = OpaqueCenter | OpaqueFrame
};

L_EN_FLAGS(LDrawBorderHint)

class LKWIDGETSDEF LRenderRule{
protected:
    std::shared_ptr<LCssBorderImageData> m_borderImage = nullptr;
    std::shared_ptr<LCssBorderData> m_border = nullptr;
    std::shared_ptr<LCssBoxData> m_box = nullptr;
    std::shared_ptr<LCssBackgroundData> m_background = nullptr;
    std::shared_ptr<LCssForegroundData> m_foreground = nullptr;
    std::shared_ptr<LCssImageData> m_image = nullptr;
    int m_clipset = 0;
    std::shared_ptr<LPainterPath> m_clipPath = nullptr;
public:
    LRenderRule(std::shared_ptr<lcss::StyleComputed> data = nullptr);

    void drawBorder(LPainter *p, const LRect<int>& rect);
    void drawBorderImage(LPainter *p, const LRect<int>& rect);
    void drawBackground(LPainter *p, const LRect<int>& rect, const LPoint<int>& pt= LPoint<int>(0, 0));
    void drawBackgroundImage(LPainter *p, const LRect<int>& rect, LPoint<int> pt= LPoint<int>(0, 0));
    void drawFrame(LPainter *p, const LRect<int>& rect);
    void drawImage(LPainter *p, const LRect<int> &rect);
    void drawRule(LPainter *p, const LRect<int>& rect);

    enum { Margin = 1, Border = 2, Padding = 4, CodeMargin=8, All=Margin|Border|Padding|CodeMargin, StyleAll=Margin|Border|Padding};
    double boundWidth(LDirection dir = LDirection::ToLeft, int flags = All);
    LMargins<double> boundWidths(int flags = All);

    int applyForeground(LPainter *p);

    bool hasBackground() const;
    bool hasForeground() const;
    bool hasBox() const;
    bool hasBorder() const;
    bool hasBorderImage() const;
    bool hasImage() const;

    bool hasNativeBorder() const;
    bool hasDrawable() const { return !hasNativeBorder() || hasBackground() || hasImage(); }

    LRect<double> borderRect(const LRect<double>& r) const;
    LRect<double> paddingRect(const LRect<double>& r) const;
    LRect<double> contentsRect(const LRect<double>& r) const;


    LRect<double> boxRect(const LRect<double>& r, int flags) const;
    LRect<double> originRect(const LRect<double> &r, lcss::Origin origin) const;

    void setClip(LPainter *p, const LRect<double> &rect);
    void unsetClip(LPainter *p);
    std::shared_ptr<LPainterPath> borderClip(const LRect<double> &rect);
    const std::shared_ptr<LPalette>& palette();
};

class LPalette;
class LKWIDGETSDEF LDrawUtil
{
public:
    LDrawUtil();

    static void drawPlainRect(LPainter *p,
                              LRect<int> r,
                              const LColor &,
                              int lineWidth = 1,
                              const std::shared_ptr<LBrush> fill = nullptr);

    static void drawShadeLine(LPainter *p, const LPoint<int> &p1, const LPoint<int> &p2,
                              const std::shared_ptr<LPalette> pal, bool sunken = true,
                              int lineWidth = 1, int midLineWidth = 0);


    static void drawShadeRect(LPainter *p, const LRect<int>&  r,
                              const std::shared_ptr<LPalette> pal,
                              bool sunken = false,
                              int lineWidth = 1, int midLineWidth = 0,
                              const std::shared_ptr<LBrush> fill = nullptr);

    static void drawShadePanel(LPainter *p, const LRect<int> &r,
                               const std::shared_ptr<LPalette> pal, bool sunken = false,
                               int lineWidth = 1, const std::shared_ptr<LBrush> fill = nullptr);

    static void drawWinPanel(LPainter *p, const LRect<int> &r,
                             const std::shared_ptr<LPalette> pal, bool sunken = false,
                             const std::shared_ptr<LBrush> fill = nullptr);

    static void drawWinShades(LPainter *p,
                              const LRect<int> &r,
                              const LColor &c1, const LColor &c2,
                              const LColor &c3, const LColor &c4,
                              const std::shared_ptr<LBrush> fill = nullptr);

    static void drawWinButton(LPainter *p, const LRect<int> &r,
                              const std::shared_ptr<LPalette> pal, bool sunken = false,
                              const std::shared_ptr<LBrush> fill = nullptr);


    static void normalizeRadii(const LRect<double> &br,
                               const std::array<LSize<double>, 4>& radii,
                               LSize<double>& tlr, LSize<double>& trr, LSize<double>& blr, LSize<double>& brr);

    static void drawEdge(LPainter *p,
                         double x1, double y1, double x2, double y2,
                         double dw1, double dw2,
                         LDirection edge, lcss::BorderStyle style, LColor c);

    static void drawRoundedCorners(LPainter *p,
                             double x1, double y1, double x2, double y2,
                             const LSize<double>& r1, const LSize<double>& r2,
                             LDirection edge, lcss::BorderStyle s, LColor c);

    static void drawBorder(LPainter *p, const LRect<double> &rect,
                        const std::array<lcss::BorderStyle,4>& styles,
                        const std::array<double,4>& borders,
                        const std::array<LColor,4>& colors,
                        const std::array<LSize<double>, 4>& radii);


    static void drawBorderPixmap(LPainter *painter,
                                    const LRect<int>& targetRect,
                                    const LMargins<int> &targetMargins,
                                    const std::shared_ptr<LPixmap> pixmap,
                                    const LRect<int> &sourceRect,
                                    const LMargins<int> &sourceMargins,
                                    const LTileRules &rules = LTileRules(),
                                    LFlags<LDrawBorderHint> drawHints = 0);


    template<typename T>
    static LRect<T> visualRect(LLayoutDirection direction,
                               const LRect<T> &boundingRect,
                               const LRect<T> &logicalRect){
        if (direction == LLayoutDirection::LeftToRight) {
            return logicalRect;
        }
        LRect<T> rect = logicalRect;
        rect.translate(2 * (boundingRect.x2() - logicalRect.x2()) +
                           logicalRect.width() - boundingRect.width(),
                       0);
        return rect;
    }

    template<typename T>
    static LPoint<T> visualPos(LLayoutDirection direction,
                        const LRect<T> &boundingRect,
                        const LPoint<T> &logicalPos)
    {
        if (direction == LLayoutDirection::LeftToRight){
            return logicalPos;
        }
        return LPoint<T>(boundingRect.x2() - logicalPos.m_x, logicalPos.m_y);
    }


    static LRect<int> alignedRect(LLayoutDirection direction, LFlags<LAlignment> alignment, const LSize<int> &size, const LRect<int> &rectangle);
};

LK_NAMESPACE_END

#endif // LDRAWUTIL_H
