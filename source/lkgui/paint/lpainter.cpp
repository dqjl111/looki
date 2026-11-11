#include "lpainter.h"
#include "lkcore/ldebug.h"
#include "lkgui/text/lfontengine.h"
#include "lkgui/lguiapplication.h"
#include "lkgui/text/ltextengine.h"
#include "lkgui/text/ltextitem.h"
#include "lkgui/text/lfontmetrics.h"
#include "lkgui/text/ltextlayout.h"
#include "lkgui/image/lpixmap.h"

LK_NAMESPACE_BEGIN
static inline uint32_t lineEmulation(uint32_t emulation)
{
    return emulation & (LPaintEngineFeature::PrimitiveTransform
                        | LPaintEngineFeature::AlphaBlend
                        | LPaintEngineFeature::Antialiasing
                        | LPaintEngineFeature::BrushStroke
                        | LPaintEngineFeature::ConstantOpacity
                        | LPaintEngineFeature::StretchToDevice
                        | LPaintEngineFeature::ObjectBoundingModeGradients
                        | LPaintEngineFeature::OpaqueBackground);
}

static inline LPoint<double> roundInDeviceCoordinates(const LPoint<double> &p, const LTransform2D &m)
{
    return m.inverted().map<double>(m.map<double>(p).toPoint().toPointF());
}


void LPainter::formatText(const std::shared_ptr<LFont> fnt, const LRect<double> &rIn,
                  LFlags<LTextFlag> tf, const LTextOption *option,
                  const std::wstring& str, LRect<double> *brect,
                  int tabstops, int *ta, int tabarraylen,
                  LPainter *painter)
{
    if (option) {
        tf |= option->m_align;
        if (option->m_wordWrap != LTextOption::NoWrap){
            tf |= LTextFlag::WordWrap;
        }

        if (option->m_flags & (uint32_t)LTextFlag::IncludeTrailingSpaces){
            tf |= LTextFlag::IncludeTrailingSpaces;
        }

        if (option->m_tabStopDistance >= 0 || !option->m_tabStops.empty()){
            tf |= LTextFlag::ExpandTabs;
        }
    }

    // we need to copy r here to protect against the case (&r == brect).
    LRect<double> r(rIn);

    bool dontclip  = (tf & LTextFlag::DontClip);
    bool wordwrap  = (tf & LTextFlag::WordWrap) || (tf & LTextFlag::WrapAnywhere);
    bool singleline = (tf & LTextFlag::SingleLine);
    bool showmnemonic = (tf & LTextFlag::ShowMnemonic);
    bool hidemnmemonic = (tf & LTextFlag::HideMnemonic);

    LLayoutDirection layoutDirection;
    if (tf & LTextFlag::ForceLeftToRight){
        layoutDirection = LLayoutDirection::LeftToRight;
    }else if (tf & LTextFlag::ForceRightToLeft){
        layoutDirection = LLayoutDirection::RightToLeft;
    }else if (option){
        layoutDirection = (LLayoutDirection)option->m_direction;
    }else if (painter!= nullptr){
        layoutDirection = painter->layoutDirection();
    }else{
        layoutDirection = LLayoutDirection::LeftToRight;
    }

    tf = LGuiApplication::visualAlignment(layoutDirection, tf);

    bool isRightToLeft = layoutDirection == LLayoutDirection::RightToLeft;
    bool expandtabs = ((tf & LTextFlag::ExpandTabs) &&
                       (((tf & LAlignment::Left) && !isRightToLeft) ||
                        ((tf & LAlignment::Right) && isRightToLeft)));

    if (painter == nullptr){
        tf |= LTextFlag::DontPrint;
    }

    uint32_t maxUnderlines = 0;

    LFontMetrics fm(fnt);
    std::wstring text = str;
    int offset = 0;
start_lengthVariant:
    bool hasMoreLengthVariants = false;
    // compatible behaviour to the old implementation. Replace
    // tabs by spaces
    int old_offset = offset;
    for (; offset < text.length(); offset++) {
        wchar_t chr = text.at(offset);
        if (chr == wchar_t('\r') || (singleline && chr == wchar_t('\n'))) {
            text[offset] = wchar_t(' ');
        } else if (chr == wchar_t('\n')) {
            text[offset] = lchar::LineSeparator;
        } else if (chr == wchar_t('&')) {
            ++maxUnderlines;
        } else if (chr == wchar_t('\t')) {
            if (!expandtabs) {
                text[offset] = wchar_t(' ');
            } else if (!tabarraylen && !tabstops) {
                tabstops = std::lround(fm.horizontalAdvance(wchar_t('x'))*8);
            }
        } else if (chr == wchar_t(0x9c)) {
            // string with multiple length variants
            hasMoreLengthVariants = true;
            break;
        }
    }

    std::vector<LTextLayout::FormatRange> underlineFormats;
    int length = offset - old_offset;
    if ((hidemnmemonic || showmnemonic) && maxUnderlines > 0) {
        wchar_t *cout = text.data() + old_offset;
        wchar_t *cout0 = cout;
        wchar_t *cin = cout;
        int l = length;
        while (l) {
            if (*cin == wchar_t('&')) {
                ++cin;
                --length;
                --l;
                if (!l)
                    break;
                if (*cin != wchar_t('&') && !hidemnmemonic && !(tf & LTextFlag::DontPrint)) {
                    LTextLayout::FormatRange range;
                    range.start = cout - cout0;
                    range.length = 1;
                    range.format.setFontUnderline(true);
                    underlineFormats.push_back(range);
                }
            }
            *cout = *cin;
            ++cout;
            ++cin;
            --l;
        }
    }

    double height = 0;
    double width = 0;

    std::wstring finalText = text.substr(old_offset, length);
    std::shared_ptr<LStackTextEngine> engine = std::make_shared<LStackTextEngine>(finalText, fnt);
    if (option) {
        engine->m_option = *option;
    }

    if ((engine->m_option.m_tabStopDistance <0) && (tabstops > 0)){
        engine->m_option.m_tabStopDistance = tabstops;
    }

    if (engine->m_option.m_tabStops.empty() && ta) {
        std::list<double> tabs;
        for (int i = 0; i < tabarraylen; i++){
            tabs.push_back(double(ta[i]));
        }
        engine->m_option.setTabArray(tabs);
    }

    engine->m_option.m_direction = (uint32_t)layoutDirection;
    if (tf & LAlignment::Justify){
        engine->m_option.m_align = (uint32_t)LAlignment::Justify;
    }else{
        engine->m_option.m_align = (uint32_t)LAlignment::Left; // do not do alignment twice
    }
    if (!option && (tf & LTextFlag::WrapAnywhere)){
        engine->m_option.m_wordWrap = LTextOption::WrapAnywhere;
    }

    if (tf & LTextFlag::JustificationForced){
        engine->m_forceJustification = true;
    }

    engine->m_cacheGlyphs = true;
    engine->setFormats(underlineFormats);

    LTextLayout textLayout(engine);
    if (finalText.empty()) {
        height = fm.height();
        width = 0;
        tf |= LTextFlag::DontPrint;
    } else {
        double lineWidth = 0x01000000;
        if (wordwrap || (tf & LTextFlag::JustificationForced)){
            lineWidth = std::max<double>(0, r.width());
        }
        if(!wordwrap){
            tf |= LTextFlag::IncludeTrailingSpaces;
        }
        textLayout.beginLayout();

        double leading = fm.leading();
        height = -leading;

        while (1) {
            LTextLine l = textLayout.createLine();
            if (!l.isValid()){
                break;
            }

            l.setLineWidth(lineWidth);
            height += leading;

            // Make sure lines are positioned on whole pixels
            height = std::ceil(height);
            l.setPosition(LPoint<double>(0., height));
            height += engine->m_lines[l.lineIndex()].height();
            width = std::max<double>(width, l.naturalTextWidth());
            if (!dontclip && !brect && height >= r.height()){
                break;
            }
        }
        textLayout.endLayout();
    }

    double yoff = 0;
    double xoff = 0;
    if (tf & LAlignment::Bottom){
        yoff = r.height() - height;
    }
    else if (tf & LAlignment::VCenter){
        yoff = (r.height() - height)/2;
    }

    if (tf & LAlignment::Right){
        xoff = r.width() - width;
    }
    else if (tf & LAlignment::HCenter){
        xoff = (r.width() - width)/2;
    }

    LRect<double> bounds = LRect<double>(r.m_x + xoff, r.m_y + yoff, width, height, false);

    if (hasMoreLengthVariants && !(tf & LTextFlag::LongestVariant) && !r.contains(bounds)) {
        offset++;
        goto start_lengthVariant;
    }
    if (brect){
        *brect = bounds;
    }

    if (!(tf & LTextFlag::DontPrint)) {
        bool restore = false;
        if (!dontclip && !r.contains(bounds)) {
            restore = true;
            painter->save();
            painter->setClipRect(r, LClipOper::Intersect);
        }

        for (int i = 0; i < textLayout.lineCount(); i++) {
            LTextLine line = textLayout.lineAt(i);
            engine->enableDelayDecorations();

            double advance = line.horizontalAdvance();
            xoff = 0;
            if (tf & LAlignment::Right) {
                xoff = r.width() - advance -
                       engine->leadingSpaceWidth(engine->m_lines[line.lineIndex()]);
            }
            else if (tf & LAlignment::HCenter){
                xoff = (r.width() - advance) / 2;
            }

            line.draw(painter, LPoint<double>(r.m_x + xoff, r.m_y + yoff));
            engine->drawDecorations(painter);
        }

        if (restore) {
            painter->restore();
        }
    }
}


void LPainter::formatText(const std::shared_ptr<LFont> fnt, const LRect<double> &r,
                  uint32_t tf, const std::wstring& str, LRect<double> *brect,
                  int tabstops, int *ta, int tabarraylen,
                  LPainter *painter)
{
    formatText(fnt, r,
                 tf, nullptr, str, brect,
                 tabstops, ta, tabarraylen,
                 painter);
}

LPainterState::LPainterState()
: brushOrigin(0, 0), transWorld(false), transView(false), clipEnabled(true)
{
    font = LGuiApplication::font();
    pen = std::make_shared<LPen>();
    brush = std::make_shared<LBrush>();
    bgBrush = std::make_shared<LBrush>(LColorID::White);
}

LPainterState::LPainterState(const LPainterState& s)
    : brushOrigin(s.brushOrigin), font(s.font),
    pen(s.pen), brush(s.brush), bgBrush(s.bgBrush),
    clipRegion(s.clipRegion), clipPath(s.clipPath),
    clipOperation(s.clipOperation),
    clipInfo(s.clipInfo),
    renderHints(s.renderHints),
    worldMatrix(s.worldMatrix), matrix(s.matrix), redirectionMatrix(s.redirectionMatrix),
    wx(s.wx), wy(s.wy), ww(s.ww), wh(s.wh),
    vx(s.vx), vy(s.vy), vw(s.vw), vh(s.vh),
    opacity(s.opacity), transWorld(s.transWorld), transView(s.transView),
    clipEnabled(s.clipEnabled), bgMode(s.bgMode),
    layoutDirection(s.layoutDirection),
    compositionMode(s.compositionMode),
    emulationSpecifier(s.emulationSpecifier), changeFlags(0)
{
    dirtyFlags = s.dirtyFlags;
    painter = s.painter;
}

LPainterState::~LPainterState()
{

}

////////////////////////////////////////////////////
std::shared_ptr<LPainterDummyState> LPainter::m_dummyState = nullptr;
LPainter::LPainter():m_txinv(false)
{
}

LPainter::LPainter(std::shared_ptr<LPaintDevice> device):m_txinv(false)
{
    if(device != nullptr){
        begin(device);
    }
}

LPainter::~LPainter()
{
    if (isActive()){
        end();
    }
}

void LPainter::fillRect(const LRect<double> &rect, const LColor &color)
{
    std::shared_ptr<LBrush> brushIn  = std::make_shared<LBrush>(color);
    fillRect(rect, brushIn);
}

void LPainter::fillRect(const LRect<double> &rect, const std::shared_ptr<LBrush> brushIn)
{
    if (m_engine== nullptr || m_state == nullptr) {
        lWarn() << "LPainter::fillRect: Painter not active";
        return;
    }

    std::shared_ptr<LBrush> oldBrush = m_state->brush;
    LPenStyle oldStyle = pen()->m_style;

    setPen(LPenStyle::No);
    setBrush(brushIn, false);
    drawRect(rect);
    setBrush(oldBrush, false);
    setPen(oldStyle);
}

void LPainter::eraseRect(const LRect<double> &rect)
{
    fillRect(rect, m_state->bgBrush);
}

std::shared_ptr<LPaintDevice> LPainter::device() const
{
    return m_device;
}

bool LPainter::begin(std::shared_ptr<LPaintDevice> pd)
{
    if(pd == nullptr){
        return false;
    }
//    lWarn() << "painter begin";
    if (pd->m_painters > 0) {
        lWarn() << "LPainter::begin: A paint device can only be painted by one painter at a time.";
        return false;
    }

    if (m_engine != nullptr) {
        lWarn() << "LPainter::begin: Painter already active";
        return false;
    }

//    d->helper_device = pd;
    m_device = pd;
    LPoint<double> redirectionOffset = m_device->m_startPos;
    m_engine = m_device->paintEngine();
    if (m_engine == nullptr) {
        lWarn() << "LPainter::begin: Paint device returned engine == 0, type: " <<  pd->devType();
        return false;
    }

    m_state = m_engine->createState(nullptr);

    m_state->painter = this;
    m_states.push_back(m_state);

    m_state->redirectionMatrix.translate(-redirectionOffset.m_x, -redirectionOffset.m_y);
    m_state->brushOrigin = LPoint<double>();

    // Slip a painter state into the engine before we do any other operations
    m_engine->setState(m_state);

    if (m_device->m_initPaintFun == nullptr) {
        switch (pd->devType()) {
        case LPaintDevice::PDtPixmap:
        {
            std::shared_ptr<LPixmap> pm = std::dynamic_pointer_cast<LPixmap>(pd);
            if (pm == nullptr) {
                lWarn() << "LPainter::begin: Cannot paint on a null pixmap";
                cleanupPainterState(this);
                return false;
            }

            if (pm->depth() == 1) {
                m_state->pen = std::make_shared<LPen>(LColorID::Black);
                m_state->brush = std::make_shared<LBrush>(LColorID::White);
            }
            break;
        }
        case LPaintDevice::PDtImage:
        {
            std::shared_ptr<LImage> img = std::dynamic_pointer_cast<LImage>(pd);
            if (img == nullptr) {
                lWarn() << "LPainter::begin: Cannot paint on a null image";
                cleanupPainterState(this);
                return false;
            } else if (img->format() == LImage::Format::Indexed8) {
                // Painting on indexed8 images is not supported.
                lWarn() << "LPainter::begin: Cannot paint on an image with the LImage::Format::Indexed8";
                cleanupPainterState(this);
                return false;
            }
            if (img->depth() == 1) {
                m_state->pen = std::make_shared<LPen>(LColorID::Black);
                m_state->brush = std::make_shared<LBrush>(LColorID::White);
            }
            break;
        }
        default:{
            break;
        }
        }
    }

    if (m_state->ww == 0){
        m_state->ww = m_state->wh = m_state->vw = m_state->vh = 1024;
    }

    bool begun = m_engine->begin(pd);
    if (!begun) {
        lWarn() << "LPainter::begin(): Returned false";
        if (m_engine->m_active) {
            end();
        } else {
            cleanupPainterState(this);
        }
        return false;
    } else {
        m_engine->m_active = begun;
    }

    // Copy painter properties from original paint device,
    // required for LPixmap::grabWidget()
    if (m_device->m_initPaintFun != nullptr) {
        m_device->m_initPaintFun(this);
    } else {
        m_state->layoutDirection = LLayoutDirection::Auto;
        m_state->font->m_dpi = m_device->logicalDpiY();
    }

    const LRect<int>& systemRect = m_engine->m_systemRect;
    if (!systemRect.isEmpty()) {
        m_state->ww = m_state->vw = systemRect.width();
        m_state->wh = m_state->vh = systemRect.height();
    } else {
        m_state->ww = m_state->vw = pd->metric(LPaintDevice::PdmWidth);
        m_state->wh = m_state->vh = pd->metric(LPaintDevice::PdmHeight);
    }

    const LPoint<int> coordinateOffset = m_engine->coordinateOffset();
    m_state->redirectionMatrix.translate(-coordinateOffset.m_x, -coordinateOffset.m_y);

    if (!m_state->redirectionMatrix.isIdentity() || effectiveDevicePixelRatio() > 1){
        updateMatrix();
    }
    m_state->renderHints = LRenderHint::TextAntialiasing;
    ++(m_device->m_painters);

    m_state->emulationSpecifier = 0;
    return true;
}

bool LPainter::end()
{
    if (m_engine == nullptr) {
        lWarn() << "LPainter::end: Painter not active, aborted";
        cleanupPainterState(this);
        return false;
    }

    bool ended = true;

    if (m_engine->m_active) {
        ended = m_engine->end();
        updateState(nullptr);

        if(m_device != nullptr){
            if(m_device->m_painters >0){
                --(m_device->m_painters);
                if (m_device->m_painters == 0) {
                    m_engine->m_active  = false;
                }
            }
        }
    }

    if (m_states.size() > 1) {
        lWarn() << "LPainter::end: Painter ended with saved states: " << m_states.size();
    }

    cleanupPainterState(this);
    return ended;

}

bool LPainter::isActive() const
{
    return m_engine != nullptr;
}

void LPainter::save()
{
    if (m_engine == nullptr) {
        lWarn() << "LPainter::save: Painter not active";
        return;
    }

    if(m_states.empty()){
        m_state = m_engine->createState(nullptr);
    }else{
        m_state = m_engine->createState(m_states.back());
    }
    m_engine->m_state = m_state;
    m_states.push_back(m_state);
}

void LPainter::restore()
{
    if (m_states.size()<=1) {
        lWarn() << "LPainter::restore: Unbalanced save/restore";
        return;
    } else if (m_engine == nullptr) {
        lWarn() << "LPainter::restore: Painter not active";
        return;
    }


    std::shared_ptr<LPainterState> tmp = m_state;
    m_states.pop_back();
    m_state = m_states.back();
    m_txinv = false;
    m_engine->setState(m_state);
}

LFlags<LRenderHint> LPainter::renderHints() const
{
    if(m_state != nullptr){
        return m_state->renderHints;
    }
    return LFlags<LRenderHint>();
}

void LPainter::setRenderHints(LFlags<LRenderHint> hints, bool on)
{
    if (m_engine == nullptr || m_state == nullptr) {
        lWarn() << "LPainter::setRenderHint: Painter must be active to set rendering hints";
        return;
    }

    if (on){
        m_state->renderHints |= hints;
    }
    else{
        m_state->renderHints &= ~hints;
    }

    m_engine->renderHintsChanged();
}

void LPainter::setTransform(const LTransform2D &transform, bool combine)
{
    setWorldTransform(transform, combine);
}

const LTransform2D &LPainter::transform() const
{
    return worldTransform();
}

void LPainter::resetTransform()
{
    if (m_engine == nullptr || m_state == nullptr) {
        lWarn() << "LPainter::resetMatrix: Painter not active";
        return;
    }

    m_state->wx = m_state->wy = m_state->vx = m_state->vy = 0;                        // default view origins
    m_state->ww = m_state->vw = m_device->metric(LPaintDevice::PdmWidth);
    m_state->wh = m_state->vh = m_device->metric(LPaintDevice::PdmHeight);
    m_state->worldMatrix = LTransform2D();
    m_state->transWorld = false;
    m_state->transView = false;
    m_engine->transformChanged();
}

void LPainter::drawImage(const LPoint<double> &p, const std::shared_ptr<LImage> image)
{
    if (m_engine == nullptr || image == nullptr){
        return;
    }

    m_engine->drawImage(p, image);
}

void LPainter::drawImage(const LRect<double> &targetRect,
                         const std::shared_ptr<LImage> image,
                         const LRect<double> &sourceRect,
                         LFlags<LImageConvertFlag> flags)
{

    if (m_engine == nullptr|| image == nullptr){
        return;
    }

    double x = targetRect.m_x;
    double y = targetRect.m_y;
    double w = targetRect.m_width;
    double h = targetRect.m_height;
    double sx = sourceRect.m_x;
    double sy = sourceRect.m_y;
    double sw = sourceRect.m_width;
    double sh = sourceRect.m_height;
    double imageScale = image->devicePixelRatio();

    // Sanity-check clipping
    if (sw <= 0){
        sw = image->width() - sx;
    }

    if (sh <= 0){
        sh = image->height() - sy;
    }

    if (w < 0){
        w = sw / imageScale;
    }if (h < 0){
        h = sh / imageScale;
    }

    if (sx < 0) {
        double w_ratio = sx * w/sw;
        x -= w_ratio;
        w += w_ratio;
        sw += sx;
        sx = 0;
    }

    if (sy < 0) {
        double h_ratio = sy * h/sh;
        y -= h_ratio;
        h += h_ratio;
        sh += sy;
        sy = 0;
    }

    if (sw + sx > image->width()) {
        double delta = sw - (image->width() - sx);
        double w_ratio = delta * w/sw;
        sw -= delta;
        w -= w_ratio;
    }

    if (sh + sy > image->height()) {
        double delta = sh - (image->height() - sy);
        double h_ratio = delta * h/sh;
        sh -= delta;
        h -= h_ratio;
    }

    if (w == 0 || h == 0 || sw <= 0 || sh <= 0){
        return;
    }

    m_engine->drawImage(LRect<double>(x, y, w, h, false),
                        image,
                        LRect<double>(sx, sy, sw, sh, false), flags);
}

void LPainter::drawPixmap(const LPoint<double> &p, const std::shared_ptr<LPixmap> pm)
{
    if (m_engine == nullptr || m_state == nullptr || pm == nullptr){
        return;
    }
    m_engine->drawPixmap(p, pm);

}

void LPainter::drawPixmap(const LRect<double> &r,
                          const std::shared_ptr<LPixmap> pm,
                          const LRect<double> &sr)
{
    if (m_engine == nullptr|| pm == nullptr || pm->isNull()){
        return;
    }
    double x = r.m_x;
    double y = r.m_y;
    double w = r.m_width;
    double h = r.m_height;
    double sx = sr.m_x;
    double sy = sr.m_y;
    double sw = sr.m_width;
    double sh = sr.m_height;

    // Get pixmap scale. Use it when calculating the target
    // rect size from pixmap size. For example, a 2X 64x64 pixel
    // pixmap should result in a 32x32 point target rect.
    const double pmscale = pm->devicePixelRatio();

    // Sanity-check clipping
    if (sw <= 0){
        sw = pm->width() - sx;
    }

    if (sh <= 0){
        sh = pm->height() - sy;
    }

    if (w < 0){
        w = sw / pmscale;
    }
    if (h < 0){
        h = sh / pmscale;
    }

    if (sx < 0) {
        double w_ratio = sx * w/sw;
        x -= w_ratio;
        w += w_ratio;
        sw += sx;
        sx = 0;
    }

    if (sy < 0) {
        double h_ratio = sy * h/sh;
        y -= h_ratio;
        h += h_ratio;
        sh += sy;
        sy = 0;
    }

    if (sw + sx > pm->width()) {
        double delta = sw - (pm->width() - sx);
        double w_ratio = delta * w/sw;
        sw -= delta;
        w -= w_ratio;
    }

    if (sh + sy > pm->height()) {
        double delta = sh - (pm->height() - sy);
        double h_ratio = delta * h/sh;
        sh -= delta;
        h -= h_ratio;
    }

    if (w == 0 || h == 0 || sw <= 0 || sh <= 0){
        return;
    }

    m_engine->drawPixmap(LRect<double>(x, y, w, h, false), pm, LRect<double>(sx, sy, sw, sh, false));
}

void LPainter::drawTiledPixmap(const LRect<double> &r,
                               const std::shared_ptr<LPixmap> pm,
                               const LPoint<double> &sp)
{
    if (m_engine == nullptr || pm == nullptr || r.isEmpty()){
        return;
    }

    int sw = std::lround(pm->width());
    int sh = std::lround(pm->height());
    int sx = std::lround(sp.m_x);
    int sy = std::lround(sp.m_y);
    if (sx < 0){
        sx = sw - (-sx % sw);
    }else{
        sx = sx % sw;
    }
    if (sy < 0){
        sy = sh - (-sy % sh);
    }else{
        sy = sy % sh;
    }
    m_engine->drawTiledPixmap(r, pm, LPoint<double>(sx, sy));
}

void LPainter::drawPixmapFragments(const std::vector<LPixmapFragment> fragments, const std::shared_ptr<LPixmap> pm, bool isOpaque)
{
    if (m_engine == nullptr || m_state == nullptr || pm == nullptr){
        return;
    }

    m_engine->drawPixmapFragments(fragments, pm, isOpaque);
}

void LPainter::setCompositionMode(LCompositionMode mode)
{
    if (m_engine == nullptr || m_state == nullptr) {
        lWarn() << "LPainter::setCompositionMode: Painter not active";
        return;
    }
    if (m_state->compositionMode == mode){
        return;
    }

    m_state->compositionMode = mode;
    m_engine->compositionModeChanged();

}

LCompositionMode LPainter::compositionMode() const
{
    if (m_engine == nullptr || m_state == nullptr) {
        lWarn() << "LPainter::compositionMode: Painter not active";
        return LCompositionMode::SourceOver;
    }
    return m_state->compositionMode;
}

void LPainter::setWorldTransform(const LTransform2D &matrix, bool combine)
{
    if (m_engine == nullptr || m_state == nullptr) {
        lWarn() << "LPainter::setWorldTransform: Painter not active";
        return;
    }

    if (combine){
        m_state->worldMatrix = matrix * m_state->worldMatrix;                        // combines
    }else{
        m_state->worldMatrix = matrix;                                // set new matrix
    }

    m_state->transWorld = true;
    updateMatrix();
}

const LTransform2D &LPainter::worldTransform() const
{
    return m_state->worldMatrix;
}

void LPainter::setClipRegion(std::shared_ptr<LRegion> r, LClipOper op)
{
    if (m_engine == nullptr) {
        lWarn() << "LPainter::setClipRegion: Painter not active";
        return;
    }
    if (!m_state->clipEnabled && op != LClipOper::No){
        op = LClipOper::Replace;
    }

    m_state->clipEnabled = true;
    m_engine->clip(r, op);
    if (op == LClipOper::No || op == LClipOper::Replace){
        m_state->clipInfo.clear();
    }
    m_state->clipInfo.push_back(LPainterClipInfo(r, op, m_state->matrix));
    m_state->clipOperation = op;
}

void LPainter::setClipRect(const LRect<double> &rect, LClipOper op)
{
    if (m_engine == nullptr || m_state == nullptr) {
        lWarn() << "LPainter::setClipRect: Painter not active";
        return;
    }

    if (!m_state->clipEnabled && op != LClipOper::No){
        op = LClipOper::Replace;
    }

    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    path->addRect(rect);

    m_state->clipEnabled = true;
    m_engine->clip(path, op);
    if (op == LClipOper::Replace || op == LClipOper::No){
        m_state->clipInfo.clear();
    }
    m_state->clipInfo.push_back(LPainterClipInfo(rect, op, m_state->matrix));
    m_state->clipOperation = op;
}

void LPainter::setClipRect(const LRect<int> &rect, LClipOper op)
{
    if (m_engine == nullptr || m_state == nullptr) {
        lWarn() << "LPainter::setClipRect: Painter not active";
        return;
    }

    if (!m_state->clipEnabled && op != LClipOper::No){
        op = LClipOper::Replace;
    }

    m_state->clipEnabled = true;
    m_engine->clip(rect, op);
    if (op == LClipOper::Replace || op == LClipOper::No){
        m_state->clipInfo.clear();
    }
    m_state->clipInfo.push_back(LPainterClipInfo(rect, op, m_state->matrix));
    m_state->clipOperation = op;
}

void LPainter::setClipPath(const std::shared_ptr<LPainterPath> path, LClipOper op)
{
    if (m_engine == nullptr || m_state == nullptr) {
        lWarn() << "LPainter::setClipPath: Painter not active";
        return;
    }

    if ((!m_state->clipEnabled && op != LClipOper::No)){
        op = LClipOper::Replace;
    }

    m_state->clipEnabled = true;
    m_engine->clip(path, op);
    if (op == LClipOper::No || op == LClipOper::Replace){
        m_state->clipInfo.clear();
    }
    m_state->clipInfo.push_back(LPainterClipInfo(path, op, m_state->matrix));
    m_state->clipOperation = op;
}

void LPainter::setPen(const LColor &color)
{
    if (m_engine == nullptr || m_state == nullptr) {
        return;
    }

    if(m_state->pen == nullptr){
        m_state->pen = std::make_shared<LPen>();
    }else if(m_state->pen->m_brush->m_color == color){
        return;
    }else{
        m_state->pen = m_state->pen->backPen();
    }

    m_state->pen->m_brush->m_color = color;
    m_engine->penChanged();
}

void LPainter::setPen(const std::shared_ptr<LPen> pen, bool useBack)
{
    if (m_engine == nullptr || m_state == nullptr  || pen == nullptr) {
        return;
    }

    if(m_state->pen != pen){
        if(useBack){
            m_state->pen = pen->backPen();
        }else{
            m_state->pen = pen;
        }

        m_engine->penChanged();
    }else{
        if(useBack){
            m_state->pen = pen->backPen();
            m_engine->penChanged();
        }
    }
}

void LPainter::setPen(LPenStyle style)
{
    if (m_engine == nullptr || m_state == nullptr) {
        return;
    }

    if(m_state->pen == nullptr){
        m_state->pen = std::make_shared<LPen>();
    }else if(m_state->pen->m_style == style){
        return;
    }else{
        m_state->pen = m_state->pen->backPen();
    }

    m_state->pen->m_style = style;
    m_engine->penChanged();
}

void LPainter::setPen(const LColor &color, LPenStyle style)
{
    if (m_engine == nullptr || m_state == nullptr) {
        return;
    }

    if(m_state->pen == nullptr){
        m_state->pen = std::make_shared<LPen>();
    }else if(m_state->pen->m_style == style
               && m_state->pen->m_brush->m_color == color){
        return;
    }else{
        m_state->pen = m_state->pen->backPen();
    }

    m_state->pen->m_style = style;
    m_state->pen->m_brush->m_color = color;
    m_engine->penChanged();
}

void LPainter::setPen(const LColor &color, LPenStyle style, int width)
{
    if (m_engine == nullptr || m_state == nullptr) {
        return;
    }

    if(m_state->pen == nullptr){
        m_state->pen = std::make_shared<LPen>();
    }else if(m_state->pen->m_style == style
               && m_state->pen->m_brush->m_color == color
               && m_state->pen->m_width == width){
        return;
    }else{
        m_state->pen = m_state->pen->backPen();
    }

    m_state->pen->m_style = style;
    m_state->pen->m_brush->m_color = color;
    m_state->pen->m_width = width;
    m_engine->penChanged();
}

std::shared_ptr<LPen> LPainter::pen() const
{
    if (m_engine == nullptr || m_state == nullptr) {
        return fakeState()->pen;
    }
    if(m_state->pen == nullptr){
        m_state->pen = std::make_shared<LPen>();
    }
    return m_state->pen;
}

void LPainter::setBrush(const LColor &color)
{
    if (m_engine == nullptr || m_state == nullptr) {
        return;
    }

    if(m_state->brush == nullptr){
        m_state->brush = std::make_shared<LBrush>();
    }else if(m_state->brush->m_color == color){
        return;
    }else{
        m_state->brush = m_state->brush->backBrush();
    }

    m_state->brush->m_color = color;
    m_engine->brushChanged();

}

void LPainter::setBrush(const std::shared_ptr<LBrush> brush, bool useBack)
{
    if (m_engine == nullptr || m_state == nullptr || brush == nullptr) {
        return;
    }

    if(m_state->brush != brush){
        if(useBack){
            m_state->brush = brush->backBrush();
        }else{
            m_state->brush = brush;
        }
        m_engine->brushChanged();
    }else{
        if(useBack){
            m_state->brush = brush->backBrush();
            m_engine->brushChanged();
        }
    }
}

void LPainter::setBrush(LBrush::Style style)
{
    if (m_engine == nullptr || m_state == nullptr) {
        return;
    }


    if(m_state->brush == nullptr){
        m_state->brush = std::make_shared<LBrush>();
    }else if(m_state->brush->m_style == style){
        return;
    }else{
        m_state->brush = m_state->brush->backBrush();
    }

    m_state->brush->m_style = style;
    m_engine->brushChanged();
}

void LPainter::setBrush(const LColor &color, LBrush::Style style)
{
    if (m_engine == nullptr || m_state == nullptr) {
        return;
    }


    if(m_state->brush == nullptr){
        m_state->brush = std::make_shared<LBrush>();
    }else if((m_state->brush->m_color == color) && (m_state->brush->m_style == style)){
        return;
    }else{
        m_state->brush = m_state->brush->backBrush();
    }

    m_state->brush->m_color = color;
    m_state->brush->m_style = style;
    m_engine->brushChanged();
}

std::shared_ptr<LBrush> LPainter::brush() const
{
    if (m_state == nullptr) {
        return fakeState()->brush;
    }

    if(m_state->brush == nullptr){
        m_state->brush = std::make_shared<LBrush>();
    }
    return m_state->brush;
}

std::shared_ptr<LFont> LPainter::font() const
{
    if (m_state == nullptr) {
        return fakeState()->font;
    }
    if(m_state->font == nullptr){
        return LGuiApplication::font();
    }
    return m_state->font;
}

void LPainter::setFont(const std::shared_ptr<LFont> font)
{
    if (m_state == nullptr || font == nullptr) {
        return;
    }

    if(m_state->font != font){
        m_state->font = font;
        if(m_device != nullptr){
            m_state->font->m_dpi = m_device->logicalDpiY();
        }
    }
}

void LPainter::translate(const LPoint<double> &offset)
{
    if (m_state == nullptr) {
        return;
    }

    m_state->worldMatrix.translate(offset.m_x, offset.m_y);
    m_state->transWorld = true;
    updateMatrix();
}

void LPainter::fillPath(const std::shared_ptr<LPainterPath> path, LColor color)
{
    std::shared_ptr<LBrush> b= std::make_shared<LBrush>(color);
    fillPath(path, b);
}

void LPainter::fillPath(const std::shared_ptr<LPainterPath> path, const std::shared_ptr<LBrush> brush)
{
    if (path == nullptr || path->isEmpty()){
        return;
    }

    if (m_engine== nullptr || m_state == nullptr) {
        lWarn() << "LPainter::drawPath: Painter not active";
        return;
    }

    m_engine->fill(path, brush);
}

void LPainter::drawPath(const std::shared_ptr<LPainterPath> path)
{
    if (path == nullptr || path->isEmpty()){
        return;
    }

    if (m_engine== nullptr || m_state == nullptr) {
        lWarn() << "LPainter::drawPath: Painter not active";
        return;
    }

    m_engine->drawPath(path);
}

void LPainter::drawRects(const LRect<double> *rects, int rectCount)
{
    if (m_engine == nullptr) {
        return;
    }

    if (rectCount <= 0){
        return;
    }

    m_engine->drawRects(rects, rectCount);
}

void LPainter::drawRect(const LRect<double> &rect)
{
    drawRects(&rect, 1);
}

void LPainter::drawRects(const LRect<int> *rects, int rectCount)
{
    if (m_engine == nullptr) {
        return;
    }

    if (rectCount <= 0){
        return;
    }
    m_engine->drawRects(rects, rectCount);
}

void LPainter::drawRect(const LRect<int>& rect)
{
    drawRects(&rect, 1);
}

void LPainter::drawRoundedRect(const LRect<double> &rect, double xRadius, double yRadius, LSizeMode mode)
{

    if (m_engine == nullptr) {
        return;
    }

    m_engine->drawRoundedRect(rect, xRadius, yRadius, mode);
}

void LPainter::drawLines(const LLine<double> *lines, int lineCount)
{
    if (m_engine == nullptr || lineCount < 1){
        return;
    }

    m_engine->drawLines(lines, lineCount);
}

void LPainter::drawLine(const LLine<double> &line)
{
    drawLines(&line, 1);
}

void LPainter::drawLines(const LLine<int> *lines, int lineCount)
{
    if (m_engine == nullptr || lineCount < 1){
        return;
    }

    m_engine->drawLines(lines, lineCount);
}

void LPainter::drawLine(const LLine<int> &line)
{
    drawLines(&line, 1);
}

void LPainter::drawArc(const LRect<double> &r, int a, int alen)
{
    if (m_engine == nullptr){
        return;
    }
    m_engine->drawArc(r, a, alen);
}

void LPainter::drawEllipses(const LRect<double> *rects, int rectCount)
{
    if (m_engine == nullptr){
        return;
    }
    m_engine->drawEllipses(rects, rectCount);
}

void LPainter::drawEllipse(const LRect<double> &r)
{
    drawEllipses(&r, 1);
}

void LPainter::drawEllipses(const LRect<int> *rects, int rectCount)
{
    if (m_engine == nullptr){
        return;
    }
    m_engine->drawEllipses(rects, rectCount);
}

void LPainter::drawEllipse(const LRect<int> &rect)
{
    drawEllipses(&rect, 1);
}

void LPainter::drawPolyline(const LPoint<double> *points, int pointCount)
{
    if (!m_engine || pointCount < 2){
        return;
    }

    m_engine->drawPolygon(points, pointCount, LPaintEngine::PolylineMode);
}

void LPainter::drawPolygon(const LPoint<double> *points, int pointCount, LFillRule rule)
{
    if (!m_engine || pointCount < 2){
        return;
    }

    m_engine->drawPolygon(points, pointCount, LPaintEngine::PolygonDrawMode(rule));
}

void LPainter::drawConvexPolygon(const LPoint<double> *points, int pointCount)
{
    if (!m_engine || pointCount < 2){
        return;
    }

    m_engine->drawPolygon(points, pointCount, LPaintEngine::ConvexMode);
}


static void drawTextItemDecoration(LPainter *painter,
                                   const LPoint<double> &pos,
                                   const std::shared_ptr<LFontEngine> fe,
                                   std::shared_ptr<LTextEngine> textEngine,
                                   LPenStyle underlineStyle,
                                   LFlags<LTextRenderHint> flags, double width,
                                   const LTextCharFormat &charFormat)
{
    if (underlineStyle ==LPenStyle::No
        && !(flags & (LTextRenderHint::StrikeOut | LTextRenderHint::Overline))){
        return;
    }

    const std::shared_ptr<LPen> oldPen = painter->pen();
    std::shared_ptr<LPen> pen = oldPen->backPen();
    const std::shared_ptr<LBrush> oldBrush = painter->brush();
    painter->setBrush(LBrush::NoPattern);
    pen->m_style = LPenStyle::Solid;
    pen->m_width = fe->lineThickness();
    pen->m_capStyle = LPenCapStyle::Flat;

    LLine<double> line(std::floor(pos.m_x), pos.m_y, std::floor(pos.m_x + width), pos.m_y);
    const double underlineOffset = fe->underlinePosition();

    if (underlineStyle != LPenStyle::No) {
        // Deliberately ceil the offset to avoid the underline coming too close to
        // the text above it, but limit it to stay within descent.
        double adjustedUnderlineOffset = std::ceil(underlineOffset) + 0.5;
        if (underlineOffset <= fe->descent()){
            adjustedUnderlineOffset = std::min<double>(adjustedUnderlineOffset, fe->descent() - 0.5);
        }
        const double underlinePos = pos.m_y + adjustedUnderlineOffset;
        pen->m_style = (LPenStyle)(underlineStyle);
        painter->setPen(pen, true);
        LLine<double> underline(line.m_pt1.m_x, underlinePos, line.m_pt2.m_x, underlinePos);
        if (textEngine){
            textEngine->addUnderline(painter, underline);
        }else{
            painter->drawLine(underline);
        }
    }


    if(flags & (LTextRenderHint::StrikeOut|LTextRenderHint::Overline)){
        pen->m_style = LPenStyle::Solid;
        painter->setPen(pen, true);
        if (flags & LTextRenderHint::StrikeOut) {
            LLine<double> strikeOutLine = line;
            strikeOutLine.translate(0., - fe->ascent() / 3.);
            painter->setPen(pen, true);
            if (textEngine){
                textEngine->addStrikeOut(painter, strikeOutLine);
            }else{
                painter->drawLine(strikeOutLine);
            }
        }

        if (flags & LTextRenderHint::Overline) {
            LLine<double> overline = line;
            overline.translate(0., - fe->ascent());
            if (textEngine){
                textEngine->addOverline(painter, overline);
            }else{
                painter->drawLine(overline);
            }
        }
    }


    painter->setPen(oldPen, true);
    painter->setBrush(oldBrush, true);
}

void LPainter::drawTextItem(const LPoint<double> &p,
                            const LTextItem &ti,
                            std::shared_ptr<LTextEngine> textEngine)
{
    if (m_engine == nullptr || m_state == nullptr){
        return;
    }


    if (pen()->m_style == LPenStyle::No){
        return;
    }

    const LFlags<LRenderHint> oldRenderHints = m_state->renderHints;
    if (!(m_state->renderHints & LRenderHint::Antialiasing)
        && (m_state->matrix.type() >= LTransform2D::TxScale)
        && (LGuiApplication::m_antialiasingDisabled == false)) {
        // draw antialias decoration (underline/overline/strikeout) with
        // transformed text
        bool aa = true;
        const LTransform2D &m = m_state->matrix;
        if (m_state->matrix.type() < LTransform2D::TxShear) {
            bool isPlain90DegreeRotation =
                (lFuzzyIsZero(m.m11())
                 && lFuzzyIsZero(m.m12() - 1)
                 && lFuzzyIsZero(m.m21() + 1)
                 && lFuzzyIsZero(m.m22())
                 )
                ||
                (lFuzzyIsZero(m.m11() + 1)
                 && lFuzzyIsZero(m.m12())
                 && lFuzzyIsZero(m.m21())
                 && lFuzzyIsZero(m.m22() + 1)
                 )
                ||
                (lFuzzyIsZero(m.m11())
                 && lFuzzyIsZero(m.m12() + 1)
                 && lFuzzyIsZero(m.m21() - 1)
                 && lFuzzyIsZero(m.m22())
                 )
                ;
            aa = !isPlain90DegreeRotation;
        }
        if (aa){
            setRenderHints(LRenderHint::Antialiasing, true);
        }
    }

    if (ti.glyphs.numGlyphs <=0) {
        drawTextItemDecoration(this, p, ti.fontEngine, textEngine, ti.underlineStyle,
                               ti.flags, ti.width, ti.charFormat);
    } else {
        m_engine->drawTextItem(p, ti);
        drawTextItemDecoration(this, p, ti.fontEngine, textEngine, ti.underlineStyle,
                               ti.flags, ti.width, ti.charFormat);
    }

    if (m_state->renderHints != oldRenderHints) {
        m_state->renderHints = oldRenderHints;
        m_engine->renderHintsChanged();
    }
}

void LPainter::drawText(const LPoint<double> &p, const std::wstring &s)
{
    drawText(p, s, 0, 0);
}

void LPainter::drawText(const LPoint<double> &p,
                        const std::wstring &str,
                        LFlags<LTextFlag> tf,
                        int justificationPadding)
{
    if(m_state == nullptr || str.empty() || pen()->m_style == LPenStyle::No){
        return;
    }

    //lWarn() << "draw text:" << str << ", color:" << pen()->color().name();

    std::shared_ptr<LStackTextEngine> engine = std::make_shared<LStackTextEngine>(str, m_state->font);
    engine->m_option.m_direction = (uint32_t)m_state->layoutDirection;
    if (tf & (LTextFlag::ForceLeftToRight|LTextFlag::ForceRightToLeft)) {
        engine->m_ignoreBidi = true;
        engine->m_option.m_direction = uint32_t((tf & LTextFlag::ForceLeftToRight) ? LLayoutDirection::LeftToRight : LLayoutDirection::RightToLeft);
    }
    engine->itemize();
    LScriptLine line;
    line.length = str.length();
    engine->shapeLine(line);

    uint32_t nItems = (uint32_t)engine->m_layoutData->m_items.size();
    std::vector<int> visualOrder(nItems);
    std::vector<uint8_t> levels(nItems);
    for (uint32_t i = 0; i < nItems; ++i){
        levels[i] = engine->m_layoutData->m_items[i].analysis.bidiLevel;
    }
    LTextEngine::bidiReorder(nItems, levels.data(), visualOrder.data());

    if (justificationPadding > 0) {
        engine->m_option.m_align = (uint32_t)LAlignment::Justify;
        engine->m_forceJustification = true;
        // this works because justify() is only interested in the difference between width and textWidth
        line.width = justificationPadding;
        engine->justify(line);
    }
    double x = p.m_x;

    for (uint32_t i = 0; i < nItems; ++i) {
        int item = visualOrder[i];
        const LScriptItem &si = engine->m_layoutData->m_items.at(item);
        if (si.analysis.flags >= LScriptAnalysis::TabOrObject) {
            x += si.width;
            continue;
        }
        std::shared_ptr<LFont> f = engine->font(si);
        LTextItem gf(&si, f);
        gf.glyphs = engine->shapedGlyphs(&si);
        gf.chars = engine->m_layoutData->m_string.data() + si.position;
        gf.numChars = engine->length(item);
        if (engine->m_forceJustification) {
            for (int j=0; j<gf.glyphs.numGlyphs; ++j){
                gf.width += gf.glyphs.effectiveAdvance(j);
            }
        } else {
            gf.width = si.width;
        }

        gf.logClusters = engine->logClusters(&si);

        drawTextItem(LPoint<double>(x, p.m_y), gf);
        x += gf.width;
    }
}

void LPainter::drawText(const LRect<double> &r,
                        const std::wstring &str,
                        LFlags<LTextFlag> tf,
                        const LTextOption &o)
{
    if(m_state == nullptr || str.empty() || pen()->m_style == LPenStyle::No){
        return;
    }
    formatText(m_state->font, r, tf, &o, str, nullptr, 0, nullptr, 0, this);
}

void LPainter::drawText(const LRect<double> &r, int flags, const std::wstring &str, LRect<double> *br)
{
    if(m_state == nullptr || str.empty() || pen()->m_style == LPenStyle::No){
        return;
    }

    formatText(m_state->font, r, flags, nullptr, str, br, 0, nullptr, 0, this);
}

void LPainter::drawPoints(const LPoint<double> *points, int pointCount)
{
    if (m_engine == nullptr) {
        lWarn() << "LPainter::drawPoints: Painter not active";
        return;
    }

    if (pointCount <= 0){
        return;
    }

    m_engine->drawPoints(points, pointCount);
}

void LPainter::drawPoints(const LPoint<int> *points, int pointCount)
{
    if (m_engine == nullptr) {
        lWarn() << "LPainter::drawPoints: Painter not active";
        return;
    }

    if (pointCount <= 0){
        return;
    }

    m_engine->drawPoints(points, pointCount);
}

void LPainter::setLayoutDirection(LLayoutDirection direction)
{
    if (m_state != nullptr){
        m_state->layoutDirection = direction;
    }
}

LLayoutDirection LPainter::layoutDirection() const
{
    if (m_state != nullptr){
        return m_state->layoutDirection;
    }
    return LLayoutDirection::Auto;
}

void LPainter::setClipping(bool enable)
{
    if (m_state == nullptr) {
        return;
    }

    if (hasClipping() == enable){
        return;
    }

    // we can't enable clipping if we don't have a clip
    if (enable
        && (m_state->clipInfo.empty() || m_state->clipInfo.back().operation == LClipOper::No)){
        return;
    }
    m_state->clipEnabled = enable;
    if (m_engine != nullptr) {
        m_engine->clipEnabledChanged();
    }
}

bool LPainter::hasClipping() const
{
    if (m_state == nullptr) {
        return false;
    }
    return m_state->clipEnabled && m_state->clipOperation != LClipOper::No;
}

std::shared_ptr<LPainterPath> LPainter::clipPath() const
{
    if (m_state == nullptr) {
        return nullptr;
    }

    // No clip, return empty
    if (m_state->clipInfo.empty()) {
        return nullptr;
    } else {

        // Update inverse matrix, used below.
        if (!m_txinv){
            const_cast<LPainter*>(this)->updateInvMatrix();
        }

        // For the simple case avoid conversion.
        if (m_state->clipInfo.size() == 1
            && m_state->clipInfo.at(0).clipType == LPainterClipInfo::PathClip) {
            LTransform2D matrix = (m_state->clipInfo.at(0).matrix * m_invMatrix);
            return  matrix.map(m_state->clipInfo.at(0).path);

        } else if (m_state->clipInfo.size() == 1
                   && m_state->clipInfo.at(0).clipType == LPainterClipInfo::RectClip) {
            LTransform2D matrix = (m_state->clipInfo.at(0).matrix * m_invMatrix);
            std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
            path->addRect(m_state->clipInfo.at(0).rect);
            return matrix.map(path);
        } else {
            // Fallback to clipRegion() for now, since we don't have isect/unite for paths
            std::shared_ptr<LRegion> r= clipRegion();
            if(r != nullptr){
                if(!r->isEmpty()){
                    return LRegion::regionToPath(r.get());
                }
            }
            return nullptr;
        }
    }
}

std::shared_ptr<LRegion> LPainter::clipRegion() const
{
    if (m_engine == nullptr) {
        lWarn() << "LPainter::clipRegion: Painter not active";
        return nullptr;
    }

    std::shared_ptr<LRegion> region = std::make_shared<LRegion>();
    bool lastWasNothing = true;

    if (!m_txinv){
        const_cast<LPainter *>(this)->updateInvMatrix();
    }

    for (const LPainterClipInfo &info : std::as_const(m_state->clipInfo)) {
        switch (info.clipType) {

        case LPainterClipInfo::RegionClip: {
            LTransform2D matrix = (info.matrix * m_invMatrix);
            if (lastWasNothing) {
                region = matrix.map(info.region);
                lastWasNothing = false;
                continue;
            }
            if (info.operation == LClipOper::Intersect)
                region->intersect(matrix.map(info.region));
            else if (info.operation == LClipOper::No) {
                lastWasNothing = true;
                region->clear();
            } else
                region = matrix.map(info.region);
            break;
        }

        case LPainterClipInfo::PathClip: {
            LTransform2D matrix = (info.matrix * m_invMatrix);

            if (lastWasNothing) {
                std::shared_ptr<LPainterPath> mapPath =  matrix.map(info.path);
                region = std::make_shared<LRegion>(mapPath->toFillPolygon().toPolygon(),info.path->fillRule());
                lastWasNothing = false;
                continue;
            }
            if (info.operation == LClipOper::Intersect) {
                std::shared_ptr<LPainterPath> mapPath =  matrix.map(info.path);
                std::shared_ptr<LRegion> tRegion =
                    std::make_shared<LRegion>(mapPath->toFillPolygon().toPolygon(),info.path->fillRule());
                region->intersect(tRegion);
            } else if (info.operation == LClipOper::No) {
                lastWasNothing = true;
                region->clear();
            } else {
                std::shared_ptr<LPainterPath> mapPath =  matrix.map(info.path);
                region = std::make_shared<LRegion>(mapPath->toFillPolygon().toPolygon(),info.path->fillRule());
            }
            break;
        }

        case LPainterClipInfo::RectClip: {
            LTransform2D matrix = (info.matrix * m_invMatrix);
            LRect<int> rect = info.rect.toRect();
            if (lastWasNothing) {
                std::shared_ptr<LRegion> tRegion = std::make_shared<LRegion>(rect);
                region = matrix.map(tRegion);
                lastWasNothing = false;
                continue;
            }

            if (info.operation == LClipOper::Intersect) {
                // Use rect intersection if possible.
                if (matrix.type() <= LTransform2D::TxScale){
                    region->intersect(matrix.mapRect(rect));
                }else{
                    std::shared_ptr<LRegion> tRegion = std::make_shared<LRegion>(rect);
                    tRegion = matrix.map(tRegion);
                    region->intersect(tRegion);
                }
            } else if (info.operation == LClipOper::No) {
                lastWasNothing = true;
                region->clear();
            } else {
                std::shared_ptr<LRegion> tRegion = std::make_shared<LRegion>(rect);
                region = matrix.map(tRegion);
            }
            break;
        }
        default:{
            break;
        }
        }
    }

    return region;
}

void LPainter::setBackground(const std::shared_ptr<LBrush> bg)
{
    if (m_state == nullptr) {
        return;
    }
    m_state->bgBrush = bg;
}

void LPainter::setBackgroundMode(LBGMode mode)
{
    if (m_state== nullptr) {
        return;
    }
    if (m_state->bgMode == mode){
        return;
    }

    m_state->bgMode = mode;
}

void LPainter::setBrushOrigin(const LPoint<double> &p)
{
    if (m_state == nullptr) {
        return;
    }

    m_state->brushOrigin = p;
    if(m_engine = nullptr){
        m_engine->brushOriginChanged();
    }
}

const std::shared_ptr<LBrush> LPainter::background() const
{
    if (m_state == nullptr) {
        return fakeState()->brush;
    }

    if(m_state->bgBrush == nullptr){
        m_state->bgBrush = std::make_shared<LBrush>();
    }
    return m_state->bgBrush;
}

const LBGMode LPainter::backgroundMode() const
{
    if(m_state == nullptr){
        return LBGMode::Transparent;
    }
    return m_state->bgMode;
}

const LPoint<double> LPainter::brushOrigin() const
{
    if(m_state == nullptr){
        return LPoint<double>();
    }
    return m_state->brushOrigin;
}

double LPainter::opacity() const
{
    if(m_state == nullptr){
        return 1.0;
    }
    return m_state->opacity;
}

void LPainter::setOpacity(double opacity)
{
    if (m_engine == nullptr || m_state == nullptr) {
        return;
    }

    opacity = std::min<double>(double(1), std::max<double>(double(0), opacity));

    if (opacity == m_state->opacity){
        return;
    }

    m_state->opacity = opacity;
    m_engine->opacityChanged();
}

void LPainter::scale(double sx, double sy)
{
    if (m_state == nullptr) {
        return;
    }

    m_state->worldMatrix.scale(sx,sy);
    m_state->transWorld = true;
    updateMatrix();
}

void LPainter::shear(double sh, double sv)
{
    if (m_state == nullptr) {
        return;
    }

    m_state->worldMatrix.shear(sh, sv);
    m_state->transWorld = true;
    updateMatrix();
}

void LPainter::rotate(double a)
{
    if (m_state == nullptr) {
        return;
    }

    m_state->worldMatrix.rotate(a);
    m_state->transWorld = true;
    updateMatrix();
}

LRect<int> LPainter::window() const
{
    if (m_state == nullptr) {
        return LRect<int>();
    }
    return LRect<int>(m_state->wx, m_state->wy, m_state->ww, m_state->wh);
}

void LPainter::setWindow(const LRect<int> &r)
{
    if (m_state == nullptr) {
        return;
    }

    m_state->wx = r.m_x;
    m_state->wy = r.m_y;
    m_state->ww = r.m_width;
    m_state->wh = r.m_height;

    m_state->transView = true;
    updateMatrix();
}


void LPainter::updateState(std::shared_ptr<LPainterState> newState)
{
    if(m_engine != nullptr){
        if (newState == nullptr) {
            m_engine->m_state = newState;
        } else if ((newState->dirtyFlags !=uint16_t(0)) || m_engine->m_state!=newState) {
            updateStateImpl(newState);
        }
    }
}

void LPainter::updateStateImpl(std::shared_ptr<LPainterState> newState)
{
    if(m_engine != nullptr){
        if (m_engine->m_state == nullptr) {
            m_engine->m_state = newState;
            m_engine->setDirty(LDirty::All);
        }

        if (m_engine->m_state->painter != newState->painter){
            // ### this could break with clip regions vs paths.
            m_engine->setDirty(LDirty::All);
        }
        // Upon restore, revert all changes since last save
        else if (m_engine->m_state != newState){
            std::shared_ptr<LPainterState> pState = std::dynamic_pointer_cast<LPainterState>(m_state);
            if(pState != nullptr){
                newState->dirtyFlags |= pState->changeFlags;
            }
        }
        // We need to store all changes made so that restore can deal with them
        else{
            newState->changeFlags |= uint32_t(newState->dirtyFlags);
        }
        newState->dirtyFlags &= ~uint32_t(LDirty::BackgroundMode|LDirty::Background);

        m_engine->m_state = newState;
        m_engine->updateState(newState);
        m_engine->clearDirty(LDirty::All);
    }
}

double LPainter::effectiveDevicePixelRatio() const
{
    if(m_device==nullptr){
        return double(1);
    }

    return std::max(double(1), m_device->devicePixelRatioF());
}

void LPainter::updateMatrix()
{
    m_state->matrix = m_state->transWorld ? m_state->worldMatrix : LTransform2D();
    if (m_state->transView){
        m_state->matrix *= viewTransform();
    }

    m_txinv = false;                                // no inverted matrix
    m_state->matrix *= m_state->redirectionMatrix;
    m_engine->transformChanged();
    m_state->matrix *= hidpiScaleTransform();
}

void LPainter::updateInvMatrix()
{
    m_txinv = true;                                // creating inverted matrix
    m_invMatrix = m_state->matrix.inverted();

}

LTransform2D LPainter::viewTransform() const
{
    if (m_state->transView) {
        double scaleW = double(m_state->vw)/double(m_state->ww);
        double scaleH = double(m_state->vh)/double(m_state->wh);
        return LTransform2D(scaleW, 0, 0, scaleH,
                            m_state->vx - m_state->wx*scaleW,
                            m_state->vy - m_state->wy*scaleH);
    }
    return LTransform2D();
}

LTransform2D LPainter::hidpiScaleTransform() const
{
    const double devicePixelRatio = effectiveDevicePixelRatio();
    return LTransform2D::fromScale(devicePixelRatio, devicePixelRatio);
}

void LPainter::cleanupPainterState(LPainter *d)
{
    d->m_states.clear();
    d->m_state = nullptr;
    d->m_engine = nullptr;
    d->m_device = nullptr;
}

std::shared_ptr<LPainterDummyState> LPainter::fakeState()
{
    if (m_dummyState == nullptr){
        m_dummyState = std::make_shared<LPainterDummyState>();
        m_dummyState->font = LGuiApplication::font();
        m_dummyState->pen = std::make_shared<LPen>();
        m_dummyState->brush = std::make_shared<LBrush>();
    }
    return m_dummyState;

}



LK_NAMESPACE_END
