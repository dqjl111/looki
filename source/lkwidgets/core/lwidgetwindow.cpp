#include "lwidgetwindow.h"
#include "lkwidgets/core/lapplication.h"
#include "lkgui/lplatformwindow.h"
#include "lkgui/linputmethod.h"

LK_NAMESPACE_BEGIN
std::weak_ptr<LWidget> LWidgetWindow::m_buttonDown; // widget got last button-down
std::weak_ptr<LWidget> LWidgetWindow::m_popupDown; // popup that contains the pressed widget
std::weak_ptr<LWidget> LWidgetWindow::m_lastMouseReceiver;
int LWidgetWindow::m_openPopupCount = 0;
bool LWidgetWindow::m_popupDownClosed = false; //popup_down has been closed
bool LWidgetWindow::m_replayPopupMouseEvent = false;

static inline bool shouldBePropagatedToWidget(std::shared_ptr<LEvent> event)
{
    switch (event->type()) {
    // Handing show events to widgets would cause them to be triggered twice
    case LEvent::Show:
    case LEvent::Hide:
    case LEvent::Timer:
    case LEvent::DynamicPropertyChange:
    case LEvent::ChildAdded:
    case LEvent::ChildRemoved:{
        return false;
    }
    default:{
        return true;
    }
    }
}



LWidgetWindow::LWidgetWindow(std::shared_ptr<LWidget> w) : m_widget(w)
{
}

std::shared_ptr<LWidget> LWidgetWindow::widget()
{
    return m_widget.lock();
}

bool LWidgetWindow::isWidgetWindow()
{
    return true;
}

std::shared_ptr<LObject> LWidgetWindow::focusObject()
{
    std::shared_ptr<LWidget> windowWidget = m_widget.lock();
    if (windowWidget == nullptr){
        return nullptr;
    }

    // A window can't have a focus object if it's being destroyed.
    if (windowWidget->m_widgetData.inDestructor){
        return nullptr;
    }

    std::shared_ptr<LWidget> widget = windowWidget->focusWidget();
    if (widget == nullptr){
        widget = windowWidget;
    }

    std::shared_ptr<LObject> focusObj = widget->focusObject();
    if (focusObj != nullptr){
        return focusObj;
    }
    return widget;
}

bool LWidgetWindow::handleEvent(std::shared_ptr<LEvent> event)
{
    std::shared_ptr<LWidget> w = m_widget.lock();
    if (w == nullptr){
        return LWindow::handleEvent(event);
    }

    if (w->testAttribute(LWidgetAttribute::DontShowOnScreen)) {
        // \a event is uninteresting for LWidgetWindow, the event was probably
        // generated before LWidgetAttribute::DontShowOnScreen was set
        if (!shouldBePropagatedToWidget(event)){
            return true;
        }
        return LCoreApplication::forwardEvent(w, event);
    }

    switch (event->type()) {
    case LEvent::Close: {
        handleCloseEvent(event);
        LWindow::handleEvent(event);
        return true;
    }

    case LEvent::Enter:
    case LEvent::Leave:{
        handleEnterLeaveEvent(event);
        return true;
    }
    case LEvent::FocusIn:{
        std::shared_ptr<LFocusEvent> evt = std::dynamic_pointer_cast<LFocusEvent>(event);
        handleFocusInEvent(evt);
        return false;
    }
    case LEvent::FocusOut: {
        return false;
    }
    case LEvent::FocusAboutToChange:{
        std::shared_ptr<LWidget>  w = LApplication::focusWidget();
        if(w != nullptr){
            if (w->testAttribute(LWidgetAttribute::InputMethodEnabled)){
                LGuiApplication::inputMethod()->commit();
            }
            LCoreApplication::forwardEvent(w, event);
        }
        return true;
    }
    case LEvent::KeyPress:
    case LEvent::KeyRelease:
    case LEvent::ShortcutOverride:{
        std::shared_ptr<LKeyEvent> evt = std::dynamic_pointer_cast<LKeyEvent>(event);
        handleKeyEvent(evt);
        return true;
    }
    case LEvent::MouseMove:
    case LEvent::MouseButtonPress:
    case LEvent::MouseButtonRelease:
    case LEvent::MouseButtonDblClick:{
        std::shared_ptr<LMouseEvent> evt = std::dynamic_pointer_cast<LMouseEvent>(event);
        handleMouseEvent(evt);
        return true;
    }
    case LEvent::NonClientAreaMouseMove:
    case LEvent::NonClientAreaMouseButtonPress:
    case LEvent::NonClientAreaMouseButtonRelease:
    case LEvent::NonClientAreaMouseButtonDblClick:{
        std::shared_ptr<LMouseEvent> evt = std::dynamic_pointer_cast<LMouseEvent>(event);
        handleNonClientAreaMouseEvent(evt);
        return true;
    }
    case LEvent::TouchBegin:
    case LEvent::TouchUpdate:
    case LEvent::TouchEnd:
    case LEvent::TouchCancel:{
//        std::shared_ptr<LTouchEvent> evt = std::dynamic_pointer_cast<LTouchEvent>(event);
//        handleTouchEvent(evt);
        return true;
    }
    case LEvent::Move:{
        std::shared_ptr<LMoveEvent> evt = std::dynamic_pointer_cast<LMoveEvent>(event);
        handleMoveEvent(evt);
        return true;
    }
    case LEvent::Resize:{
        std::shared_ptr<LResizeEvent> evt = std::dynamic_pointer_cast<LResizeEvent>(event);
        handleResizeEvent(evt);
        return true;
    }
    case LEvent::Wheel:{
        std::shared_ptr<LWheelEvent> evt = std::dynamic_pointer_cast<LWheelEvent>(event);
        handleWheelEvent(evt);
        return true;
    }
/*    case LEvent::DragEnter:{
        std::shared_ptr<LDragEnterEvent> evt = std::dynamic_pointer_cast<LDragEnterEvent>(event);
        handleDragEnterEvent(evt);
        return true;
    }
    case LEvent::DragMove:{
        std::shared_ptr<LDragMoveEvent> evt = std::dynamic_pointer_cast<LDragMoveEvent>(event);
        handleDragMoveEvent(evt);
        return true;
    }
    case LEvent::DragLeave:{
        std::shared_ptr<LDragLeaveEvent> evt = std::dynamic_pointer_cast<LDragLeaveEvent>(event);
        handleDragLeaveEvent(evt);
        return true;
    }
    case LEvent::Drop:{
        std::shared_ptr<LDropEvent> evt = std::dynamic_pointer_cast<LDropEvent>(event);
        handleDropEvent(evt);
        return true;
    }*/
    case LEvent::Expose:{
        std::shared_ptr<LExposeEvent> evt = std::dynamic_pointer_cast<LExposeEvent>(event);
        handleExposeEvent(evt);
        return true;
    }
    case LEvent::WindowStateChange:{
        LWindow::handleEvent(event); // Update LWindow::Visibility and emit signals.
        std::shared_ptr<LWindowStateChangeEvent> evt =
            std::dynamic_pointer_cast<LWindowStateChangeEvent>(event);
        handleWindowStateChangedEvent(evt);
        return true;
    }
    case LEvent::ThemeChange: {
        std::shared_ptr<LEvent> widgetEvent
            = std::make_shared<LEvent>(LEvent::ThemeChange);
        LCoreApplication::forwardEvent(w, widgetEvent, event);
        return true;
    }
/*    case LEvent::TabletPress:
    case LEvent::TabletMove:
    case LEvent::TabletRelease:{
        std::shared_ptr<LTabletEvent> evt = std::dynamic_pointer_cast<LTabletEvent>(event);
        handleTabletEvent(evt);
        return true;
    }
    case LEvent::NativeGesture:{
        std::shared_ptr<LNativeGestureEvent> evt = std::dynamic_pointer_cast<LNativeGestureEvent>(event);
        handleGestureEvent(evt);
        return true;
    }
    case LEvent::ContextMenu:{
        std::shared_ptr<LContextMenuEvent> evt = std::dynamic_pointer_cast<LContextMenuEvent>(event);
        handleContextMenuEvent(evt);
        return true;
    }*/
    case LEvent::WindowBlocked:{
        m_buttonDown.reset();
        break;
    }
    case LEvent::UpdateRequest:{
        // This is not the same as an UpdateRequest for a LWidget. That just
        // syncs the backing store while here we also must mark as dirty.
        w->repaint();
        return true;
    }
    default:
        break;
    }

    if (shouldBePropagatedToWidget(event) && LCoreApplication::forwardEvent(w, event)){
        return true;
    }
    return LWindow::handleEvent(event);
}

void LWidgetWindow::handleCloseEvent(std::shared_ptr<LEvent> event)
{
    std::shared_ptr<LWidget>  w = m_widget.lock();
    if(w != nullptr){
        bool isClosing = w->closeHelper(LCloseMode::WithSpontaneousEvent);
        event->setAccepted(isClosing);
    }
}

void LWidgetWindow::handleEnterLeaveEvent(std::shared_ptr<LEvent> event)
{
    std::shared_ptr<LWidget> widget = m_widget.lock();
    if(widget == nullptr){
        return;
    }

    if (LApplication::inPopupMode()
        && widget != LApplication::activePopupWidget()
        && !widget->underMouse()){
        return;
    }

    if (event->type() == LEvent::Leave) {
        std::shared_ptr<LWidget> enter = nullptr;
        // Check from window system event queue if the next queued enter targets a window
        // in the same window hierarchy (e.g. enter a child of this window). If so,
        // remove the enter event from queue and handle both in single dispatch.
        std::shared_ptr<wse::LEnterEvent> systemEvent =
            std::dynamic_pointer_cast<wse::LEnterEvent>(LWindowSystemInterface::peekWindowSystemEvent(LWinSysEvent::Enter));
        LPoint<double> globalPosF;
        if (systemEvent != nullptr) {
            globalPosF = systemEvent->m_globalPos;

            std::shared_ptr<LWidgetWindow> enterWindow = std::dynamic_pointer_cast<LWidgetWindow>(systemEvent->m_window);
            if (enterWindow != nullptr)
            {
                std::shared_ptr<LWindow> thisParent = getPtrT<LWindow>();
                std::shared_ptr<LWindow> enterParent = enterWindow;
                std::shared_ptr<LWindow> t= parent();
                while (t != nullptr){
                    thisParent = t;
                    t = thisParent->parent();
                }
                t = enterParent->parent();
                while (t != nullptr){
                    enterParent = t;
                    t = enterParent->parent();
                }

                if (thisParent == enterParent) {
                    LApplication::m_currentMouseWindow = enterWindow;
                    enter = enterWindow->widget();
                    LWindowSystemInterface::removeWindowSystemEvent(systemEvent);
                }
            }
        }else{
            globalPosF = LGuiApplication::m_lastCursorPosition;
        }
        // Enter-leave between sibling widgets is ignored when there is a mousegrabber - this makes
        // both native and non-native widgets work similarly.
        // When mousegrabbing, leaves are only generated if leaving the parent window.
        if (enter == nullptr || LWidget::mouseGrabber() == nullptr) {
            // Preferred leave target is the last mouse receiver, unless it has native window,
            // in which case it is assumed to receive it's own leave event when relevant.
            std::shared_ptr<LWidget> leave;
            std::shared_ptr<LWidget> t = m_lastMouseReceiver.lock();
            if (t != nullptr && !t->internalWinId()){
                leave = t;
            }else{
                leave = widget;
            }
            LApplication::dispatchEnterLeave(enter, leave, globalPosF);
            m_lastMouseReceiver = enter;
        }
    } else {
        std::shared_ptr<LEnterEvent> ee = std::dynamic_pointer_cast<LEnterEvent>(event);
        std::shared_ptr<LWidget> child = widget->childAt(ee->pos());
        std::shared_ptr<LWidget> receiver = child ? child : widget;
        std::shared_ptr<LWidget> leave = m_lastMouseReceiver.lock();
        if(!LApplication::inPopupMode() || receiver != widget || leave != widget){
            leave = nullptr;
        }
        LApplication::dispatchEnterLeave(receiver, leave, ee->screenPos());
        m_lastMouseReceiver = receiver;
    }
}

void LWidgetWindow::handleFocusInEvent(std::shared_ptr<LFocusEvent> e)
{
    std::shared_ptr<LWidget> focusWidget = nullptr;
    if (e->reason() == LFocusReason::Backtab){
        focusWidget = getFocusWidget(LastFocusWidget);
    }
    else if (e->reason() == LFocusReason::Tab){
        focusWidget = getFocusWidget(FirstFocusWidget);
    }

    if (focusWidget != nullptr){
        focusWidget->setFocus();
    }
}

void LWidgetWindow::handleKeyEvent(std::shared_ptr<LKeyEvent> ev)
{
    if (LApplication::guiInst()->modalState() && !LApplication::tryModal(m_widget.lock(), ev->type())){
        return;
    }

    std::shared_ptr<LObject> receiver = LWidget::keyboardGrabber();
    if (receiver == nullptr){
        if(LApplication::inPopupMode()) {
            std::shared_ptr<LWidget> popup = LApplication::activePopupWidget();
            std::shared_ptr<LWidget>  popupFocusWidget = popup->focusWidget();
            receiver = popupFocusWidget ? popupFocusWidget : popup;
        }
    }
    if (receiver == nullptr){
        receiver = focusObject();
    }

    if(receiver != nullptr){
        LCoreApplication::forwardEvent(receiver, ev);
    }
}

void LWidgetWindow::handleMouseEvent(std::shared_ptr<LMouseEvent> event)
{
    std::shared_ptr<LWidget> widgetLock = m_widget.lock();
    if(widgetLock == nullptr){
        event->ignore();
        return;
    }

    static const LEvent::Type contextMenuTrigger = LEvent::MouseButtonRelease;
    if (LApplication::inPopupMode()) {
        std::shared_ptr<LWidget> activePopupWidget = LApplication::activePopupWidget();

        LPoint<int> mapped = event->pos();
        if (activePopupWidget != widgetLock){
            mapped = activePopupWidget->mapFromGlobal(event->globalPos());
        }
        bool releaseAfter = false;
        std::shared_ptr<LWidget> popupChild  = activePopupWidget->childAt(mapped);

        if (activePopupWidget != m_popupDown.lock()) {
            m_buttonDown.reset();
            m_popupDown.reset();
        }

        switch (event->type()) {
        case LEvent::MouseButtonPress:
        case LEvent::MouseButtonDblClick:
            m_buttonDown = popupChild;
            m_popupDown = activePopupWidget;
            m_popupDownClosed = false;
            break;
        case LEvent::MouseButtonRelease:
            releaseAfter = true;
            break;
        default:
            break; // nothing for mouse move
        }

        int oldOpenPopupCount = m_openPopupCount;

        if (activePopupWidget->isEnabled()) {
            // deliver event
            m_replayPopupMouseEvent = false;
            std::shared_ptr<LWidget> receiver = m_buttonDown.lock();
            if(receiver == nullptr){
                if (popupChild != nullptr){
                    receiver = popupChild;
                }else{
                    receiver = activePopupWidget;
                }
            }

            LPoint<int> widgetPos = mapped;
            if (receiver != activePopupWidget){
                widgetPos = receiver->mapFromGlobal(event->globalPos());
            }


            const bool reallyUnderMouse = activePopupWidget->rect().contains(mapped);
            const bool underMouse = activePopupWidget->underMouse();
            if (underMouse != reallyUnderMouse) {
                if (reallyUnderMouse) {
                    const LPoint<int> receiverMapped = receiver->mapFromGlobal(event->screenPos().toPoint());
                    // Prevent negative mouse position on enter event - this event
                    // should be properly handled in "handleEnterLeaveEvent()".
                    if (receiverMapped.m_x >= 0 && receiverMapped.m_y >= 0) {
                        LApplication::dispatchEnterLeave(receiver, nullptr, event->screenPos());
                        m_lastMouseReceiver = receiver;
                    }
                } else {
                    LApplication::dispatchEnterLeave(nullptr, m_lastMouseReceiver.lock(), event->screenPos());
                    m_lastMouseReceiver = receiver;
                    receiver = activePopupWidget;
                }
            }

            if ((event->type() != LEvent::MouseButtonPress)
                || !(event->flags().testFlag(LMouseEventFlag::CreatedDoubleClick))) {
                // if the widget that was pressed is gone, then deliver move events without buttons
                const auto buttons = event->type() == LEvent::MouseMove && m_popupDownClosed
                                         ? LMouseButton::No : event->buttons();
                std::shared_ptr<LMouseEvent> e = std::make_shared<LMouseEvent>(event->type(),
                                                              widgetPos.toPointF(),
                                                              event->windowPos(), event->screenPos(),
                                                              event->button(), buttons,
                                                              event->modifiers(), event->source());
                e->setTimestamp(event->timestamp());
                LApplication::sendMouseEvent(receiver, e,
                                             receiver, receiver->window(),
                                             m_buttonDown, m_lastMouseReceiver);
                m_lastMouseReceiver = receiver;
            }
        } else {
            // close disabled popups when a mouse button is pressed or released
            switch (event->type()) {
            case LEvent::MouseButtonPress:
            case LEvent::MouseButtonDblClick:
            case LEvent::MouseButtonRelease:
                activePopupWidget->close();
                break;
            default:
                break;
            }
        }

        if (LApplication::activePopupWidget() != activePopupWidget
            && m_replayPopupMouseEvent) {
            if (widgetLock->windowType() != LWindowType::Popup){
                m_buttonDown.reset();
            }
            if (event->type() == LEvent::MouseButtonPress) {
                // the popup disappeared, replay the mouse press event
                std::shared_ptr<LWidget> w = LApplication::widgetAt(event->globalPos());
                if (w && !LApplication::isBlockedByModal(w)) {
                    // activate window of the widget under mouse pointer
                    if (!w->isActiveWindow()) {
                        w->activateWindow();
                        w->window()->raise();
                    }

                    if (std::shared_ptr<LWindow> win = w->windowHandle(LWidget::WinHandleMode::Closest)) {
                        const LRect<int> globalGeometry = win->isTopLevel()
                                                         ? win->geometry()
                                                              : LRect<int>(win->mapToGlobal(LPoint<int>(0, 0)), win->size());
                        if (globalGeometry.contains(event->globalPos())) {
                            // Use postEvent() to ensure the local LEventLoop terminates when called from LMenu::exec()
                            const LPoint<double> localPos = win->mapFromGlobal(event->globalPos()).toPointF();
                            std::shared_ptr<LMouseEvent> e =
                                std::make_shared<LMouseEvent>(LEvent::MouseButtonPress,
                                                              localPos, localPos, event->globalPos().toPointF(),
                                                              event->button(), event->buttons(),
                                                              event->modifiers(), event->source());
                            e->setSpontaneous(true);
                            e->setTimestamp(event->timestamp());
                            LCoreApplication::postEvent(win, e);
                        }
                    }
                }
            }
            m_replayPopupMouseEvent = false;
        } else if (event->type() == contextMenuTrigger
                   && event->button() == LMouseButton::Right
                   && (m_openPopupCount == oldOpenPopupCount)) {
            std::shared_ptr<LWidget> receiver = m_buttonDown.lock();
            if (receiver == nullptr){
                if(popupChild != nullptr){
                    receiver = popupChild;
                }else{
                    receiver = activePopupWidget;
                }
            }

            std::shared_ptr<LContextMenuEvent> e
                = std::make_shared<LContextMenuEvent>(LContextMenuEvent::Mouse,
                                                      mapped,
                                                      event->globalPos(),
                                                      event->modifiers());
            LApplication::forwardEvent(receiver, e, event);
        }


        if (releaseAfter) {
            m_buttonDown.reset();
            m_popupDownClosed = false;
            m_popupDown.reset();
        }
        return;
    }

    m_popupDownClosed = false;

    // modal event handling
    if (LApplication::guiInst()->modalState()
        && !LApplication::tryModal(widgetLock, event->type())){
        return;
    }

    // which child should have it?
    std::shared_ptr<LWidget> widget = widgetLock->childAt(event->pos());
    LPoint<int> mapped = event->pos();

    if (widget == nullptr){
        widget = widgetLock;
    }

    const bool initialPress = event->buttons() == event->button();
    if (event->type() == LEvent::MouseButtonPress && initialPress){
        m_buttonDown = widget;
    }

    std::shared_ptr<LWidget> receiver = LApplication::pickMouseReceiver(widgetLock,
                                                                        event->windowPos().toPoint(),
                                                                        &mapped, event->type(),
                                                                        event->buttons(),
                                                                        m_buttonDown.lock(),
                                                                        widget);
    if (receiver == nullptr){
        return;
    }

    if (isPopup() && receiver->window()->windowHandle().get() != this) {
        receiver = widget;
    }

    if ((event->type() != LEvent::MouseButtonPress)
        || !(event->flags().testFlag(LMouseEventFlag::CreatedDoubleClick))) {

        // The preceding statement excludes MouseButtonPress events which caused
        // creation of a MouseButtonDblClick event.
        std::shared_ptr<LMouseEvent> translated
            = std::make_shared<LMouseEvent>(event->type(), mapped.toPointF(), event->windowPos(), event->screenPos(),
                               event->button(), event->buttons(), event->modifiers(), event->source());
        translated->setTimestamp(event->timestamp());
        LApplication::sendMouseEvent(receiver,
                                     translated,
                                     widget,
                                     widgetLock,
                                     m_buttonDown,
                                     m_lastMouseReceiver);
        event->setAccepted(translated->isAccepted());
    }

    if (event->type() == contextMenuTrigger && event->button() == LMouseButton::Right
        && widgetLock->rect().contains(event->pos())) {
        std::shared_ptr<LContextMenuEvent> e
            = std::make_shared<LContextMenuEvent>(LContextMenuEvent::Mouse, mapped,
                                                  event->globalPos(), event->modifiers());
        LGuiApplication::forwardEvent(receiver, e, event);
    }
}

void LWidgetWindow::handleNonClientAreaMouseEvent(std::shared_ptr<LMouseEvent> event)
{
    LApplication::forwardEvent(m_widget.lock(), event);
}

void LWidgetWindow::handleMoveEvent(std::shared_ptr<LMoveEvent> event)
{
    std::shared_ptr<LWidget> widgetLock = m_widget.lock();
    if(widgetLock == nullptr){
        event->ignore();
        return;
    }

    if (widgetLock->testAttribute(LWidgetAttribute::OutsideWSRange)){
        return;
    }

    auto oldPosition = widgetLock->m_widgetData.crect.topLeft();
    auto newPosition = geometry().topLeft();

    if (!widgetLock->isWindow()) {
        std::shared_ptr<LWidget> nativeParent = widgetLock->nativeParentWidget();
        if (nativeParent != nullptr){
            std::shared_ptr<LWidget> parent = widgetLock->parentT<LWidget>();
            newPosition = parent->mapFrom(nativeParent, newPosition);
        }
    }

    bool changed = newPosition != oldPosition;

    if (changed){
        widgetLock->m_widgetData.crect.moveTopLeft(newPosition);
    }

    updateMargins();
    if (changed) {
        std::shared_ptr<LMoveEvent> widgetEvent
            = std::make_shared<LMoveEvent>(newPosition, oldPosition);
        LGuiApplication::forwardEvent(widgetLock, widgetEvent, event);
    }
}

void LWidgetWindow::handleResizeEvent(std::shared_ptr<LResizeEvent> ev)
{
    std::shared_ptr<LWidget> w = m_widget.lock();
    if (w != nullptr){

        auto oldRect = w->rect();
        if (updateSize()) {
            LGuiApplication::forwardEvent(w, ev);

            if (w->shouldPaintOnScreen()) {
                std::shared_ptr<LRegion> dirtyRegion = std::make_shared<LRegion>(w->rect());
                if (w->testAttribute(LWidgetAttribute::StaticContents)){
                    dirtyRegion->subtract(oldRect);
                }
                w->syncBackingStore(dirtyRegion);
            } else {
                w->syncBackingStore();
            }
        }
    }
}

void LWidgetWindow::handleWheelEvent(std::shared_ptr<LWheelEvent> ev)
{
    if (LApplication::guiInst()->modalState() && !LApplication::tryModal(m_widget.lock(), ev->type())){
        return;
    }

    std::shared_ptr<LWidget> w = m_widget.lock();
    if (w != nullptr){
        LPoint<int> pos = ev->position().toPoint();

        // Use proper popup window for wheel event. Sends the wheel
        // event to the root menu, so redirect it to the proper popup window.
        std::shared_ptr<LWidget>  activePopupWidget = LApplication::activePopupWidget();
        if (activePopupWidget != nullptr && activePopupWidget != w) {
            w = activePopupWidget;
            pos = w->mapFromGlobal(ev->globalPosition().toPoint());
        }

        // which child should have it?
        std::shared_ptr<LWidget> widget = w->childAt(pos);
        if (widget == nullptr){
            widget = w;
        }else{
            pos = widget->mapFrom(w, pos);
        }

        std::shared_ptr<LWheelEvent> translated=
            std::make_shared<LWheelEvent>(pos.toPointF(), ev->globalPosition(),
                                         ev->pixelDelta(), ev->angleDelta(),
                                         ev->buttons(), ev->modifiers(), ev->phase(), ev->inverted(), ev->source());
        translated->setTimestamp(ev->timestamp());
        LGuiApplication::forwardEvent(widget, translated, ev);
    }
}

void LWidgetWindow::handleExposeEvent(std::shared_ptr<LExposeEvent> event)
{
    std::shared_ptr<LWidget> w = m_widget.lock();
    if(w != nullptr){
        const bool exposed = isExposed();

        if (w->m_childrenHiddenByWState) {
            // If widgets has been previously hidden by window state change event
            // and they aren't yet shown...
            if (exposed) {
                // If the window becomes exposed...
                if (!w->m_childrenShownByExpose) {
                    // ... and they haven't been shown by this function yet - show it.
                    w->showChildren(true);
                    std::shared_ptr<LEvent> showEvent = std::make_shared<LEvent>(LEvent::Show);
                    LCoreApplication::forwardEvent(w, showEvent, event);
                    w->m_childrenShownByExpose = true;
                }
            } else {
                // If the window becomes not exposed...
                if (w->m_childrenShownByExpose) {
                    w->hideChildren(true);
                    std::shared_ptr<LEvent> hideEvent = std::make_shared<LEvent>(LEvent::Hide);
                    LCoreApplication::forwardEvent(w, hideEvent, event);
                    w->m_childrenShownByExpose = false;
                }
            }
        }

        if (exposed) {
            w->setAttribute(LWidgetAttribute::Mapped);
            std::shared_ptr<LWidget> p= w->parentT<LWidget>();
            for (; p != nullptr && !p->testAttribute(LWidgetAttribute::Mapped); p = p->parentT<LWidget>()){
                p->setAttribute(LWidgetAttribute::Mapped);
            }

            std::shared_ptr<LRegion> r= event->region();
            if (r!= nullptr ){
                if(!r->isEmpty()){
                    w->syncBackingStore(r);
                }
            }
        } else {
            w->setAttribute(LWidgetAttribute::Mapped, false);
        }
    }
}

void LWidgetWindow::handleWindowStateChangedEvent(std::shared_ptr<LWindowStateChangeEvent> event)
{
    std::shared_ptr<LWidget> w = m_widget.lock();
    if(w != nullptr){
        // LWindow does currently not know 'active'.
        LFlags<LWindowState> eventState = event->oldState();
        LFlags<LWindowState> widgetState = w->windowState();
        LFlags<LWindowState> windowState = windowStates();
        if (widgetState & LWindowState::Active){
            eventState |= LWindowState::Active;
        }

        // Determine the new widget state, remember maximized/full screen
        // during minimized.
        if (windowState & LWindowState::Minimized) {
            widgetState |= LWindowState::Minimized;
        } else {
            widgetState = windowState | (widgetState & LWindowState::Active);
        }

        // Sent event if the state changed (that is, it is not triggered by
        // LWidget::setWindowState(), which also sends an event to the widget).
        if (uint32_t(widgetState) != w->m_widgetData.windowState) {
            w->m_widgetData.windowState = uint32_t(widgetState);
            std::shared_ptr<LWindowStateChangeEvent> widgetEvent =
                std::make_shared<LWindowStateChangeEvent>(eventState);
            LGuiApplication::forwardEvent(w, widgetEvent, event);
        }
    }
}

bool LWidgetWindow::updateSize()
{
    bool changed = false;
    std::shared_ptr<LWidget> w = m_widget.lock();
    if(w != nullptr){
        if (w->testAttribute(LWidgetAttribute::OutsideWSRange)){
            return changed;
        }
        if (w->m_widgetData.crect.size() != geometry().size()) {
            changed = true;
            w->m_widgetData.crect.setSize(geometry().size());
        }
        updateMargins();
    }
    return changed;
}

void LWidgetWindow::updateMargins()
{
    std::shared_ptr<LWidget> w = m_widget.lock();
    if(w != nullptr){
        const LMargins<int> margins = frameMargins();
        LTLWExtra *te = w->topData();
        te->posIncludesFrame= false;
        te->frameStrut = margins;
        w->m_widgetData.fstrutDirty = false;
    }
}

std::shared_ptr<LWidget> LWidgetWindow::getFocusWidget(FocusWidgets fw)
{
    std::shared_ptr<LWidget> tlw = m_widget.lock();
    std::shared_ptr<LWidget> last = tlw;

    std::shared_ptr<LWidget> w = tlw->nextInFocusChain();
    LFocusPolicy focusFlag = LGuiApplication::tabAllWidgets() ? LFocusPolicy::Tab : LFocusPolicy::Strong;

    while (w != tlw)
    {
        if ((((uint8_t)w->focusPolicy() & (uint8_t)focusFlag) == (uint8_t)focusFlag)
            && w->isVisibleTo(tlw) && w->isEnabled())
        {
            last = w;
            if (fw == FirstFocusWidget){
                break;
            }
        }
        w = w->nextInFocusChain();
    }
    return last;
}

LK_NAMESPACE_END
