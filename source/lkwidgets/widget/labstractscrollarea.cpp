#include "labstractscrollarea.h"
#include "lkwidgets/draw/lstyle.h"

LK_NAMESPACE_BEGIN
LAbstractScrollArea::LAbstractScrollArea(std::shared_ptr<LWidget> parent ): LFrame(parent)
{
    setFrameShape(LFrame::StyledPanel);
    setFrameShadow(LFrame::Sunken);
    setSizePolicy(LSizePolicy(LSizePolicy::Expanding, LSizePolicy::Expanding));
}


void LAbstractScrollArea::initScroll()
{
    std::shared_ptr<LAbstractScrollArea> q= getPtrT<LAbstractScrollArea>();
    m_hBar = makeInit<LScrollBar>(q, LOrientation::Horizontal);
    m_hBar->setRange(0,0);
    m_hBar->setVisible(false);

    m_vBar = makeInit<LScrollBar>(q, LOrientation::Vertical);
    m_vBar->setRange(0,0);
    m_vBar->setVisible(false);

    m_hBar->sigValueChanged.connect(this, &LAbstractScrollArea::hSlide);
    m_vBar->sigValueChanged.connect(this, &LAbstractScrollArea::vSlide);

    m_hBar->sigRangeChanged.connect(this, &LAbstractScrollArea::hRangeChange, ConnectionType::Queued);
    m_vBar->sigRangeChanged.connect(this, &LAbstractScrollArea::vRangeChange, ConnectionType::Queued);
}

void LAbstractScrollArea::initSpecific()
{
    initViewport();
    initScroll();
    layoutChildren();
    setFocusPolicy(LFocusPolicy::Strong);
}

void LAbstractScrollArea::initViewport()
{
    std::shared_ptr<LAbstractScrollArea> q= getPtrT<LAbstractScrollArea>();
    m_viewport = makeInit<LWidget>(q);
    m_viewport->setBackgroundRole(LPalette::Base);
    m_viewport->setAutoFillBackground(true);
    m_viewport->addEventFilter(q);
}

bool LAbstractScrollArea::viewportEvent(std::shared_ptr<LEvent> e)
{
    switch (e->type()) {
    case LEvent::Resize:
    case LEvent::Wheel:{
        return LFrame::handleEvent(e);
    }
    case LEvent::LayoutRequest:{
        return handleEvent(e);
    }
    case LEvent::ScrollPrepare:
    case LEvent::Scroll:{
        return handleEvent(e);
    }
    default:
        break;
    }
    return false; // let the viewport widget handle the event
}

bool LAbstractScrollArea::eventFilter(std::shared_ptr<LObject> o, std::shared_ptr<LEvent> e)
{
    if(o == m_viewport){
        return viewportEvent(e);
    }
    return false;
}

void LAbstractScrollArea::blink()
{
    m_viewport->blink();
}

void LAbstractScrollArea::hSlide(int x)
{
    int dx = m_offset.m_x - x;
    m_offset.m_x = x;
    scrollContentsBy(dx, 0);
    if (m_hBar->isVisible()){
        m_hBar->update();
    }
}

void LAbstractScrollArea::vSlide(int y)
{
    int dy = m_offset.m_y - y;
    m_offset.m_y = y;
    scrollContentsBy(0, dy);
    if (m_vBar->isVisible()){
        m_vBar->update();
    }
}

void LAbstractScrollArea::hRangeChange(int, int)
{
    layoutChildren();
}

void LAbstractScrollArea::vRangeChange(int, int)
{
    layoutChildren();
}

void LAbstractScrollArea::layoutChildren()
{
    bool needH = false;
    bool needV = false;
    layoutChildrenHelper(&needH, &needV);
    // Call a second time if one scrollbar was needed and not the other to
    // check if it needs to readjust accordingly
    if (needH != needV){
        layoutChildrenHelper(&needH, &needV);
    }
}

void LAbstractScrollArea::layoutChildrenHelper(bool *needHBar, bool *needVBar)
{
    bool needh = *needHBar;
    if(needh == false){
        if(m_hPolicy == LScrollPolicy::AlwaysOn){
            needh = true;
        }else if(m_hPolicy == LScrollPolicy::AsNeeded){
            needh = m_hBar->minimum() < m_hBar->maximum() && !m_hBar->sizeHint().isEmpty();
        }
    }


    bool needv = *needVBar;
    if(needv == false){
        if(m_vPolicy == LScrollPolicy::AlwaysOn){
            needv = true;
        }else if(m_vPolicy == LScrollPolicy::AsNeeded){
            needv = m_vBar->minimum() < m_vBar->maximum() && !m_vBar->sizeHint().isEmpty();
        }
    }


    LStyleOpt opt;
    initStyleOpt(opt);

    const int hsbExt = m_hBar->sizeHint().height();
    const int vsbExt = m_vBar->sizeHint().width();
    const LPoint<int> extPoint(vsbExt, hsbExt);
    const LSize<int> extSize(vsbExt, hsbExt);
    const bool hasCornerWidget = (m_cornerWidget != nullptr);

    LPoint<int> cornerOffset(needv ? vsbExt : 0, needh ? hsbExt : 0);
    LRect<int> controlsRect = contentsRect();
    LRect<int> viewportRect = LRect<int>(controlsRect.topLeft(), controlsRect.bottomRight() - cornerOffset);
    cornerOffset = LPoint<int>(needv ? vsbExt : 0, needh ? hsbExt : 0);

    // If we have a corner widget and are only showing one scroll bar, we need to move it
    // to make room for the corner widget.
    if (hasCornerWidget && (needv || needh)){
        cornerOffset =  extPoint;
    }

    // The corner point is where the scroll bar rects, the corner widget rect and the
    // viewport rect meets.
    const LPoint<int> cornerPoint(controlsRect.bottomRight() + LPoint<int>(1, 1) - cornerOffset);

    // Some styles paints the corner if both scorllbars are showing and there is
    // no corner widget.
    if (needv && needh && !hasCornerWidget){
        m_cornerPaintingRect = LDrawUtil::visualRect<int>(opt.direction, opt.rect, LRect<int>(cornerPoint, extSize));
    }else{
        m_cornerPaintingRect = LRect<int>();
    }

    // move the scrollbars away from top/left headers
    int vHeaderRight = 0;
    int hHeaderBottom = 0;

    if (needh) {
        LRect<int> horizontalScrollBarRect(LPoint<int>(controlsRect.m_x + vHeaderRight, cornerPoint.m_y),
                                           LPoint<int>(cornerPoint.m_x - 1, controlsRect.y2()));
        m_hBar->setGeometry(LDrawUtil::visualRect<int>(opt.direction, opt.rect, horizontalScrollBarRect));
        m_hBar->raise();
    }

    if (needv) {
        LRect<int> verticalScrollBarRect(LPoint<int>(cornerPoint.m_x, controlsRect.m_y + hHeaderBottom),
                                         LPoint<int>(controlsRect.x2(), cornerPoint.m_y - 1));
        m_vBar->setGeometry(LDrawUtil::visualRect<int>(opt.direction, opt.rect, verticalScrollBarRect));
        m_vBar->raise();
    }

    if (m_cornerWidget!= nullptr) {
        const LRect<int> cornerWidgetRect(cornerPoint, controlsRect.bottomRight());
        m_cornerWidget->setGeometry(LDrawUtil::visualRect<int>(opt.direction, opt.rect, cornerWidgetRect));
    }

    m_hBar->setVisible(needh);
    m_vBar->setVisible(needv);

    viewportRect.adjust(m_viewMargin.m_left, m_viewMargin.m_top, -m_viewMargin.m_right, -m_viewMargin.m_bottom);
    viewportRect = LDrawUtil::visualRect<int>(opt.direction, opt.rect, viewportRect);
    viewportRect.translate(-m_overshoot);
    m_viewport->setGeometry(viewportRect); // resize the viewport last

    *needHBar = needh;
    *needVBar = needv;
}

LSize<int> LAbstractScrollArea::viewportSizeHint() const
{
    if (m_viewport != nullptr) {
        const LSize<int> sh = m_viewport->sizeHint();
        if (sh.isValid()) {
            return sh;
        }
    }
    const int h = std::max<int>(10, std::lround(fontMetrics().height()));
    return LSize<int>(6 * h, 4 * h);
}

void LAbstractScrollArea::scrollContentsBy(int dx, int dy)
{
    if (m_viewport != nullptr) {
        m_viewport->update();
    }
}

bool LAbstractScrollArea::canStartScrollingAt(const LPoint<int> &startPos) const
{
    if(m_viewport != nullptr){
        std::shared_ptr<LWidget> c = m_viewport->childAt(startPos);
        if(c != nullptr){
            std::shared_ptr<LAbstractSlider> s = std::dynamic_pointer_cast<LAbstractSlider>(c);
            return s == nullptr;
        }
        return true;
    }
    return false;
}

void LAbstractScrollArea::replaceScrollBar(std::shared_ptr<LScrollBar> scrollBar, LOrientation orientation)
{
    bool horizontal = (orientation == LOrientation::Horizontal);
    std::shared_ptr<LScrollBar> oldBar = horizontal ? m_hBar : m_vBar;
    if(horizontal){
        m_hBar = scrollBar;
    }else{
        m_vBar = scrollBar;
    }

    std::shared_ptr<LWidget> q= getPtrT<LAbstractScrollArea>();

    oldBar->sigValueChanged.disconnect(this);
    oldBar->sigRangeChanged.disconnect(this);

    scrollBar->setParent(q);
    scrollBar->setVisible(oldBar->isVisibleTo(q));
    scrollBar->setRange(oldBar->minimum(), oldBar->maximum());
    scrollBar->setOrientation(oldBar->orientation());
    scrollBar->setPageStep(oldBar->pageStep());
    scrollBar->setSingleStep(oldBar->singleStep());
    scrollBar->setSliderDown(oldBar->isSliderDown());
    scrollBar->setSliderPosition(oldBar->sliderPosition());
    scrollBar->setTracking(oldBar->hasTracking());
    scrollBar->setValue(oldBar->value());


    removeChild(oldBar);


    if(horizontal){
        m_hBar->sigValueChanged.connect(this, &LAbstractScrollArea::hSlide);
        m_hBar->sigRangeChanged.connect(this, &LAbstractScrollArea::hRangeChange, ConnectionType::Queued);
    }else{
        m_vBar->sigValueChanged.connect(this, &LAbstractScrollArea::vSlide);
        m_vBar->sigRangeChanged.connect(this, &LAbstractScrollArea::vRangeChange, ConnectionType::Queued);
    }
    layoutChildren();
}

std::shared_ptr<LWidget> LAbstractScrollArea::viewport() const
{
    return m_viewport;
}

LSize<int> LAbstractScrollArea::maximumViewportSize() const
{
    int f = 2 * frameWidth();
    LSize<int> max = size() - LSize<int>(f + m_viewMargin.m_left + m_viewMargin.m_right,
                                         f + m_viewMargin.m_top + m_viewMargin.m_bottom);
    // Count the sizeHint of the bar only if it is displayed.
    if (m_vPolicy == LScrollPolicy::AlwaysOn){
        max.rwidth() -= m_vBar->sizeHint().width();
    }
    if (m_hPolicy == LScrollPolicy::AlwaysOn){
        max.rheight() -= m_hBar->sizeHint().height();
    }
    return max;
}

LPoint<int> LAbstractScrollArea::contentsOffset() const
{
    LPoint<int> offset;
    if (m_vBar->isVisible()){
        offset.m_y = m_vBar->value();
    }
    if (m_hBar->isVisible()) {
        offset.m_x = m_hBar->value();
    }
    return offset;
}

LSize<int> LAbstractScrollArea::minimumSizeHint() const
{
    int hsbExt = m_hBar->sizeHint().height();
    int vsbExt = m_vBar->sizeHint().width();
    int extra = 2 * frameWidth();
    return LSize<int>(m_vBar->sizeHint().width() + vsbExt + extra,
                      m_vBar->sizeHint().height() + hsbExt + extra);
}

LSize<int> LAbstractScrollArea::sizeHint() const
{
    if (m_sizeAdjustPolicy == LAbstractScrollArea::AdjustIgnored){
        return LSize<int>(256, 192);
    }

    if (!m_sizeHint.isValid() || m_sizeAdjustPolicy == AdjustToContents) {
        std::shared_ptr<LWidget> q= getPtrT<LWidget>();

        const int f = 2 * frameWidth();
        const LSize<int> frame(f, f);
        const bool vbarHidden = m_vPolicy == LScrollPolicy::AlwaysOff || (!m_vBar->isVisibleTo(q));
        const bool hbarHidden = m_hPolicy == LScrollPolicy::AlwaysOff || (!m_hBar->isVisibleTo(q));

        const LSize<int> scrollbars(vbarHidden ? 0 : m_vBar->sizeHint().width(),
                               hbarHidden ? 0 : m_hBar->sizeHint().height());
        m_sizeHint = frame + scrollbars + viewportSizeHint();
    }
    return m_sizeHint;
}

void LAbstractScrollArea::setSizeAdjustPolicy(SizeAdjustPolicy policy)
{
    if (m_sizeAdjustPolicy == policy){
        return;
    }

    m_sizeAdjustPolicy = policy;
    m_sizeHint.reset();
    updateGeometry();
}

LScrollPolicy LAbstractScrollArea::verticalScrollBarPolicy() const
{
    return m_vPolicy;
}

void LAbstractScrollArea::setVerticalScrollBarPolicy(LScrollPolicy policy)
{
    const LScrollPolicy oldPolicy = m_vPolicy;
    m_vPolicy = policy;

    if (oldPolicy != m_vPolicy){
        layoutChildren();
        scrollBarPolicyChanged(LOrientation::Vertical, m_vPolicy);
    }else if(isVisible()){
        layoutChildren();
    }
}

LScrollPolicy LAbstractScrollArea::horizontalScrollBarPolicy() const
{
    return m_hPolicy;
}

void LAbstractScrollArea::setHorizontalScrollBarPolicy(LScrollPolicy policy)
{
    const LScrollPolicy oldPolicy = m_hPolicy;
    m_hPolicy = policy;
    if (oldPolicy != m_hPolicy){
        layoutChildren();
        scrollBarPolicyChanged(LOrientation::Horizontal, m_hPolicy);
    }else if(isVisible()){
        layoutChildren();
    }
}

std::shared_ptr<LScrollBar> LAbstractScrollArea::verticalScrollBar() const
{
    return m_vBar;
}

void LAbstractScrollArea::setVerticalScrollBar(std::shared_ptr<LScrollBar> scrollBar)
{
    if(scrollBar != nullptr && scrollBar != m_vBar && scrollBar != m_hBar){
        replaceScrollBar(scrollBar, LOrientation::Vertical);
    }
}

std::shared_ptr<LScrollBar> LAbstractScrollArea::horizontalScrollBar() const
{
    return m_hBar;
}

void LAbstractScrollArea::setHorizontalScrollBar(std::shared_ptr<LScrollBar> scrollBar)
{
    if(scrollBar != nullptr && scrollBar != m_vBar && scrollBar != m_hBar){
        replaceScrollBar(scrollBar, LOrientation::Horizontal);
    }
}

bool LAbstractScrollArea::handleEvent(std::shared_ptr<LEvent> e)
{
    switch (e->type()) {
    case LEvent::AcceptDropsChange:{
        if (m_viewport != nullptr){
            m_viewport->setAcceptDrops(acceptDrops());
        }
        break;
    }
    case LEvent::MouseTrackingChange:{
        if (m_viewport != nullptr){
            m_viewport->setMouseTracking(hasMouseTracking());
        }
        break;
    }
    case LEvent::Resize:{
        if (!m_inResize) {
            m_inResize = true;
            layoutChildren();
            m_inResize = false;
        }
        break;
    }
    case LEvent::Show:{
        if (!m_shownOnce && m_sizeAdjustPolicy == LAbstractScrollArea::AdjustToContentsOnFirstShow) {
            m_sizeHint.reset();
            updateGeometry();
        }
        m_shownOnce = true;
        return LFrame::handleEvent(e);
    }
    case LEvent::Paint:{
        // 先铺内容底色：viewport 区域自身不绘制背景，否则会露出未绘制内容（暗色主题下尤其明显）
        {
            LPainter bgPainter(paintDevice());
            bgPainter.fillRect(rect().toRectF(), palette()->brush(LPalette::Base));
        }

        if (m_cornerPaintingRect.isValid()) {
            LPainter painter(paintDevice());
            std::shared_ptr<LStyle> pStyle = style();
            std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
            pStyle->drawPrimitive(tThis, m_cornerPaintingRect, &painter, LUiElementType::Corner);
        }

        std::shared_ptr<LPaintEvent> pe = std::dynamic_pointer_cast<LPaintEvent>(e);
        LFrame::paintEvent(pe);
        break;
     }
    case LEvent::ContextMenu:{
         std::shared_ptr<LContextMenuEvent> me = std::dynamic_pointer_cast<LContextMenuEvent>(e);
        if (me->reason() == LContextMenuEvent::Keyboard){
            return LFrame::handleEvent(e);
        }
        e->ignore();
        break;
    }
    case LEvent::MouseButtonPress:
    case LEvent::MouseButtonRelease:
    case LEvent::MouseButtonDblClick:
    case LEvent::MouseMove:
    case LEvent::Wheel:
    case LEvent::Drop:
    case LEvent::DragEnter:
    case LEvent::DragMove:
    case LEvent::DragLeave:
    // ignore touch events in case they have been propagated from the viewport
    case LEvent::TouchBegin:
    case LEvent::TouchUpdate:
    case LEvent::TouchEnd:{
        return false;
    }
    case LEvent::Gesture:{
        return false;
    }
    case LEvent::ScrollPrepare:{
        std::shared_ptr<LScrollPrepareEvent> se = std::dynamic_pointer_cast<LScrollPrepareEvent>(e);
        if (canStartScrollingAt(se->startPos().toPoint())) {
            se->setViewportSize(m_viewport->sizeF());
            se->setContentPosRange(LRect<double>(0, 0, m_hBar->maximum(), m_vBar->maximum(), false));
            se->setContentPos(LPoint<double>(m_hBar->value(), m_vBar->value()));
            se->accept();
            return true;
        }
        return false;
    }
    case LEvent::Scroll:{
        std::shared_ptr<LScrollEvent> se = std::dynamic_pointer_cast<LScrollEvent>(e);

        m_hBar->setValue(se->contentPos().m_x);
        m_vBar->setValue(se->contentPos().m_y);

        LPoint<int> delta = m_overshoot - se->overshootDistance().toPoint();
        if (!delta.isNull()){
            viewport()->move(viewport()->pos() + delta);
        }

        m_overshoot = se->overshootDistance().toPoint();
        return true;
    }
    case LEvent::StyleChange:
    case LEvent::LayoutDirectionChange:
    case LEvent::ApplicationLayoutDirectionChange:
    case LEvent::LayoutRequest:{
        layoutChildren();
        _FALLTHROUGH;
    }
    default:{
        return LFrame::handleEvent(e);
    }
    }
    return true;
}

LAbstractScrollArea::SizeAdjustPolicy LAbstractScrollArea::sizeAdjustPolicy() const
{
    return m_sizeAdjustPolicy;
}

void LAbstractScrollArea::keyPressEvent(std::shared_ptr<LKeyEvent> ev)
{
    if(m_viewport != nullptr){
        m_viewport->handleEvent(ev);
    }
}

void LAbstractScrollArea::inputMethodEvent(std::shared_ptr<LInputMethodEvent> ev)
{
    if(m_viewport != nullptr){
        m_viewport->handleEvent(ev);
    }
}


LK_NAMESPACE_END
