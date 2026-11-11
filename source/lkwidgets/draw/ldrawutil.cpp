#include "ldrawutil.h"
#include "lkcore/ldebug.h"
#include "lkgui/lpalette.h"

LK_NAMESPACE_BEGIN
class LPainterStateGuard {
public:
    explicit LPainterStateGuard(LPainter *p) : m_painter(p) {}
    ~LPainterStateGuard()
    {
        for ( ; m_level > 0; --m_level){
            m_painter->restore();
        }
    }

    void save()
    {
        m_painter->save();
        ++m_level;
    }

    void restore()
    {
        m_painter->restore();
        --m_level;
    }

private:
    LPainter *m_painter;
    int m_level= 0;
};


// Determines if Edge e1 draws over Edge e2. Depending on this trapezoids or rectangles are drawn
static bool lPaintsOver(const std::array<lcss::BorderStyle, 4>& styles, const std::array<LColor, 4> colors, LDirection e1, LDirection e2)
{
    const lcss::BorderStyle& s2 = styles[(uint8_t)e2];
    if (s2 == lcss::BorderStyle::None || colors[(uint8_t)e2].isTransparent()){
        return true;
    }

    const lcss::BorderStyle& s1 = styles[(uint8_t)e1];
    if ((s1 == lcss::BorderStyle::Solid && s2 == lcss::BorderStyle::Solid) && (colors[(uint8_t)e1] == colors[(uint8_t)e2])
        && colors[(uint8_t)e1].isOpaque()) {
        return true;
    }
    return false;
}

static std::shared_ptr<LPen> lPenFromStyle(const LColor& c, double width, lcss::BorderStyle style)
{
    LPenStyle ps = LPenStyle::No;

    switch (style) {
    case lcss::BorderStyle::Dotted:{
        ps  = LPenStyle::Dot;
        break;
    }
    case lcss::BorderStyle::Dashed:{
        ps = lFuzzyIsZero(width-1) ? LPenStyle::Dot : LPenStyle::Dash;
        break;
    }
    case lcss::BorderStyle::DotDash:{
        ps = LPenStyle::DashDot;
        break;
    }
    case lcss::BorderStyle::DotDotDash:{
        ps = LPenStyle::DashDotDot;
        break;
    }
    case lcss::BorderStyle::Inset:
    case lcss::BorderStyle::Outset:
    case lcss::BorderStyle::Solid:
        ps = LPenStyle::Solid;
        break;
    default:
        break;
    }

    std::shared_ptr<LPen> pen = std::make_shared<LPen>(c, width, ps);
    return pen;
}



LRenderRule::LRenderRule(std::shared_ptr<lcss::StyleComputed> data)
{
    LCssBuilder builder(data);
    m_borderImage = builder.buildBorderImage();
    m_border = builder.buildBorderData();
    m_box = builder.buildBoxData();
    m_background = builder.buildBackgroundData();
    m_foreground = builder.buildForegroundData();
}

void LRenderRule::drawBorder(LPainter *p, const LRect<int> &rect)
{
    if (!hasBorder()){
        return;
    }

    if (hasBorderImage()) {
        drawBorderImage(p, rect);
        return;
    }

    bool wasAntialiased = p->renderHints() & LRenderHint::Antialiasing;
    p->setRenderHints(LRenderHint::Antialiasing);

    LRect<double> br = rect.toRectF();
    LDrawUtil::drawBorder(p, br,
                          m_border->styles,
                          m_border->borders,
                          m_border->colors,
                          m_border->radiis);
    p->setRenderHints(LRenderHint::Antialiasing, wasAntialiased);
}

void LRenderRule::drawBorderImage(LPainter *p, const LRect<int> &rect)
{
    if(m_borderImage == nullptr || m_border == nullptr || m_borderImage->texture == nullptr){
        return;
    }

    const std::shared_ptr<LPixmap>  borderImageData = m_borderImage->texture->getNormal();
    if(borderImageData != nullptr){
        setClip(p, rect.toRectF());

        const std::array<double, 4>& sourceBorders = m_borderImage->slices;
        const std::array<double, 4>& targetBorders = m_border->borders;
        LMargins<double> sourceMargins(sourceBorders[(uint8_t)LDirection::ToLeft], sourceBorders[(uint8_t)LDirection::ToTop],
                                       sourceBorders[(uint8_t)LDirection::ToRight], sourceBorders[(uint8_t)LDirection::ToBottom]);
        LMargins<double> targetMargins(targetBorders[(uint8_t)LDirection::ToLeft], targetBorders[(uint8_t)LDirection::ToTop],
                               targetBorders[(uint8_t)LDirection::ToRight], targetBorders[(uint8_t)LDirection::ToBottom]);

        bool wasSmoothPixmapTransform = p->renderHints() & LRenderHint::SmoothPixmapTransform;
        p->setRenderHints(LRenderHint::SmoothPixmapTransform);

        LDrawUtil::drawBorderPixmap(p, rect, targetMargins.toMargin(),
                                    borderImageData, borderImageData->rect(), sourceMargins.toMargin(),
                                    LTileRules(m_borderImage->horizRepeat, m_borderImage->vertRepeat));
        p->setRenderHints(LRenderHint::SmoothPixmapTransform, wasSmoothPixmapTransform);
        unsetClip(p);
    }
}

void LRenderRule::drawBackground(LPainter *p, const LRect<int> &rect, const LPoint<int> &pt)
{
    if (m_background != nullptr) {

        std::shared_ptr<LBrush> brush = nullptr;
        if(m_background->texture == nullptr || m_background->texture->isEmpty()){
            if(!m_background->color.isTransparent()){
                brush = std::make_shared<LBrush>(m_background->color);
            }
        }else{
            if(m_background->texture->isGradientUse()){
                brush = std::make_shared<LBrush>(m_background->texture->getGradient());
            }else{
                drawBackgroundImage(p, rect, pt);
            }
        }

        if(brush != nullptr){
            LRect<double> orect = originRect(rect.toRectF(), m_background->origin);
            const std::shared_ptr<LPainterPath> borderPath = borderClip(orect);
            if (borderPath != nullptr && (borderPath->isEmpty() == false)) {
                // Drawn instead of being used as clipping path for better visual quality
                bool wasAntialiased = p->renderHints() & LRenderHint::Antialiasing;
                p->setRenderHints(LRenderHint::Antialiasing);
                p->fillPath(borderPath, brush);
                p->setRenderHints(LRenderHint::Antialiasing, wasAntialiased);
            } else {
                p->fillRect(orect, brush);
            }
        }
    }
}

void LRenderRule::drawBackgroundImage(LPainter *p, const LRect<int> &r, LPoint<int> off)
{
    if (m_background != nullptr) {

        if(m_background->texture != nullptr){
            std::shared_ptr<LPixmap>  bgp = m_background->texture->getNormal();
            if(bgp != nullptr){
                LRect<double> rect = r.toRectF();
                setClip(p, borderRect(rect));

                if (m_background->origin != m_background->clip) {
                    p->save();
                    p->setClipRect(originRect(rect, m_background->clip), LClipOper::Intersect);
                }

                if (m_background->attachment == lcss::Attachment::Fixed){
                    off.reset();
                }

                LSize<int> bgpSize = bgp->size() / bgp->devicePixelRatio();
                int bgpHeight = bgpSize.height();
                int bgpWidth = bgpSize.width();
                LRect<int> r = originRect(rect, m_background->origin).toRect();
                LRect<int> aligned = LDrawUtil::alignedRect(LLayoutDirection::LeftToRight, m_background->position, bgpSize, r);
                LRect<int> inter = aligned.translated(-off).intersected(r);

                switch (m_background->repeat) {
                case lcss::Repeat::Y:{
                    p->drawTiledPixmap(LRect<double>(inter.m_x, r.m_y, inter.width(), r.height(), false),
                                       bgp,
                                       LPoint<double>(inter.m_x - aligned.m_x + off.m_x,
                                                    bgpHeight - int(aligned.m_y - r.m_y) % bgpHeight + off.m_y));
                    break;
                }
                case lcss::Repeat::X:{
                    p->drawTiledPixmap(LRect<double>(r.m_x, inter.m_y, r.width(), inter.height(), false),
                                       bgp,
                                       LPoint<double>(bgpWidth - int(aligned.m_x - r.m_x)%bgpWidth + off.m_x,
                                                    inter.m_y - aligned.m_y + off.m_y));
                    break;
                }
                case lcss::Repeat::XY:
                    p->drawTiledPixmap(rect, bgp,
                                       LPoint<double>(bgpWidth - int(aligned.m_x - r.m_x)% bgpWidth + off.m_x,
                                              bgpHeight - int(aligned.m_y - r.m_y)%bgpHeight + off.m_y));
                    break;
                case lcss::Repeat::None:
                default:
                    p->drawPixmap(LPoint<double>(inter.m_x, inter.m_y),
                                  bgp,
                                  LRect<double>(inter.m_x - aligned.m_x + off.m_x,
                                              inter.m_y - aligned.m_y + off.m_y,
                                              bgp->width() ,
                                              bgp->height(), false));
                    break;
                }


                if (m_background->origin != m_background->clip){
                    p->restore();
                }
                unsetClip(p);
            }
        }
    }
}

void LRenderRule::drawFrame(LPainter *p, const LRect<int> &rect)
{
    drawBackground(p, rect);
    if (hasBorder()){
        drawBorder(p, borderRect(rect.toRectF()).toRect());
    }
}

void LRenderRule::drawImage(LPainter *p, const LRect<int> &rect)
{
    if (!hasImage()){
        return;
    }
    m_image->texture->paint(p, rect, m_image->alignment);
}

void LRenderRule::drawRule(LPainter *p, const LRect<int> &rect)
{
    drawFrame(p, rect);
    drawImage(p, contentsRect(rect.toRectF()).toRect());
}

double LRenderRule::boundWidth(LDirection dir, int flags)
{
    double ret = 0;
    if(m_box != nullptr){
        if(flags &Margin){
            ret += m_box->margins[(uint8_t)dir];
        }

        if(flags &Padding){
            ret += m_box->paddings[(uint8_t)dir];
        }
    }

    if(m_border != nullptr){
        if(flags &Border){
            ret += m_border->borders[(uint8_t)dir];
        }
    }
    return ret;
}

LMargins<double> LRenderRule::boundWidths(int flags)
{
    LMargins<double> ret;
    if(m_box != nullptr){
        if(flags &Margin){
            ret.m_top += m_box->margins[(uint8_t)LDirection::ToTop];
            ret.m_right += m_box->margins[(uint8_t)LDirection::ToRight];
            ret.m_bottom += m_box->margins[(uint8_t)LDirection::ToBottom];
            ret.m_left += m_box->margins[(uint8_t)LDirection::ToLeft];
        }

        if(flags &Padding){
            ret.m_top += m_box->paddings[(uint8_t)LDirection::ToTop];
            ret.m_right += m_box->paddings[(uint8_t)LDirection::ToRight];
            ret.m_bottom += m_box->paddings[(uint8_t)LDirection::ToBottom];
            ret.m_left += m_box->paddings[(uint8_t)LDirection::ToLeft];
        }
    }

    if(m_border != nullptr){
        if(flags &Border){
            ret.m_top += m_border->borders[(uint8_t)LDirection::ToTop];
            ret.m_right += m_border->borders[(uint8_t)LDirection::ToRight];
            ret.m_bottom += m_border->borders[(uint8_t)LDirection::ToBottom];
            ret.m_left += m_border->borders[(uint8_t)LDirection::ToLeft];
        }
    }
    return ret;
}


int LRenderRule::applyForeground(LPainter *p)
{
    int ret = 0;
    if(m_foreground!=nullptr){
        if(!m_foreground->color.isTransparent()){
            p->setPen(m_foreground->color);
            ret |=1;
        }

        if(m_foreground->font != nullptr){
            p->setFont(m_foreground->font);
            ret |=2;
        }
    }
    return ret;
}

bool LRenderRule::hasBackground() const
{
    return m_background != nullptr;
}

bool LRenderRule::hasForeground() const
{
    return m_foreground != nullptr;
}

bool LRenderRule::hasBox() const
{
    return m_box != nullptr;
}

bool LRenderRule::hasBorder() const
{
    return m_border != nullptr;
}

bool LRenderRule::hasBorderImage() const
{
    if(m_border != nullptr && m_borderImage != nullptr){
        return m_borderImage->texture != nullptr;
    }
    return false;
}

bool LRenderRule::hasImage() const
{
    if(m_image != nullptr){
        return m_image->texture != nullptr;
    }
    return false;
}

bool LRenderRule::hasNativeBorder() const
{
    if(m_border == nullptr){
        return true;
    }else if(m_borderImage == nullptr){
        return m_border->styles[0] == lcss::BorderStyle::Native;
    }
    return false;
}

LRect<double> LRenderRule::borderRect(const LRect<double> &r) const
{
    if (!hasBox()){
        return r;
    }
    const std::array<double, 4>& m = m_box->margins;
    return r.adjusted(m[(uint8_t)LDirection::ToLeft],
                      m[(uint8_t)LDirection::ToTop],
                      -m[(uint8_t)LDirection::ToRight],
                      -m[(uint8_t)LDirection::ToBottom]);
}

LRect<double> LRenderRule::paddingRect(const LRect<double> &r) const
{
    LRect<double> br = borderRect(r);
    if (!hasBorder()){
        return br;
    }
    const std::array<double, 4>& b = m_border->borders;
    return br.adjusted(b[(uint8_t)LDirection::ToLeft],
                       b[(uint8_t)LDirection::ToTop],
                      -b[(uint8_t)LDirection::ToRight],
                      -b[(uint8_t)LDirection::ToBottom]);
}

LRect<double> LRenderRule::contentsRect(const LRect<double> &r) const
{
    LRect<double> pr = paddingRect(r);
    if (!hasBox()){
        return pr;
    }
    const std::array<double, 4>& p = m_box->paddings;
    return pr.adjusted(p[(uint8_t)LDirection::ToLeft],
                       p[(uint8_t)LDirection::ToTop],
                      -p[(uint8_t)LDirection::ToRight],
                      -p[(uint8_t)LDirection::ToBottom]);
}

LRect<double> LRenderRule::boxRect(const LRect<double> &cr, int flags) const
{
    LRect<double> r = cr;
    if (hasBox()) {
        if (flags & Margin) {
            const std::array<double, 4>& m = m_box->margins;
            r.adjust(-m[(uint8_t)LDirection::ToLeft],
                     -m[(uint8_t)LDirection::ToTop],
                     m[(uint8_t)LDirection::ToRight],
                     m[(uint8_t)LDirection::ToBottom]);

        }
        if (flags & Padding) {
            const std::array<double, 4>& p = m_box->paddings;
            r.adjust(-p[(uint8_t)LDirection::ToLeft],
                     -p[(uint8_t)LDirection::ToTop],
                     p[(uint8_t)LDirection::ToRight],
                     p[(uint8_t)LDirection::ToBottom]);
        }
    }
    if (hasBorder() && (flags & Border)) {
        const std::array<double, 4>& b = m_border->borders;
        r.adjust(-b[(uint8_t)LDirection::ToLeft],
                 -b[(uint8_t)LDirection::ToTop],
                 b[(uint8_t)LDirection::ToRight],
                 b[(uint8_t)LDirection::ToBottom]);
    }
    return r;
}

LRect<double> LRenderRule::originRect(const LRect<double> &rect, lcss::Origin origin) const
{
    switch (origin) {
    case lcss::Origin::Padding:{
        return paddingRect(rect);
    }
    case lcss::Origin::Border:{
        return borderRect(rect);
    }
    case lcss::Origin::Content:{
        return contentsRect(rect);
    }
    case lcss::Origin::Margin:
    default:{
        return rect;
    }
    }
}

void LRenderRule::setClip(LPainter *p, const LRect<double> &rect)
{
    if (m_clipset++){
        return;
    }
    m_clipPath = borderClip(rect);
    if (m_clipPath != nullptr) {
        if(!m_clipPath->isEmpty()){
            p->save();
            p->setClipPath(m_clipPath, LClipOper::Intersect);
        }
    }
}

void LRenderRule::unsetClip(LPainter *p)
{
    if (--m_clipset){
        return;
    }
    if(m_clipPath != nullptr){
        if (!m_clipPath->isEmpty()){
            p->restore();
        }
    }
}

std::shared_ptr<LPainterPath> LRenderRule::borderClip(const LRect<double> &r)
{
    if (!hasBorder()){
        return nullptr;
    }

    LSize<double> tlr, trr, blr, brr;
    LDrawUtil::normalizeRadii(r, m_border->radiis, tlr, trr, blr, brr);
    if (tlr.isNull() && trr.isNull() && blr.isNull() && brr.isNull()){
        return nullptr;
    }

    const LRect<double> rect(r);
    const std::array<double, 4>& borders = m_border->borders;
    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    double curY = rect.m_y + borders[(uint8_t)LDirection::ToTop]/2.0;
    path->moveTo(rect.m_x + tlr.width(), curY);
    path->lineTo(rect.x2() - trr.width(), curY);
    double curX = rect.x2() - borders[(uint8_t)LDirection::ToRight]/2.0;
    path->arcTo(curX - 2*trr.width() + borders[(uint8_t)LDirection::ToRight],
                curY,
                trr.width()*2 - borders[(uint8_t)LDirection::ToRight],
                trr.height()*2 - borders[(uint8_t)LDirection::ToTop], 90, -90);

    path->lineTo(curX, rect.y2() - brr.height());
    curY = rect.y2() - borders[(uint8_t)LDirection::ToBottom]/2.0;
    path->arcTo(curX - 2*brr.width() + borders[(uint8_t)LDirection::ToRight],
                curY - 2*brr.height() + borders[(uint8_t)LDirection::ToBottom],
                brr.width()*2 - borders[(uint8_t)LDirection::ToRight],
                brr.height()*2 - borders[(uint8_t)LDirection::ToBottom], 0, -90);

    path->lineTo(rect.m_x + blr.width(), curY);
    curX = rect.m_x + borders[(uint8_t)LDirection::ToLeft]/2.0;
    path->arcTo(curX,
                rect.y2() - 2*blr.height() + borders[(uint8_t)LDirection::ToBottom]/2.0,
                blr.width()*2 - borders[(uint8_t)LDirection::ToLeft],
                blr.height()*2 - borders[(uint8_t)LDirection::ToBottom], 270, -90);

    path->lineTo(curX, rect.m_y + tlr.height());
    path->arcTo(curX,
                rect.m_y + borders[(uint8_t)LDirection::ToTop]/2.0,
                tlr.width()*2 - borders[(uint8_t)LDirection::ToLeft],
                tlr.height()*2 - borders[(uint8_t)LDirection::ToTop], 180, -90);

    path->closeSubpath();
    return path;
}

LDrawUtil::LDrawUtil()
{

}

void LDrawUtil::drawPlainRect(LPainter *p,
                              LRect<int> r,
                              const LColor &c,
                              int lineWidth,
                              const std::shared_ptr<LBrush> fill)
{
    if(p == nullptr || p->device() == nullptr){
        lWarn() << "Invalid painter!";
        return;
    }

    if (r.m_width <= 0 || r.m_height <= 0 || lineWidth < 0) {
        lWarn() << "drawPlainRect: Invalid parameters";
        return;
    }

    LPainterStateGuard painterGuard(p);
    const double devicePixelRatio = p->device()->devicePixelRatioF();
    if (!lFuzzyCompare(devicePixelRatio, double(1))) {
        painterGuard.save();
        const double inverseScale = double(1) / devicePixelRatio;
        p->scale(inverseScale, inverseScale);
        r.m_x = std::lround(devicePixelRatio * r.m_x);
        r.m_y = std::lround(devicePixelRatio * r.m_y);
        r.m_width = std::lround(devicePixelRatio * r.m_width);
        r.m_height = std::lround(devicePixelRatio * r.m_height);
        lineWidth = std::lround(devicePixelRatio * lineWidth);
    }

    std::shared_ptr<LPen> oldPen = p->pen();
    LColor oldColor = oldPen->color();
    LPenStyle oldPenStyle = oldPen->m_style;

    std::shared_ptr<LBrush> oldBrush = p->brush();
    LBrush::Style oldBrushStyle = oldBrush->m_style;

    p->setPen(c);
    p->setBrush(LBrush::NoPattern);

    r.m_height-=1;
    r.m_width-=1;
    for (int i=0; i<lineWidth; i++){
        if(r.m_width <= 0 || r.m_height <= 0){
            break;
        }

        p->drawRect(r);
        r.m_x++;
        r.m_y++;
        r.m_width-=2;
        r.m_height-=2;
    }

    if (fill != nullptr) {
        r.m_height+=1;
        r.m_width+=1;
        if(r.m_width > 0 && r.m_height > 0){
            p->setPen(LPenStyle::No);
            p->setBrush(fill);
            p->drawRect(r);
        }
    }

    oldBrush->m_style = oldBrushStyle;
    p->setBrush(oldBrush);
    p->setPen(oldColor, oldPenStyle);
}

void LDrawUtil::drawShadeLine(LPainter *p,
                              const LPoint<int> &p1,
                              const LPoint<int> &p2,
                              const std::shared_ptr<LPalette> pal,
                              bool sunken,
                              int lineWidth,
                              int midLineWidth)
{
    if(p == nullptr || p->device() == nullptr){
        lWarn() << "Invalid painter!";
        return;
    }

    if (L_UNLIKELY(!p || lineWidth < 0 || midLineWidth < 0)) {
        lWarn() << "qDrawShadeLine: Invalid parameters";
        return;
    }
    int tlw = lineWidth*2 + midLineWidth;        // total line width
    LColor oldColor = p->pen()->color(); // save pen color
    if (sunken){
        p->setPen(pal->color(LPalette::Dark));
    }else{
        p->setPen(pal->light()->m_color);
    }
    int x1 = p1.m_x, y1 = p1.m_y, x2 = p2.m_x, y2 = p2.m_y;

    LPolygon<double> a;
    a.resize(3);
    int i;
    if (y1 == y2) {                                // horizontal line
        int y = y1 - tlw/2;
        if (x1 > x2) {                        // swap x1 and x2
            int t = x1;
            x1 = x2;
            x2 = t;
        }
        x2--;
        for (i=0; i<lineWidth; i++) {                // draw top shadow
            a[0].m_x = x1+i;
            a[0].m_y = y+tlw-1-i;
            a[1].m_x = a[0].m_x;
            a[1].m_y = y+i;
            a[2].m_x = x2-i;
            a[2].m_y = a[1].m_y;
            p->drawPolyline(a);
        }
        if (midLineWidth > 0) {
            p->setPen(pal->mid()->m_color);
            for (i=0; i<midLineWidth; i++){        // draw lines in the middle
                p->drawLine(LLine<double>(x1+lineWidth, y+lineWidth+i,
                                          x2-lineWidth, y+lineWidth+i));
            }
        }
        if (sunken){
            p->setPen(pal->light()->m_color);
        }else{
            p->setPen(pal->dark()->m_color);
        }
        for (i=0; i<lineWidth; i++) {                // draw bottom shadow
            a[0].m_x = x1+i;
            a[0].m_y = y+tlw-1-i;
            a[1].m_x = x2-i;
            a[1].m_y = a[0].m_y;
            a[2].m_x = a[1].m_x;
            a[2].m_y = y+i;

            p->drawPolyline(a);
        }
    }
    else if (x1 == x2) {                        // vertical line
        int x = x1 - tlw/2;
        if (y1 > y2) {                        // swap y1 and y2
            int t = y1;
            y1 = y2;
            y2 = t;
        }
        y2--;
        for (i=0; i<lineWidth; i++) {                // draw left shadow
            a[0].m_x = x+i;
            a[0].m_y = y2;
            a[1].m_x = a[0].m_x;
            a[1].m_y = y1+i;
            a[2].m_x = x+tlw-1;
            a[2].m_y = a[1].m_y;

            p->drawPolyline(a);
        }
        if (midLineWidth > 0) {
            p->setPen(pal->mid()->m_color);
            for (i=0; i<midLineWidth; i++){        // draw lines in the middle
                p->drawLine(LLine<double>(x+lineWidth+i, y1+lineWidth, x+lineWidth+i, y2));
            }
        }
        if (sunken){
            p->setPen(pal->light()->m_color);
        }else{
            p->setPen(pal->dark()->m_color);
        }
        for (i=0; i<lineWidth; i++) {                // draw right shadow
            a[0].m_x = x+lineWidth;
            a[0].m_y = y2-i;
            a[1].m_x = x+tlw-i-1;
            a[1].m_y = a[0].m_y;
            a[2].m_x = a[1].m_x;
            a[2].m_y = y1+lineWidth;

            p->drawPolyline(a);
        }
    }
    p->setPen(oldColor);
}

void LDrawUtil::drawShadeRect(LPainter *p,
                              const LRect<int>& r,
                              const std::shared_ptr<LPalette> pal,
                              bool sunken,
                              int lineWidth,
                              int midLineWidth,
                              const std::shared_ptr<LBrush> fill)
{
    if(p == nullptr || p->device() == nullptr){
        lWarn() << "Invalid painter!";
        return;
    }

    if (r.m_width <= 0 || r.m_height <= 0 || lineWidth < 0 || midLineWidth < 0) {
        lWarn() << "drawShadeRect: Invalid parameters";
        return;
    }

    int x1=r.m_x, y1=r.m_y, w=r.m_width, h=r.m_height;

    LPainterStateGuard painterGuard(p);
    const double devicePixelRatio = p->device()->devicePixelRatioF();
    if (!lFuzzyCompare(devicePixelRatio, double(1))) {
        painterGuard.save();
        const double inverseScale = double(1) / devicePixelRatio;
        p->scale(inverseScale, inverseScale);
        x1 = std::lround(devicePixelRatio * x1);
        y1 = std::lround(devicePixelRatio * y1);
        w = std::lround(devicePixelRatio * w);
        h = std::lround(devicePixelRatio * h);
        lineWidth = std::lround(devicePixelRatio * lineWidth);
        midLineWidth = std::lround(devicePixelRatio * midLineWidth);
    }
    int x2 = x1+w, y2 = y1+h;

    std::shared_ptr<LPen> oldPen = p->pen();
    LColor oldPenColor = oldPen->color();
    LPenStyle oldPenStyle = oldPen->m_style;
    if (sunken){
        p->setPen(pal->dark()->m_color);
    }else{
        p->setPen(pal->light()->m_color);
    }

    if (lineWidth == 1 && midLineWidth == 0) {// standard shade rectangle
        LRect<int> t(x1, y1, LSize<int>(w-1, h-1));
        if(t.m_height >0 && t.m_width>0){
            p->drawRect(t);
        }
        if (sunken){
            p->setPen(pal->light()->m_color);
        }else{
            p->setPen(pal->dark()->m_color);
        }
        LLine<double> lines[4] = { LLine<double>(x1+1, y1+1, x2-2, y1+1),
                           LLine<double>(x1+1, y1+2, x1+1, y2-2),
                           LLine<double>(x1, y2, x2, y2),
                           LLine<double>(x2,y1, x2,y2-1) };
        p->drawLines(lines, 4);              // draw bottom/right lines
    } else {                                        // more complicated
        int m = lineWidth+midLineWidth;
        int i, j=0, k=m;
        for (i=0; i<lineWidth; i++) {                // draw top shadow
            LLine<double> lines[4] = { LLine<double>(x1+i, y2-i, x1+i, y1+i),
                               LLine<double>(x1+i, y1+i, x2-i, y1+i),
                               LLine<double>(x1+k, y2-k, x2-k, y2-k),
                               LLine<double>(x2-k, y2-k, x2-k, y1+k) };
            p->drawLines(lines, 4);
            k++;
        }
        p->setPen(pal->mid()->m_color);
        j = lineWidth*2;
        LRect<int> t(x1+lineWidth, y1+lineWidth, w-j-1, h-j-1, false);
        for (i=0; i<midLineWidth; i++) {        // draw lines in the middle
            if(t.m_width>0 && t.m_height>0){
                p->drawRect(t);
                t.m_x++;
                t.m_y++;
                t.m_width-=2;
                t.m_height-=2;
            }else{
                break;
            }
        }
        if (sunken){
            p->setPen(pal->light()->m_color);
        }else{
            p->setPen(pal->dark()->m_color);
        }
        k = m;
        for (i=0; i<lineWidth; i++) {                // draw bottom shadow
            LLine<double> lines[4] = { LLine<double>(x1+1+i, y2-i, x2-i, y2-i),
                                       LLine<double>(x2-i, y2-i, x2-i, y1+i+1),
                                       LLine<double>(x1+k, y2-k, x1+k, y1+k),
                                       LLine<double>(x1+k, y1+k, x2-k, y1+k) };
            p->drawLines(lines, 4);
            k++;
        }
    }
    if (fill!= nullptr) {
        std::shared_ptr<LBrush> oldBrush = p->brush();
        int tlw = lineWidth + midLineWidth;
        p->setPen(LPenStyle::No);
        p->setBrush(fill);
        p->drawRect(LRect<int>(x1+tlw, y1+tlw, w-2*tlw, h-2*tlw, false));
        p->setBrush(oldBrush);
    }
    p->setPen(oldPenColor, oldPenStyle);   // restore pen
}

void LDrawUtil::drawShadePanel(LPainter *p,
                               const LRect<int> &r,
                               const std::shared_ptr<LPalette> pal,
                               bool sunken,
                               int lineWidth,
                               const std::shared_ptr<LBrush> fill)
{
    if(p == nullptr || p->device() == nullptr){
        lWarn() << "Invalid painter!";
        return;
    }

    if (r.m_width <= 0 || r.m_height <= 0 || lineWidth < 0) {
        lWarn() << "drawPlainRect: Invalid parameters";
        return;
    }

    LPainterStateGuard painterGuard(p);
    const double devicePixelRatio = p->device()->devicePixelRatioF();

    int x= r.m_x, y = r.m_y, w = r.m_width, h = r.m_height;
    if (!lFuzzyCompare(devicePixelRatio, double(1))) {
        painterGuard.save();
        const double inverseScale = double(1) / devicePixelRatio;
        p->scale(inverseScale, inverseScale);
        x = std::lround(devicePixelRatio * x);
        y = std::lround(devicePixelRatio * y);
        w = std::lround(devicePixelRatio * w);
        h = std::lround(devicePixelRatio * h);
        lineWidth = std::lround(devicePixelRatio * lineWidth);
    }

    LColor shade = pal->dark()->m_color;
    LColor light = pal->light()->m_color;
    if (fill != nullptr) {
        if (fill->m_color == shade){
            shade = pal->shadow()->m_color;
        }
        if (fill->m_color == light){
            light = pal->midlight()->m_color;
        }
    }

    LColor oldColor = p->pen()->color();
    std::vector<LLine<double>> lines;
    lines.reserve(2*lineWidth);

    if (sunken){
        p->setPen(shade);
    }else{
        p->setPen(light);
    }
    int x1, y1, x2, y2;
    int i;
    x1 = x;
    y1 = y2 = y;
    x2 = x+w-2;
    for (i=0; i<lineWidth; i++) {                // top shadow
        lines.push_back(LLine<double>(x1, y1++, x2--, y2++));
    }
    x2 = x1;
    y1 = y+h-2;
    for (i=0; i<lineWidth; i++) {                // left shado
        lines.push_back(LLine<double>(x1++, y1, x2++, y2--));
    }
    p->drawLines(lines.data(), lines.size());
    lines.clear();
    if (sunken){
        p->setPen(light);
    }else{
        p->setPen(shade);
    }
    x1 = x;
    y1 = y2 = y+h-1;
    x2 = x+w-1;
    for (i=0; i<lineWidth; i++) {                // bottom shadow
        lines.push_back(LLine<double>(x1++, y1--, x2, y2--));
    }
    x1 = x2;
    y1 = y;
    y2 = y+h-lineWidth-1;
    for (i=0; i<lineWidth; i++) {                // right shadow
        lines.push_back(LLine<double>(x1--, y1++, x2--, y2));
    }
    p->drawLines(lines.data(), lines.size());
    if (fill!= nullptr){                                // fill with fill color
        p->fillRect(LRect<double>(x+lineWidth, y+lineWidth, w-lineWidth*2, h-lineWidth*2, false), fill);
    }
    p->setPen(oldColor);
}


void LDrawUtil::drawWinPanel(LPainter *p,
                             const LRect<int> &r,
                             const std::shared_ptr<LPalette> pal,
                             bool sunken,
                             const std::shared_ptr<LBrush> fill)
{
    if (sunken){
        drawWinShades(p, r,
                      pal->dark()->m_color, pal->light()->m_color, pal->shadow()->m_color,
                      pal->midlight()->m_color, fill);
    }else{
        drawWinShades(p, r,
                      pal->light()->m_color, pal->shadow()->m_color, pal->midlight()->m_color,
                      pal->dark()->m_color, fill);
    }
}

void LDrawUtil::drawWinShades(LPainter *p,
                              const LRect<int> &r,
                              const LColor &c1, const LColor &c2,
                              const LColor &c3, const LColor &c4,
                              const std::shared_ptr<LBrush> fill)
{
    if (r.m_width < 2 || r.m_height < 2){
        return;
    }

    LPainterStateGuard painterGuard(p);
    const double devicePixelRatio = p->device()->devicePixelRatioF();
    int x= r.m_x, y = r.m_y, w = r.m_width, h = r.m_height;
    if (!lFuzzyCompare(devicePixelRatio, double(1))) {
        painterGuard.save();
        const double inverseScale = double(1) / devicePixelRatio;
        p->scale(inverseScale, inverseScale);
        x = std::lround(devicePixelRatio * x);
        y = std::lround(devicePixelRatio * y);
        w = std::lround(devicePixelRatio * w);
        h = std::lround(devicePixelRatio * h);
    }

    LColor oldPenColor = p->pen()->color();
    LPoint<double> a[3] = { LPoint<double>(x, y+h-2), LPoint<double>(x, y), LPoint<double>(x+w-2, y) };
    p->setPen(c1);
    p->drawPolyline(a, 3);
    LPoint<double> b[3] = { LPoint<double>(x, y+h-1), LPoint<double>(x+w-1, y+h-1), LPoint<double>(x+w-1, y) };
    p->setPen(c2);
    p->drawPolyline(b, 3);
    if (w > 4 && h > 4) {
        LPoint<double> c[3] = { LPoint<double>(x+1, y+h-3), LPoint<double>(x+1, y+1), LPoint<double>(x+w-3, y+1) };
        p->setPen(c3);
        p->drawPolyline(c, 3);
        LPoint<double> d[3] = { LPoint<double>(x+1, y+h-2), LPoint<double>(x+w-2, y+h-2), LPoint<double>(x+w-2, y+1) };
        p->setPen(c4);
        p->drawPolyline(d, 3);
        if (fill != nullptr){
            p->fillRect(LRect<double>(x+2, y+2, w-4, h-4, false), fill);
        }
    }
    p->setPen(oldPenColor);
}

void LDrawUtil::drawWinButton(LPainter *p,
                              const LRect<int> &r,
                              const std::shared_ptr<LPalette> pal,
                              bool sunken,
                              const std::shared_ptr<LBrush> fill)
{
    if (sunken){
        drawWinShades(p, r,
                      pal->shadow()->m_color, pal->light()->m_color,
                      pal->dark()->m_color, pal->button()->m_color,
                      fill);
    }else{
        drawWinShades(p, r,
                      pal->light()->m_color, pal->light()->m_color,
                      pal->button()->m_color, pal->dark()->m_color,
                      fill);
    }
}

void LDrawUtil::normalizeRadii(const LRect<double> &br, const std::array<LSize<double>, 4> &radii,
                               LSize<double> &tlr, LSize<double> &trr, LSize<double> &blr, LSize<double> &brr)
{
    tlr = radii[int(LCorner::TopLeft)].expandedTo(LSize<double>(0, 0));
    trr = radii[int(LCorner::TopRight)].expandedTo(LSize<double>(0, 0));
    blr = radii[int(LCorner::BottomLeft)].expandedTo(LSize<double>(0, 0));
    brr = radii[int(LCorner::BottomRight)].expandedTo(LSize<double>(0, 0));

    if (tlr.width() + trr.width() > br.width()){
        tlr = trr = LSize<double>(0, 0);
    }
    if (blr.width() + brr.width() > br.width()){
        blr = brr = LSize<double>(0, 0);
    }
    if (tlr.height() + blr.height() > br.height()){
        tlr = blr = LSize<double>(0, 0);
    }
    if (trr.height() + brr.height() > br.height()){
        trr = brr = LSize<double>(0, 0);
    }
}

void LDrawUtil::drawEdge(LPainter *p, double x1, double y1, double x2, double y2,
                         double dw1, double dw2, LDirection edge, lcss::BorderStyle style, LColor c)
{
    p->save();
    const double width = (edge == LDirection::ToTop || edge == LDirection::ToBottom) ? (y2-y1) : (x2-x1);

    if (width <= 2 && style == lcss::BorderStyle::Double){
        style = lcss::BorderStyle::Solid;
    }

    switch (style) {
    case lcss::BorderStyle::Inset:
    case lcss::BorderStyle::Outset:{
        if ((style == lcss::BorderStyle::Outset && (edge == LDirection::ToTop || edge == LDirection::ToLeft))
            || (style == lcss::BorderStyle::Inset && (edge == LDirection::ToBottom || edge == LDirection::ToRight))){
            c = c.lighter();
        }
    }
    _FALLTHROUGH;
    case lcss::BorderStyle::Solid: {
        p->setPen(LPenStyle::No);
        p->setBrush(c, LBrush::SolidPattern);
        if (lFuzzyIsZero(width-1) || (lFuzzyIsZero(dw1) && lFuzzyIsZero(dw2))) {
            p->drawRect(LRect<double>(x1, y1, x2, y2, true));
        } else { // draw trapezoid
            std::array<LPoint<double>, 4> quad;
            switch (edge) {
            case LDirection::ToTop:
                quad = {LPoint<double>(x1, y1), LPoint<double>(x1 + dw1, y2),
                        LPoint<double>(x2 - dw2, y2), LPoint<double>(x2, y1)};
                break;
            case LDirection::ToBottom:
                quad = {LPoint<double>(x1 + dw1, y1), LPoint<double>(x1, y2),
                        LPoint<double>(x2, y2), LPoint<double>(x2 - dw2, y1)};
                break;
            case LDirection::ToLeft:
                quad = {LPoint<double>(x1, y1), LPoint<double>(x1, y2),
                        LPoint<double>(x2, y2 - dw2), LPoint<double>(x2, y1 + dw1)};
                break;
            case LDirection::ToRight:
                quad = {LPoint<double>(x1, y1 + dw1), LPoint<double>(x1, y2 - dw2),
                        LPoint<double>(x2, y2), LPoint<double>(x2, y1)};
                break;
            default:
                break;
            }
            p->drawConvexPolygon(quad.data(), static_cast<int>(quad.size()));
        }
        break;
    }
    case lcss::BorderStyle::Dotted:
    case lcss::BorderStyle::Dashed:
    case lcss::BorderStyle::DotDash:
    case lcss::BorderStyle::DotDotDash:{
        std::shared_ptr<LPen> pen = lPenFromStyle(c, width, style);
        p->setPen(pen);
        if (lFuzzyIsZero(width-1))
            p->drawLine(LLine<double>(x1, y1, x2 - 1, y2 - 1));
        else if (edge == LDirection::ToTop || edge == LDirection::ToBottom){
            p->drawLine(LLine<double>(x1 + width/2, (y1 + y2)/2, x2 - width/2, (y1 + y2)/2));
        }
        else{
            p->drawLine(LLine<double>((x1+x2)/2, y1 + width/2, (x1+x2)/2, y2 - width/2));
        }
        break;
    }
    case lcss::BorderStyle::Double: {
        int wby3 = std::lround(width/3);
        int dw1by3 = std::lround(dw1/3);
        int dw2by3 = std::lround(dw2/3);
        switch (edge) {
        case LDirection::ToTop:
            LDrawUtil::drawEdge(p, x1, y1, x2, y1 + wby3, dw1by3, dw2by3, LDirection::ToTop, lcss::BorderStyle::Solid, c);
            LDrawUtil::drawEdge(p, x1 + dw1 - dw1by3, y2 - wby3, x2 - dw2 + dw1by3, y2,
                      dw1by3, dw2by3, LDirection::ToTop, lcss::BorderStyle::Solid, c);
            break;
        case LDirection::ToLeft:
            LDrawUtil::drawEdge(p, x1, y1, x1 + wby3, y2, dw1by3, dw2by3, LDirection::ToLeft, lcss::BorderStyle::Solid, c);
            LDrawUtil::drawEdge(p, x2 - wby3, y1 + dw1 - dw1by3, x2, y2 - dw2 + dw2by3, dw1by3, dw2by3,
                      LDirection::ToLeft, lcss::BorderStyle::Solid, c);
            break;
        case LDirection::ToBottom:
            LDrawUtil::drawEdge(p, x1 + dw1 - dw1by3, y1, x2 - dw2 + dw2by3, y1 + wby3, dw1by3, dw2by3,
                      LDirection::ToBottom, lcss::BorderStyle::Solid, c);
            LDrawUtil::drawEdge(p, x1, y2 - wby3, x2, y2, dw1by3, dw2by3, LDirection::ToBottom, lcss::BorderStyle::Solid, c);
            break;
        case LDirection::ToRight:
            LDrawUtil::drawEdge(p, x2 - wby3, y1, x2, y2, dw1by3, dw2by3, LDirection::ToRight, lcss::BorderStyle::Solid, c);
            LDrawUtil::drawEdge(p, x1, y1 + dw1 - dw1by3, x1 + wby3, y2 - dw2 + dw2by3, dw1by3, dw2by3,
                      LDirection::ToRight, lcss::BorderStyle::Solid, c);
            break;
        default:
            break;
        }
        break;
    }
    case lcss::BorderStyle::Ridge:
    case lcss::BorderStyle::Groove: {
        lcss::BorderStyle s1, s2;
        if (style == lcss::BorderStyle::Groove) {
            s1 = lcss::BorderStyle::Inset;
            s2 = lcss::BorderStyle::Outset;
        } else {
            s1 = lcss::BorderStyle::Outset;
            s2 = lcss::BorderStyle::Inset;
        }
        double dw1by2 = std::floor(dw1/2), dw2by2 = std::floor(dw2/2);
        int wby2 = std::lround(width/2);
        switch (edge) {
        case LDirection::ToTop:
            LDrawUtil::drawEdge(p, x1, y1, x2, y1 + wby2, dw1by2, dw2by2, LDirection::ToTop, s1, c);
            LDrawUtil::drawEdge(p, x1 + dw1by2, y1 + wby2, x2 - dw2by2, y2, dw1by2, dw2by2, LDirection::ToTop, s2, c);
            break;
        case LDirection::ToBottom:
            LDrawUtil::drawEdge(p, x1, y1 + wby2, x2, y2, dw1by2, dw2by2, LDirection::ToBottom, s1, c);
            LDrawUtil::drawEdge(p, x1 + dw1by2, y1, x2 - dw2by2, y1 + wby2, dw1by2, dw2by2, LDirection::ToBottom, s2, c);
            break;
        case LDirection::ToLeft:
            LDrawUtil::drawEdge(p, x1, y1, x1 + wby2, y2, dw1by2, dw2by2, LDirection::ToLeft, s1, c);
            LDrawUtil::drawEdge(p, x1 + wby2, y1 + dw1by2, x2, y2 - dw2by2, dw1by2, dw2by2, LDirection::ToLeft, s2, c);
            break;
        case LDirection::ToRight:
            LDrawUtil::drawEdge(p, x1 + wby2, y1, x2, y2, dw1by2, dw2by2, LDirection::ToRight, s1, c);
            LDrawUtil::drawEdge(p, x1, y1 + dw1by2, x1 + wby2, y2 - dw2by2, dw1by2, dw2by2, LDirection::ToRight, s2, c);
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
    p->restore();
}

void LDrawUtil::drawRoundedCorners(LPainter *p, double x1, double y1, double x2, double y2,
                                   const LSize<double> &r1, const LSize<double> &r2, LDirection edge, lcss::BorderStyle s, LColor c)
{
    const double pw = (edge == LDirection::ToTop || edge == LDirection::ToBottom) ? y2-y1 : x2-x1;
    if (s == lcss::BorderStyle::Double) {
        double wby3 = pw/3;
        switch (edge) {
        case LDirection::ToTop:
        case LDirection::ToBottom:
            drawRoundedCorners(p, x1, y1, x2, y1+wby3, r1, r2, edge, lcss::BorderStyle::Solid, c);
            drawRoundedCorners(p, x1, y2-wby3, x2, y2, r1, r2, edge, lcss::BorderStyle::Solid, c);
            break;
        case LDirection::ToLeft:
            drawRoundedCorners(p, x1, y1+1, x1+wby3, y2, r1, r2, LDirection::ToLeft, lcss::BorderStyle::Solid, c);
            drawRoundedCorners(p, x2-wby3, y1+1, x2, y2, r1, r2, LDirection::ToLeft, lcss::BorderStyle::Solid, c);
            break;
        case LDirection::ToRight:
            drawRoundedCorners(p, x1, y1+1, x1+wby3, y2, r1, r2, LDirection::ToRight, lcss::BorderStyle::Solid, c);
            drawRoundedCorners(p, x2-wby3, y1+1, x2, y2, r1, r2, LDirection::ToRight, lcss::BorderStyle::Solid, c);
            break;
        default:
            break;
        }
        return;
    } else if (s == lcss::BorderStyle::Ridge || s == lcss::BorderStyle::Groove) {
        lcss::BorderStyle s1, s2;
        if (s == lcss::BorderStyle::Groove) {
            s1 = lcss::BorderStyle::Inset;
            s2 = lcss::BorderStyle::Outset;
        } else {
            s1 = lcss::BorderStyle::Outset;
            s2 = lcss::BorderStyle::Inset;
        }
        int pwby2 = std::lround(pw/2);
        switch (edge) {
        case LDirection::ToTop:
            drawRoundedCorners(p, x1, y1, x2, y1 + pwby2, r1, r2, LDirection::ToTop, s1, c);
            drawRoundedCorners(p, x1, y1 + pwby2, x2, y2, r1, r2, LDirection::ToTop, s2, c);
            break;
        case LDirection::ToBottom:
            drawRoundedCorners(p, x1, y1 + pwby2, x2, y2, r1, r2, LDirection::ToBottom, s1, c);
            drawRoundedCorners(p, x1, y1, x2, y2-pwby2, r1, r2, LDirection::ToBottom, s2, c);
            break;
        case LDirection::ToLeft:
            drawRoundedCorners(p, x1, y1, x1 + pwby2, y2, r1, r2, LDirection::ToLeft, s1, c);
            drawRoundedCorners(p, x1 + pwby2, y1, x2, y2, r1, r2, LDirection::ToLeft, s2, c);
            break;
        case LDirection::ToRight:
            drawRoundedCorners(p, x1 + pwby2, y1, x2, y2, r1, r2, LDirection::ToRight, s1, c);
            drawRoundedCorners(p, x1, y1, x2 - pwby2, y2, r1, r2, LDirection::ToRight, s2, c);
            break;
        default:
            break;
        }
    } else if ((s == lcss::BorderStyle::Outset && (edge == LDirection::ToTop || edge == LDirection::ToLeft))
               || (s == lcss::BorderStyle::Inset && (edge == LDirection::ToBottom || edge == LDirection::ToRight)))
        c = c.lighter();

    p->save();
    double pwby2 = pw/2;
    p->setBrush(LBrush::NoPattern);
    std::shared_ptr<LPen> pen = lPenFromStyle(c, pw, s);
    // pen->m_capStyle = LPenCapStyle::Square; // this eliminates the offby1 errors that we might hit below
    p->setPen(pen);
    switch (edge) {
    case LDirection::ToTop:
        if (!r1.isEmpty()){
            p->drawArc(LRect<double>(x1 - r1.width() + pwby2, y1 + pwby2,
                              2*r1.width() - pw, 2*r1.height() - pw, false), 135*16, -45*16);
        }
        if (!r2.isEmpty()){
            p->drawArc(LRect<double>(x2 - r2.width() + pwby2, y1 + pwby2,
                              2*r2.width() - pw, 2*r2.height() - pw, false), 45*16, 45*16);
        }
        break;
    case LDirection::ToBottom:
        if (!r1.isEmpty()){
            p->drawArc(LRect<double>(x1 - r1.width() + pwby2, y2 - 2*r1.height() + pwby2,
                              2*r1.width() - pw, 2*r1.height() - pw, false), -90 * 16, -45 * 16);
        }
        if (!r2.isEmpty()){
            p->drawArc(LRect<double>(x2 - r2.width() + pwby2, y2 - 2*r2.height() + pwby2,
                              2*r2.width() - pw, 2*r2.height() - pw, false), -90 * 16, 45 * 16);
        }
        break;
    case LDirection::ToLeft:
        if (!r1.isEmpty()){
            p->drawArc(LRect<double>(x1 + pwby2, y1 - r1.height() + pwby2,
                              2*r1.width() - pw, 2*r1.height() - pw, false), 135*16, 45*16);
        }
        if (!r2.isEmpty()){
            p->drawArc(LRect<double>(x1 + pwby2, y2 - r2.height() + pwby2,
                              2*r2.width() - pw, 2*r2.height() - pw, false), 180*16, 45*16);
        }
        break;
    case LDirection::ToRight:
        if (!r1.isEmpty()){
            p->drawArc(LRect<double>(x2 - 2*r1.width() + pwby2, y1 - r1.height() + pwby2,
                              2*r1.width() - pw, 2*r1.height() - pw, false), 45*16, -45*16);
        }
        if (!r2.isEmpty()){
            p->drawArc(LRect<double>(x2 - 2*r2.width() + pwby2, y2 - r2.height() + pwby2,
                              2*r2.width() - pw, 2*r2.height() - pw, false), 315*16, 45*16);
        }
        break;
    default:
        break;
    }
    p->restore();
}

void LDrawUtil::drawBorderPixmap(LPainter *painter, const LRect<int> &targetRect, const LMargins<int> &targetMargins,
                                 const std::shared_ptr<LPixmap> pixmap, const LRect<int> &sourceRect, const LMargins<int> &sourceMargins,
                                 const LTileRules &rules, LFlags<LDrawBorderHint> hints)
{
    if(painter == nullptr || painter->device() == nullptr){
        lWarn() << "Invalid painter!";
        return;
    }

    LPixmapFragment d;
    d.opacity = 1.0;
    d.rotation = 0.0;

    std::vector<LPixmapFragment> opaqueData;
    std::vector<LPixmapFragment> translucentData;

    // source center
    const int sourceCenterTop = sourceRect.m_y + sourceMargins.m_top;
    const int sourceCenterLeft = sourceRect.m_x + sourceMargins.m_left;
    const int sourceCenterBottom = sourceRect.y2() - sourceMargins.m_bottom + 1;
    const int sourceCenterRight = sourceRect.x2() - sourceMargins.m_right + 1;
    const int sourceCenterWidth = sourceCenterRight - sourceCenterLeft;
    const int sourceCenterHeight = sourceCenterBottom - sourceCenterTop;
    // target center
    const int targetCenterTop = targetRect.m_y + targetMargins.m_top;
    const int targetCenterLeft = targetRect.m_x + targetMargins.m_left;
    const int targetCenterBottom = targetRect.y2() - targetMargins.m_bottom + 1;
    const int targetCenterRight = targetRect.x2() - targetMargins.m_right + 1;
    const int targetCenterWidth = targetCenterRight - targetCenterLeft;
    const int targetCenterHeight = targetCenterBottom - targetCenterTop;

    std::vector<double> xTarget; // x-coordinates of target rectangles
    std::vector<double> yTarget; // y-coordinates of target rectangles

    int columns = 3;
    int rows = 3;
    if (rules.horizontal != LTileRule::Stretch && sourceCenterWidth != 0){
        columns = std::max<int>(3, 2 + std::ceil(targetCenterWidth / double(sourceCenterWidth)));
    }
    if (rules.vertical != LTileRule::Stretch && sourceCenterHeight != 0){
        rows = std::max<int>(3, 2 + std::ceil(targetCenterHeight / double(sourceCenterHeight)));
    }

    xTarget.resize(columns + 1);
    yTarget.resize(rows + 1);

    bool oldAA = painter->renderHints() & LRenderHint::Antialiasing;
    painter->setRenderHints(LRenderHint::Antialiasing, false);

    xTarget[0] = targetRect.m_x;
    xTarget[1] = targetCenterLeft;
    xTarget[columns - 1] = targetCenterRight;
    xTarget[columns] = targetRect.m_x + targetRect.width();

    yTarget[0] = targetRect.m_y;
    yTarget[1] = targetCenterTop;
    yTarget[rows - 1] = targetCenterBottom;
    yTarget[rows] = targetRect.m_y + targetRect.height();

    double dx;
    double dy;

    switch (rules.horizontal) {
    case LTileRule::Stretch:{
        dx = targetCenterWidth;
        break;
    }
    case LTileRule::Repeat:{
        dx = sourceCenterWidth;
        break;
    }
    case LTileRule::Round:{
        dx = targetCenterWidth / double(columns - 2);
        break;
    }
    }

    for (int i = 2; i < columns - 1; ++i)
        xTarget[i] = xTarget[i - 1] + dx;

    switch (rules.vertical) {
    case LTileRule::Stretch:{
        dy = targetCenterHeight;
        break;
    }
    case LTileRule::Repeat:{
        dy = sourceCenterHeight;
        break;
    }
    case LTileRule::Round:{
        dy = targetCenterHeight / double(rows - 2);
        break;
    }
    }

    for (int i = 2; i < rows - 1; ++i){
        yTarget[i] = yTarget[i - 1] + dy;
    }

    // corners
    if (targetMargins.m_top > 0 && targetMargins.m_left > 0 && sourceMargins.m_top > 0 && sourceMargins.m_left > 0) { // top left
        d.x = (0.5 * (xTarget[1] + xTarget[0]));
        d.y = (0.5 * (yTarget[1] + yTarget[0]));
        d.sourceLeft = sourceRect.m_x;
        d.sourceTop = sourceRect.m_y;
        d.width = sourceMargins.m_left;
        d.height = sourceMargins.m_top;
        d.scaleX = double(xTarget[1] - xTarget[0]) / d.width;
        d.scaleY = double(yTarget[1] - yTarget[0]) / d.height;
        if (hints & LDrawBorderHint::OpaqueTopLeft){
            opaqueData.push_back(d);
        }else{
            translucentData.push_back(d);
        }
    }
    if (targetMargins.m_top > 0 && targetMargins.m_right > 0 && sourceMargins.m_top > 0 && sourceMargins.m_right > 0) { // top right
        d.x = (0.5 * (xTarget[columns] + xTarget[columns - 1]));
        d.y = (0.5 * (yTarget[1] + yTarget[0]));
        d.sourceLeft = sourceCenterRight;
        d.sourceTop = sourceRect.m_y;
        d.width = sourceMargins.m_right;
        d.height = sourceMargins.m_top;
        d.scaleX = double(xTarget[columns] - xTarget[columns - 1]) / d.width;
        d.scaleY = double(yTarget[1] - yTarget[0]) / d.height;
        if (hints & LDrawBorderHint::OpaqueTopRight){
            opaqueData.push_back(d);
        }else{
            translucentData.push_back(d);
        }
    }
    if (targetMargins.m_bottom > 0 && targetMargins.m_left > 0 && sourceMargins.m_bottom > 0 && sourceMargins.m_left > 0) { // bottom left
        d.x = (0.5 * (xTarget[1] + xTarget[0]));
        d.y =(0.5 * (yTarget[rows] + yTarget[rows - 1]));
        d.sourceLeft = sourceRect.m_x;
        d.sourceTop = sourceCenterBottom;
        d.width = sourceMargins.m_left;
        d.height = sourceMargins.m_bottom;
        d.scaleX = double(xTarget[1] - xTarget[0]) / d.width;
        d.scaleY = double(yTarget[rows] - yTarget[rows - 1]) / d.height;
        if (hints & LDrawBorderHint::OpaqueBottomLeft){
            opaqueData.push_back(d);
        }else{
            translucentData.push_back(d);
        }
    }
    if (targetMargins.m_bottom > 0 && targetMargins.m_right > 0 && sourceMargins.m_bottom > 0 && sourceMargins.m_right > 0) { // bottom right
        d.x = (0.5 * (xTarget[columns] + xTarget[columns - 1]));
        d.y = (0.5 * (yTarget[rows] + yTarget[rows - 1]));
        d.sourceLeft = sourceCenterRight;
        d.sourceTop = sourceCenterBottom;
        d.width = sourceMargins.m_right;
        d.height = sourceMargins.m_bottom;
        d.scaleX = double(xTarget[columns] - xTarget[columns - 1]) / d.width;
        d.scaleY = double(yTarget[rows] - yTarget[rows - 1]) / d.height;
        if (hints & LDrawBorderHint::OpaqueBottomRight){
            opaqueData.push_back(d);
        }else{
            translucentData.push_back(d);
        }
    }

    // horizontal edges
    if (targetCenterWidth > 0 && sourceCenterWidth > 0) {
        if (targetMargins.m_top > 0 && sourceMargins.m_top > 0) { // top
            std::vector<LPixmapFragment> &data = hints & LDrawBorderHint::OpaqueTop ? opaqueData : translucentData;
            d.sourceLeft = sourceCenterLeft;
            d.sourceTop = sourceRect.m_y;
            d.width = sourceCenterWidth;
            d.height = sourceMargins.m_top;
            d.y = (0.5 * (yTarget[1] + yTarget[0]));
            d.scaleX = dx / d.width;
            d.scaleY = double(yTarget[1] - yTarget[0]) / d.height;
            for (int i = 1; i < columns - 1; ++i) {
                d.x = (0.5 * (xTarget[i + 1] + xTarget[i]));
                data.push_back(d);
            }
            if (rules.horizontal == LTileRule::Repeat){
                data[data.size() - 1].width = ((xTarget[columns - 1] - xTarget[columns - 2]) / d.scaleX);
            }
        }
        if (targetMargins.m_bottom > 0 && sourceMargins.m_bottom > 0) { // bottom
            std::vector<LPixmapFragment> &data = hints & LDrawBorderHint::OpaqueBottom ? opaqueData : translucentData;
            d.sourceLeft = sourceCenterLeft;
            d.sourceTop = sourceCenterBottom;
            d.width = sourceCenterWidth;
            d.height = sourceMargins.m_bottom;
            d.y = (0.5 * (yTarget[rows] + yTarget[rows - 1]));
            d.scaleX = dx / d.width;
            d.scaleY = double(yTarget[rows] - yTarget[rows - 1]) / d.height;
            for (int i = 1; i < columns - 1; ++i) {
                d.x = (0.5 * (xTarget[i + 1] + xTarget[i]));
                data.push_back(d);
            }
            if (rules.horizontal == LTileRule::Repeat){
                data[data.size() - 1].width = ((xTarget[columns - 1] - xTarget[columns - 2]) / d.scaleX);
            }
        }
    }

    // vertical edges
    if (targetCenterHeight > 0 && sourceCenterHeight > 0) {
        if (targetMargins.m_left > 0 && sourceMargins.m_left > 0) { // left
            std::vector<LPixmapFragment> &data = hints & LDrawBorderHint::OpaqueLeft ? opaqueData : translucentData;
            d.sourceLeft = sourceRect.m_x;
            d.sourceTop = sourceCenterTop;
            d.width = sourceMargins.m_left;
            d.height = sourceCenterHeight;
            d.x = (0.5 * (xTarget[1] + xTarget[0]));
            d.scaleX = double(xTarget[1] - xTarget[0]) / d.width;
            d.scaleY = dy / d.height;
            for (int i = 1; i < rows - 1; ++i) {
                d.y = (0.5 * (yTarget[i + 1] + yTarget[i]));
                data.push_back(d);
            }
            if (rules.vertical == LTileRule::Repeat){
                data[data.size() - 1].height = ((yTarget[rows - 1] - yTarget[rows - 2]) / d.scaleY);
            }
        }
        if (targetMargins.m_right > 0 && sourceMargins.m_right > 0) { // right
            std::vector<LPixmapFragment> &data = hints & LDrawBorderHint::OpaqueRight ? opaqueData : translucentData;
            d.sourceLeft = sourceCenterRight;
            d.sourceTop = sourceCenterTop;
            d.width = sourceMargins.m_right;
            d.height = sourceCenterHeight;
            d.x = (0.5 * (xTarget[columns] + xTarget[columns - 1]));
            d.scaleX = double(xTarget[columns] - xTarget[columns - 1]) / d.width;
            d.scaleY = dy / d.height;
            for (int i = 1; i < rows - 1; ++i) {
                d.y = (0.5 * (yTarget[i + 1] + yTarget[i]));
                data.push_back(d);
            }
            if (rules.vertical == LTileRule::Repeat){
                data[data.size() - 1].height = ((yTarget[rows - 1] - yTarget[rows - 2]) / d.scaleY);
            }
        }
    }

    // center
    if (targetCenterWidth > 0 && targetCenterHeight > 0 && sourceCenterWidth > 0 && sourceCenterHeight > 0) {
        std::vector<LPixmapFragment> &data = hints & LDrawBorderHint::OpaqueCenter ? opaqueData : translucentData;
        d.sourceLeft = sourceCenterLeft;
        d.sourceTop = sourceCenterTop;
        d.width = sourceCenterWidth;
        d.height = sourceCenterHeight;
        d.scaleX = dx / d.width;
        d.scaleY = dy / d.height;

        double repeatWidth = (xTarget[columns - 1] - xTarget[columns - 2]) / d.scaleX;
        double repeatHeight = (yTarget[rows - 1] - yTarget[rows - 2]) / d.scaleY;

        for (int j = 1; j < rows - 1; ++j) {
            d.y = (0.5 * (yTarget[j + 1] + yTarget[j]));
            for (int i = 1; i < columns - 1; ++i) {
                d.x = (0.5 * (xTarget[i + 1] + xTarget[i]));
                data.push_back(d);
            }
            if (rules.horizontal == LTileRule::Repeat)
                data[data.size() - 1].width = repeatWidth;
        }
        if (rules.vertical == LTileRule::Repeat) {
            for (int i = 1; i < columns - 1; ++i){
                data[data.size() - i].height = repeatHeight;
            }
        }
    }

    if (opaqueData.size()){
        painter->drawPixmapFragments(opaqueData, pixmap, true);
    }
    if (translucentData.size()){
        painter->drawPixmapFragments(translucentData, pixmap, false);
    }

    if (oldAA){
        painter->setRenderHints(LRenderHint::Antialiasing, true);
    }
}

LRect<int> LDrawUtil::alignedRect(LLayoutDirection direction, LFlags<LAlignment> alignment, const LSize<int> &size, const LRect<int> &rectangle)
{
    alignment = LGuiApplication::visualAlignment(direction, alignment);
    int x = rectangle.m_x;
    int y = rectangle.m_y;
    int w = size.width();
    int h = size.height();
    if ((alignment & LAlignment::VCenter) == LAlignment::VCenter){
        y += rectangle.size().height()/2 - h/2;
    }
    else if ((alignment & LAlignment::Bottom) == LAlignment::Bottom){
        y += rectangle.size().height() - h;
    }

    if ((alignment & LAlignment::Right) == LAlignment::Right){
        x += rectangle.size().width() - w;
    }
    else if ((alignment & LAlignment::HCenter) == LAlignment::HCenter){
        x += rectangle.size().width()/2 - w/2;
    }
    return LRect<int>(x, y, w, h, false);
}

void LDrawUtil::drawBorder(LPainter *p, const LRect<double> &br, const std::array<lcss::BorderStyle, 4> &styles,
                           const std::array<double, 4> &borders, const std::array<LColor, 4> &colors, const std::array<LSize<double>, 4> &radii)
{
    if(p == nullptr || p->device() == nullptr){
        lWarn() << "Invalid painter!";
        return;
    }

    LSize<double> tlr, trr, blr, brr;
    normalizeRadii(br, radii, tlr, trr, blr, brr);

    // Drawn in increasing order of precedence
    if (styles[(uint8_t)LDirection::ToBottom] != lcss::BorderStyle::None && borders[(uint8_t)LDirection::ToBottom] > 0) {
        bool hasBlCorner = (lFuzzyIsZero(blr.width()) == false);
        bool hasBrCorner = (lFuzzyIsZero(brr.width()) == false);

        double dw1 = (hasBlCorner || lPaintsOver(styles, colors, LDirection::ToBottom, LDirection::ToLeft)) ? 0 : borders[(uint8_t)LDirection::ToLeft];
        double dw2 = (hasBrCorner || lPaintsOver(styles, colors, LDirection::ToBottom, LDirection::ToRight)) ? 0 : borders[(uint8_t)LDirection::ToRight];
        double x1 = br.m_x + blr.width();
        double y1 = br.m_y + br.height() - borders[(uint8_t)LDirection::ToBottom];
        double x2 = br.m_x + br.width() - brr.width();
        double y2 = br.m_y + br.height() ;

        drawEdge(p, x1, y1, x2, y2, dw1, dw2, LDirection::ToBottom, styles[(uint8_t)LDirection::ToBottom], colors[(uint8_t)LDirection::ToBottom]);
        if (hasBlCorner || hasBrCorner){
            drawRoundedCorners(p, x1, y1, x2, y2, blr, brr, LDirection::ToBottom, styles[(uint8_t)LDirection::ToBottom], colors[(uint8_t)LDirection::ToBottom]);
        }
    }
    if (styles[(uint8_t)LDirection::ToRight] != lcss::BorderStyle::None && borders[(uint8_t)LDirection::ToRight] > 0) {
        bool hasTrCorner = (lFuzzyIsZero(trr.height()) == false);
        bool hasBrCorner = (lFuzzyIsZero(brr.height()) == false);

        double dw1 = (hasTrCorner || lPaintsOver(styles, colors, LDirection::ToRight, LDirection::ToTop)) ? 0 : borders[(uint8_t)LDirection::ToTop];
        double dw2 = (hasBrCorner || lPaintsOver(styles, colors, LDirection::ToRight, LDirection::ToBottom)) ? 0 : borders[(uint8_t)LDirection::ToBottom];
        double x1 = br.m_x + br.width() - borders[(uint8_t)LDirection::ToRight];
        double y1 = br.m_y + trr.height();
        double x2 = br.m_x + br.width();
        double y2 = br.m_y + br.height() - brr.height();

        drawEdge(p, x1, y1, x2, y2, dw1, dw2, LDirection::ToRight, styles[(uint8_t)LDirection::ToRight], colors[(uint8_t)LDirection::ToRight]);
        if (hasTrCorner || hasBrCorner){
            drawRoundedCorners(p, x1, y1, x2, y2, trr, brr, LDirection::ToRight, styles[(uint8_t)LDirection::ToRight], colors[(uint8_t)LDirection::ToRight]);
        }
    }
    if (styles[(uint8_t)LDirection::ToLeft] != lcss::BorderStyle::None && borders[(uint8_t)LDirection::ToLeft] > 0) {
        bool hasTlCorner = (lFuzzyIsZero(tlr.height()) == false);
        bool hasBlCorner = (lFuzzyIsZero(blr.height()) == false);

        double dw1 = (hasTlCorner || lPaintsOver(styles, colors, LDirection::ToLeft, LDirection::ToTop)) ? 0 : borders[(uint8_t)LDirection::ToTop];
        double dw2 = (hasBlCorner || lPaintsOver(styles, colors, LDirection::ToLeft, LDirection::ToBottom)) ? 0 : borders[(uint8_t)LDirection::ToBottom];
        double x1 = br.m_x;
        double y1 = br.m_y + tlr.height();
        double x2 = br.m_x + borders[(uint8_t)LDirection::ToLeft];
        double y2 = br.m_y + br.height() - blr.height();

        drawEdge(p, x1, y1, x2, y2, dw1, dw2, LDirection::ToLeft, styles[(uint8_t)LDirection::ToLeft], colors[(uint8_t)LDirection::ToLeft]);
        if (hasTlCorner || hasBlCorner){
            drawRoundedCorners(p, x1, y1, x2, y2, tlr, blr, LDirection::ToLeft, styles[(uint8_t)LDirection::ToLeft], colors[(uint8_t)LDirection::ToLeft]);
        }
    }
    if (styles[(uint8_t)LDirection::ToTop] != lcss::BorderStyle::None && borders[(uint8_t)LDirection::ToTop] > 0) {
        bool hasTlCorner = (lFuzzyIsZero(tlr.width()) == false);
        bool hasTrCorner = (lFuzzyIsZero(trr.width()) == false);

        double dw1 = (hasTlCorner || lPaintsOver(styles, colors, LDirection::ToTop, LDirection::ToLeft)) ? 0 : borders[(uint8_t)LDirection::ToLeft];
        double dw2 = (hasTrCorner || lPaintsOver(styles, colors, LDirection::ToTop, LDirection::ToRight)) ? 0 : borders[(uint8_t)LDirection::ToRight];
        double x1 = br.m_x + tlr.width();
        double y1 = br.m_y;
        double x2 = br.m_x + br.width() - trr.width();
        double y2 = br.m_y + borders[(uint8_t)LDirection::ToTop];

        drawEdge(p, x1, y1, x2, y2, dw1, dw2, LDirection::ToTop, styles[(uint8_t)LDirection::ToTop], colors[(uint8_t)LDirection::ToTop]);
        if (hasTlCorner || hasTrCorner){
            drawRoundedCorners(p, x1, y1, x2, y2, tlr, trr, LDirection::ToTop, styles[(uint8_t)LDirection::ToTop], colors[(uint8_t)LDirection::ToTop]);
        }
    }
}


LK_NAMESPACE_END


