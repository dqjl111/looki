#include "lscrollarea.h"

LK_NAMESPACE_BEGIN

LScrollArea::LScrollArea(std::shared_ptr<LWidget> parent): LAbstractScrollArea(parent)
{

}


void LScrollArea::initSpecific()
{
    initViewport();
    initScroll();
    m_viewport->setBackgroundRole(LPalette::NoRole);
    m_vBar->setSingleStep(20);
    m_hBar->setSingleStep(20);
    layoutChildren();
}

std::shared_ptr<LWidget> LScrollArea::widget() const
{
    return m_widget;
}

void LScrollArea::setWidget(std::shared_ptr<LWidget> widget)
{
    if (widget == m_widget || widget == nullptr){
        return;
    }

    m_widget = nullptr;
    m_hBar->setValue(0);
    m_vBar->setValue(0);
    if (widget->parentT<LWidget>() != m_viewport){
        widget->setParent(m_viewport);
    }
    if (!widget->testAttribute(LWidgetAttribute::Resized)){
        widget->resize(widget->sizeHint());
    }
    m_widget = widget;
    m_widget->setAutoFillBackground(true);

    std::shared_ptr<LAbstractScrollArea> q= getPtrT<LAbstractScrollArea>();
    m_widget->addEventFilter(q);
    m_widgetSize.reset();
    updateScrollBars();
    m_widget->show();
}

std::shared_ptr<LWidget> LScrollArea::takeWidget()
{
    std::shared_ptr<LWidget> w = m_widget;
    m_widget = nullptr;
    if (w != nullptr){
        w->setParent(nullptr, uint32_t(m_widgetData.windowFlags) & ~uint32_t(LWindowType::WindowTypeMask));
    }
    return w;
}

bool LScrollArea::widgetResizable() const
{
    return m_resizable;
}

void LScrollArea::setWidgetResizable(bool resizable)
{
    m_resizable = resizable;
    updateGeometry();
    updateScrollBars();
}

LSize<int> LScrollArea::sizeHint() const
{
    int f = 2 * frameWidth();
    LSize<int> sz(f, f);
    int h = (int)fontMetrics().height();
    if (m_widget != nullptr) {
        if (!m_widgetSize.isValid()){
            m_widgetSize= m_resizable ? m_widget->sizeHint() : m_widget->size();
        }
        sz += m_widgetSize;
    } else {
        sz += LSize<int>(12 * h, 8 * h);
    }
    if (m_vPolicy == LScrollPolicy::AlwaysOn){
        sz.setWidth(sz.width() + m_vBar->sizeHint().width());
    }
    if (m_hPolicy == LScrollPolicy::AlwaysOn){
        sz.setHeight(sz.height() + m_hBar->sizeHint().height());
    }
    return sz.boundedTo(LSize<int>(36 * h, 24 * h));
}

bool LScrollArea::focusNextPrevChild(bool next)
{
    if (LWidget::focusNextPrevChild(next)) {
        std::shared_ptr<LWidget> fw = focusWidget();
        if (fw != nullptr){
            ensureWidgetVisible(fw);
        }
        return true;
    }
    return false;
}

LFlags<LAlignment> LScrollArea::alignment() const
{
    return m_alignment;
}

void LScrollArea::setAlignment(LFlags<LAlignment> a)
{
    m_alignment = a;
    if (m_widget != nullptr){
        updateWidgetPosition();
    }
}

void LScrollArea::ensureVisible(int x, int y, int xmargin, int ymargin)
{
    int logicalX = LDrawUtil::visualPos<int>(layoutDirection(), m_viewport->rect(), LPoint<int>(x, y)).m_x;

    if (logicalX - xmargin < m_hBar->value()) {
        m_hBar->setValue(std::max<int>(0, logicalX - xmargin));
    } else if (logicalX > m_hBar->value() + m_viewport->width() - xmargin) {
        m_hBar->setValue(std::min<int>(logicalX - m_viewport->width() + xmargin, m_hBar->maximum()));
    }

    if (y - ymargin < m_vBar->value()) {
        m_vBar->setValue(std::max<int>(0, y - ymargin));
    } else if (y > m_vBar->value() + m_viewport->height() - ymargin) {
        m_vBar->setValue(std::min<int>(y - m_viewport->height() + ymargin, m_vBar->maximum()));
    }
}

void LScrollArea::ensureWidgetVisible(std::shared_ptr<LWidget> childWidget, int xmargin, int ymargin)
{
    if (!m_widget->isAncestorOf(childWidget)){
        return;
    }

    LRect<int> microFocus(childWidget->inputMethodQuery(LInputMethodQuery::ImCursorRectangle).toCbor());
    LRect<int> defaultMicroFocus(childWidget->LWidget::inputMethodQuery(LInputMethodQuery::ImCursorRectangle).toCbor());
    LRect<int> focusRect = (microFocus != defaultMicroFocus)
                          ? LRect<int>(childWidget->mapTo(m_widget, microFocus.topLeft()), microFocus.size())
                          : LRect<int>(childWidget->mapTo(m_widget, LPoint<int>(0,0)), childWidget->size());
    const LRect<int> visibleRect(-m_widget->pos(), m_viewport->size());

    if (visibleRect.contains(focusRect)){
        return;
    }

    focusRect.adjust(-xmargin, -ymargin, xmargin, ymargin);

    if (focusRect.width() > visibleRect.width()){
        m_hBar->setValue(focusRect.center().m_x - m_viewport->width() / 2);
    }else if (focusRect.x2() > visibleRect.x2()){
        m_hBar->setValue(focusRect.x2() - m_viewport->width() + 1);
    }else if (focusRect.m_x < visibleRect.m_x){
        m_hBar->setValue(focusRect.m_x);
    }

    if (focusRect.height() > visibleRect.height()){
        m_vBar->setValue(focusRect.center().m_y - m_viewport->height() / 2);
    }else if (focusRect.y2() > visibleRect.y2()){
        m_vBar->setValue(focusRect.y2() - m_viewport->height() + 1);
    }else if (focusRect.m_y < visibleRect.m_y){
        m_vBar->setValue(focusRect.m_y);
    }
}


bool LScrollArea::handleEvent(std::shared_ptr<LEvent> e)
{
    if (e->type() == LEvent::StyleChange || e->type() == LEvent::LayoutRequest) {
        updateScrollBars();
    }
    return LAbstractScrollArea::handleEvent(e);
}

bool LScrollArea::eventFilter(std::shared_ptr<LObject> o, std::shared_ptr<LEvent> e)
{
    if (o == m_widget && e->type() == LEvent::Resize){
        updateScrollBars();
    }
    return LAbstractScrollArea::eventFilter(o, e);
}

bool LScrollArea::viewportEvent(std::shared_ptr<LEvent> e)
{
    switch (e->type()) {
    case LEvent::Resize:
    case LEvent::MouseButtonPress:
    case LEvent::MouseButtonRelease:
    case LEvent::MouseButtonDblClick:
    case LEvent::TouchBegin:
    case LEvent::TouchUpdate:
    case LEvent::TouchEnd:
    case LEvent::MouseMove:
    case LEvent::ContextMenu:
    case LEvent::Wheel:
    case LEvent::Drop:
    case LEvent::DragEnter:
    case LEvent::DragMove:
    case LEvent::DragLeave:{
        return LFrame::handleEvent(e);
    }
    case LEvent::LayoutRequest:
    case LEvent::Gesture:
    case LEvent::GestureOverride:{
        return handleEvent(e);
    }
    case LEvent::ScrollPrepare:
    case LEvent::Scroll:{
        return handleEvent(e);
    }
    default:
        break;
    }
    return false;
}

void LScrollArea::resizeEvent(std::shared_ptr<LResizeEvent> e)
{
    updateScrollBars();
}

void LScrollArea::scrollContentsBy(int dx, int dy)
{
    if (m_widget == nullptr){
        return;
    }
    updateWidgetPosition();
}

LSize<int> LScrollArea::viewportSizeHint() const
{
    if (m_widget != nullptr) {
        return m_resizable ? m_widget->sizeHint() : m_widget->size();
    }
    const int h = (int)fontMetrics().height();
    return LSize<int>(6 * h, 4 * h);
}

void LScrollArea::updateWidgetPosition()
{
    LLayoutDirection dir = layoutDirection();
    LRect<int> scrolled = LDrawUtil::visualRect<int>(dir,
                                                     m_viewport->rect(),
                                                     LRect<int>(-m_hBar->value(), -m_vBar->value(), m_widget->size()));
    LRect<int> aligned = LDrawUtil::alignedRect(dir, m_alignment, m_widget->size(), m_viewport->rect());
    m_widget->move(m_widget->width() < m_viewport->width() ? aligned.m_x : scrolled.m_x,
                m_widget->height() < m_viewport->height() ? aligned.m_y : scrolled.m_y);
}

void LScrollArea::updateScrollBars()
{
    if (m_widget == nullptr){
        return;
    }
    LSize<int> p = m_viewport->size();
    LSize<int> m = maximumViewportSize();

    LSize<int> min = LLayoutItem::smartMinSize(m_widget);
    LSize<int> max = LLayoutItem::smartMaxSize(m_widget);

    if (m_resizable) {
        std::shared_ptr<LLayout> wLayout = m_widget->layout();
        if ((wLayout!= nullptr ? wLayout->hasHeightForWidth() : m_widget->sizePolicy().hasHeightForWidth())) {
            LSize<int>  pHfw = p.expandedTo(min).boundedTo(max);
            int h = m_widget->heightForWidth(pHfw.width());
            // If the height we calculated requires a vertical scrollbar,
            // then we need to constrain the width and calculate the height again,
            // otherwise we end up flipping the scrollbar on and off all the time.
            if (m_vPolicy == LScrollPolicy::AsNeeded) {
                int vbarWidth = m_vBar->sizeHint().width();
                LSize<int> mHfw = m.expandedTo(min).boundedTo(max);
                // is there any point in searching?
                if (m_widget->heightForWidth(mHfw.width() - vbarWidth) <= m.height()) {
                    while (h > m.height() && vbarWidth) {
                        --vbarWidth;
                        --mHfw.rwidth();
                        h = m_widget->heightForWidth(mHfw.width());
                    }
                }
                max = LSize<int>(mHfw.width(), std::max<int>(mHfw.height(), h));
            }
            min = LSize<int>(pHfw.width(), std::max<int>(pHfw.height(), h));
        }
    }

    if ((m_resizable && m.expandedTo(min) == m && m.boundedTo(max) == m)
        || (!m_resizable && m.expandedTo(m_widget->size()) == m)){
        p = m; // no scroll bars needed
    }

    if (m_resizable){
        m_widget->resize(p.expandedTo(min).boundedTo(max));
    }
    LSize<int> v = m_widget->size();

    m_hBar->setRange(0, v.width() - p.width());
    m_hBar->setPageStep(p.width());
    m_vBar->setRange(0, v.height() - p.height());
    m_vBar->setPageStep(p.height());
    updateWidgetPosition();
}


LK_NAMESPACE_END
