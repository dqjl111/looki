#include "lapplication.h"
#include "lkcore/ldebug.h"
#include "lkgui/lguienums.h"
#include "lkgui/lwindowsysteminterface.h"
#include "lkgui/linputmethod.h"
#include "lkgui/lplatformwindow.h"
#include "lkgui/lrect.h"
#include "lkwidgets/util/leditdoc.h"
#include "lwidgetwindow.h"

LK_NAMESPACE_BEGIN


std::list<std::shared_ptr<LWidget> > LApplication::m_popupWidgets;
std::weak_ptr<LWidget> LApplication::m_focusWidget;
std::weak_ptr<LWidget> LApplication::m_hiddenFocusWidget;
std::shared_ptr<LDesktopWidget> LApplication::m_desktop = nullptr;
std::shared_ptr<LWidget> LApplication::m_activeWindow = nullptr;
std::weak_ptr<LWidget> LApplication::m_leaveAfterRelease;
bool LApplication::m_inTabKeyEvent = false;
std::shared_ptr<LStyle> LApplication::m_style;
std::shared_ptr<LTimer> LApplication::m_blinkTimer = nullptr;

std::unordered_map<WId, std::weak_ptr<LWidget> > LApplication::m_mapper;
std::list<std::weak_ptr<LWidget> > LApplication::m_allWidgets;
bool LApplication::m_popupGrabOk = false;
bool LApplication::m_autoSipEnabled = false;


LApplication::LApplication(int &aargc,  char **aargv)
: LGuiApplication(aargc, aargv)
{

}

LApplication::~LApplication()
{
    m_desktop = nullptr;
    m_activeWindow = nullptr;
}


void LApplication::closeAllPopups()
{
    // Close all popups: In case some popup refuses to close,
    // we give up after 1024 attempts (to avoid an infinite loop).
    int maxiter = 1024;
    std::shared_ptr<LWidget> popup;
    while ((popup = LApplication::activePopupWidget()) && maxiter--){
        popup->close();
    }
}


void LApplication::ungrabKeyboardForPopup(std::shared_ptr<LWidget> popup)
{
    std::shared_ptr<LWidget> keyboardGrabber = LWidget::keyboardGrabber();
    if (keyboardGrabber != nullptr){
        keyboardGrabber->stealKeyboardGrab(true);
    }
    else{
        popup->stealKeyboardGrab(false);
    }
}

void LApplication::ungrabMouseForPopup(std::shared_ptr<LWidget> popup)
{
    std::shared_ptr<LWidget> mouseGrabber = LWidget::mouseGrabber();
    if (mouseGrabber != nullptr){
        mouseGrabber->stealMouseGrab(true);
    }else{
        popup->stealMouseGrab(false);
    }
}

void LApplication::grabForPopup(std::shared_ptr<LWidget>  popup)
{
    m_popupGrabOk = popup->stealKeyboardGrab(true);
    if (m_popupGrabOk) {
        m_popupGrabOk = popup->stealMouseGrab(true);
        if (!m_popupGrabOk) {
            // transfer grab back to the keyboard grabber if any
            ungrabKeyboardForPopup(popup);
        }
    }
}

bool LApplication::shouldSetFocus(std::shared_ptr<LWidget> w, LFocusPolicy policy)
{
    if (((uint8_t)w->focusPolicy() & (uint8_t)policy) != (uint8_t)policy){
        return false;
    }

    std::shared_ptr<LWidget> f = w;
    while (f->m_pExtraData!= nullptr){
        std::shared_ptr<LWidget> p = f->m_pExtraData->focusProxy.lock();
        if(p == nullptr){
            break;
        }
        f = p;
    }

    if (w != f && ((uint8_t)f->focusPolicy() & (uint8_t)policy) != (uint8_t)policy){
        return false;
    }
    return true;
}


std::shared_ptr<LWidget> LApplication::activePopupWidget()
{
    std::list<std::shared_ptr<LWidget> >::reverse_iterator iter = LApplication::m_popupWidgets.rbegin();
    if(iter != LApplication::m_popupWidgets.rend()){
        return *iter;
    }
    return nullptr;
}

std::shared_ptr<LWidget> LApplication::activeModalWidget()
{
    std::shared_ptr<LWidgetWindow> widgetWindow = std::dynamic_pointer_cast<LWidgetWindow>(modalWindow());
    if(widgetWindow != nullptr){
        return widgetWindow->widget();
    }
    return nullptr;
}

void LApplication::setFocusWidget(std::shared_ptr<LWidget> focus, LFocusReason reason)
{
    m_hiddenFocusWidget.reset();

    std::shared_ptr<LWidget> prev = m_focusWidget.lock();
    if (focus != prev) {
        if (focus && focus->isHidden()) {
            m_hiddenFocusWidget = focus;
            return;
        }

        if (focus && (reason == LFocusReason::Backtab || reason == LFocusReason::Tab)
            && LApplication::m_inTabKeyEvent)
            focus->window()->setAttribute(LWidgetAttribute::KeyboardFocusChange);
        else if (focus && reason == LFocusReason::Shortcut) {
            focus->window()->setAttribute(LWidgetAttribute::KeyboardFocusChange);
        }
        m_focusWidget = focus;

        if(focus != nullptr){
            focus->setFocusSys();
        }

        if (reason != LFocusReason::No) {
            //send events
            if (prev != nullptr ) {
                std::shared_ptr<LFocusEvent> out = std::make_shared<LFocusEvent>(LEvent::FocusOut, reason);
                LCoreApplication::sendEvent(prev, out);
            }
            if(focus != nullptr) {
                std::shared_ptr<LFocusEvent> in = std::make_shared<LFocusEvent>(LEvent::FocusIn, reason);
                LCoreApplication::sendEvent(focus, in);
            }

            wInst()->sigFocusChanged.emt(prev, focus);
        }
    }
}

std::shared_ptr<LDesktopWidget> LApplication::desktop()
{
    if((m_desktop == nullptr) || (m_desktop->windowType() != LWindowType::Desktop)){
        m_desktop = std::make_shared<LDesktopWidget>();
    }
    return m_desktop;
}


void LApplication::closePopup(std::shared_ptr<LWidget> popup)
{
    if(m_popupWidgets.empty()){
        return;
    }

    m_popupWidgets.remove(popup);
    if(popup == LWidgetWindow::m_popupDown.lock()){
        LWidgetWindow::m_buttonDown.reset();
        LWidgetWindow::m_popupDownClosed = true;
        LWidgetWindow::m_popupDown.reset();
    }


    if (LApplication::m_popupWidgets.size() == 0) { // this was the last popup
        if (m_popupGrabOk) {
            m_popupGrabOk = false;

            if (popup->geometry().contains(LPoint<int>(LGuiApplication::m_mousePressX, LGuiApplication::m_mousePressY))
                || popup->testAttribute(LWidgetAttribute::NoMouseReplay)) {
                // mouse release event or inside
                LWidgetWindow::m_replayPopupMouseEvent = false;
            } else { // mouse press event
                LWidgetWindow::m_replayPopupMouseEvent = true;
            }

            // transfer grab back to mouse grabber if any, otherwise release the grab
            ungrabMouseForPopup(popup);
            // transfer grab back to keyboard grabber if any, otherwise release the grab
            ungrabKeyboardForPopup(popup);
        }

        if (m_activeWindow != nullptr) {
            if (std::shared_ptr<LWidget> fw = m_activeWindow->focusWidget()) {
                if (fw != LApplication::focusWidget()) {
                    fw->setFocus(LFocusReason::Popup);
                } else {
                    std::shared_ptr<LFocusEvent> e
                        = std::make_shared<LFocusEvent>(LEvent::FocusIn, LFocusReason::Popup);
                    LCoreApplication::sendEvent(fw, e);
                }
            }
        }

    } else {
        // A popup was closed, so the previous popup gets the focus.
        auto rIter = LApplication::m_popupWidgets.rbegin();
        if(rIter != LApplication::m_popupWidgets.rend()){
            std::shared_ptr<LWidget> aw = *rIter;
            std::shared_ptr<LWidget> fw = aw->focusWidget();
            if(fw != nullptr){
                fw->setFocus(LFocusReason::Popup);
            }

            // can become nullptr due to setFocus() above
            if (LApplication::m_popupWidgets.size() == 1){ // grab mouse/keyboard
                grabForPopup(aw);
            }
        }
    }

}

void LApplication::openPopup(std::shared_ptr<LWidget> popup)
{
    if(popup != nullptr){
        m_popupWidgets.push_back(popup); // add to end of list

        if (m_popupWidgets.size() == 1){ // grab mouse/keyboard
            grabForPopup(popup);
        }

        // popups are not focus-handled by the window system (the first
        // popup grabbed the keyboard), so we have to do that manually: A
        // new popup gets the focus
        std::shared_ptr<LWidget> popFocus = popup->focusWidget();
        if (popFocus != nullptr) {
            popFocus->setFocus(LFocusReason::Popup);
        } else if (m_popupWidgets.size() == 1) { // this was the first popup
            std::shared_ptr<LWidget> focus = m_focusWidget.lock();
            if (focus != nullptr) {
                std::shared_ptr<LFocusEvent> e
                    = std::make_shared<LFocusEvent>(LEvent::FocusOut, LFocusReason::Popup);
                LCoreApplication::sendEvent(focus, e);
            }
        }
    }
}

std::shared_ptr<LWidget> LApplication::activeWindow()
{
    return LApplication::m_activeWindow;
}

void LApplication::setActiveWindow(std::shared_ptr<LWidget> act)
{
    std::shared_ptr<LWidget> window = nullptr;
    if(act){
        window = act->window();
    }

    if (LApplication::m_activeWindow == window){
        return;
    }

    std::list<std::shared_ptr<LWidget> > toBeActivated;
    std::list<std::shared_ptr<LWidget> > toBeDeactivated;

    if (LApplication::m_activeWindow != nullptr) {
        toBeDeactivated.push_back(LApplication::m_activeWindow);
    }

    std::shared_ptr<LWidget> focus = LApplication::m_focusWidget.lock();
    if (focus != nullptr) {
        if (focus->testAttribute(LWidgetAttribute::InputMethodEnabled)){
            LGuiApplication::inputMethod()->commit();
        }

        std::shared_ptr<LFocusEvent> focusAboutToChange
            = std::make_shared<LFocusEvent>(LEvent::FocusAboutToChange, LFocusReason::ActiveWindow);
        LCoreApplication::sendEvent(focus, focusAboutToChange);
    }

    LApplication::m_activeWindow = window;
    if (LApplication::m_activeWindow != nullptr) {
        toBeActivated.push_back(LApplication::m_activeWindow);
    }

    // first the activation/deactivation events
    std::shared_ptr<LEvent> activationChange
        = std::make_shared<LEvent>(LEvent::ActivationChange);
    std::shared_ptr<LEvent> windowActivate
        = std::make_shared<LEvent>(LEvent::WindowActivate);
    std::shared_ptr<LEvent> windowDeactivate
        = std::make_shared<LEvent>(LEvent::WindowDeactivate);

    for (const std::shared_ptr<LWidget>& w : toBeActivated) {
        sendSpontaneousEvent(w, windowActivate);
        sendSpontaneousEvent(w, activationChange);
    }

    for(const std::shared_ptr<LWidget>& w : toBeDeactivated) {
        sendSpontaneousEvent(w, windowDeactivate);
        sendSpontaneousEvent(w, activationChange);
    }

    if (!LApplication::m_popupWidgets.empty()) { // !inPopupMode()
        // then focus events
        if ((LApplication::m_activeWindow != nullptr)
            && (focus != nullptr)) {
            LApplication::setFocusWidget(nullptr, LFocusReason::ActiveWindow);
        } else if (LApplication::m_activeWindow != nullptr) {
            std::shared_ptr<LWidget> w = LApplication::m_activeWindow->focusWidget();
            if ((w!=nullptr) && w->isVisible()){
                w->setFocus(LFocusReason::ActiveWindow);
            }
            else {
                w = LApplication::focusNextPrevChildHelper(LApplication::m_activeWindow, true);
                if (w!= nullptr) {
                    w->setFocus(LFocusReason::ActiveWindow);
                } else {
                    // If the focus widget is not in the activate_window, clear the focus
                    w = focus;
                    if ((w!=nullptr)&& LApplication::m_activeWindow->focusPolicy() != LFocusPolicy::No){
                        LApplication::setFocusWidget(LApplication::m_activeWindow, LFocusReason::ActiveWindow);
                    }else if (!LApplication::m_activeWindow->isAncestorOf(w)){
                        LApplication::setFocusWidget(nullptr, LFocusReason::ActiveWindow);
                    }
                }
            }
        }
    }

}

std::shared_ptr<LWidget> LApplication::topLevelWidgetForWindow(std::shared_ptr<LWindow> wnd)
{
    while (wnd && !wnd->isTopLevel()) {
        std::shared_ptr<LWindow> parent = wnd->parent();
        if (parent == nullptr){
            break;
        }

        std::shared_ptr<LPlatformWindow> h = parent->handle();
        if(h!= nullptr){
            if(h->isForeignWindow()){
                break;
            }
        }
        wnd = wnd->parent();
    }
    if (wnd) {
        const auto tlws = LApplication::topLevelWidgets();
        for (std::shared_ptr<LWidget> tlw : tlws) {
            if (tlw->windowHandle() == wnd){
                return tlw;
            }
        }
    }
    return nullptr;
}

std::list<std::shared_ptr<LWidget> > LApplication::topLevelWidgets()
{
    std::list<std::shared_ptr<LWidget> > list;
    for(const std::weak_ptr<LWidget>& ww: m_allWidgets){
        std::shared_ptr<LWidget> w = ww.lock();
        if(w->isWindow() && w->windowType() != LWindowType::Desktop){
            list.push_back(w);
        }
    }
    return list;
}

std::shared_ptr<LWidget> LApplication::topLevelWidgdetAt(const LPoint<int> &p)
{
    if (const std::shared_ptr<LWindow> window = LGuiApplication::topLevelAt(p)) {
        const std::shared_ptr<LWidgetWindow> widgetWindow
            = std::dynamic_pointer_cast<LWidgetWindow>(window);
        if (widgetWindow != nullptr){
            return widgetWindow->widget();
        }
    }
    return nullptr;
}

std::shared_ptr<LStyle> LApplication::style()
{
    if(m_style == nullptr){
        //Todo create
        m_style = std::make_shared<LStyle>();
    }
    return m_style;
}

std::shared_ptr<LWidget> LApplication::widgetAt(const LPoint<int> &p)
{
    std::shared_ptr<LWidget> window = LApplication::topLevelWidgdetAt(p);
    if (window == nullptr){
        return nullptr;
    }

    if (window->testAttribute(LWidgetAttribute::TransparentForMouseEvents)){
        std::shared_ptr<LRegion> oldmask = window->mask();
        LPoint<int> wpoint = window->mapFromGlobal(p);
        bool oldMaskEmpty = (oldmask == nullptr || oldmask->isEmpty());
        std::shared_ptr<LRegion> newmask;
        if(oldMaskEmpty){
            newmask = std::make_shared<LRegion>(window->rect());
        }else{
            newmask = oldmask->subtracted(LRect<int>(wpoint.m_x, wpoint.m_y, 1, 1, false));
        }
        window->setMask(newmask);
        std::shared_ptr<LWidget> recurse = nullptr;
        if (LApplication::topLevelWidgdetAt(p) != window){ // verify recursion will terminate
            recurse = widgetAt(p);
        }
        if (oldMaskEmpty){
            window->clearMask();
        }else{
            window->setMask(oldmask);
        }
        return recurse;
    }else{
        std::shared_ptr<LWidget> child = nullptr;
        LPoint<int> wpoint = window->mapFromGlobal(p);
        child = window->childAt(wpoint);
        if (child != nullptr){
            return child;
        }
        return window;
    }
}

std::shared_ptr<LTimer> LApplication::blinkTimer()
{
    if(m_blinkTimer == nullptr){
        m_blinkTimer = std::make_shared<LTimer>();
        m_blinkTimer->start(std::chrono::milliseconds(m_themeHints.cursorFlashTime()), true);
    }
    return m_blinkTimer;
}

void LApplication::dispatchEnterLeave(std::shared_ptr<LWidget> enter,
                                      std::shared_ptr<LWidget> leave,
                                      const LPoint<double> &globalPosF)
{
    if(enter != nullptr){
        if (enter->testAttribute(LWidgetAttribute::CanEnterSubItem)) {
            if(!enter->isWindow()){
                const LPoint<int> globalPos = std::isinf(globalPosF.m_x)
                                                  ? LPoint<int>(LWIDGETSIZE_MAX, LWIDGETSIZE_MAX)
                                                  : globalPosF.toPoint();
                const LPoint<int> loclPos = enter->mapFromGlobal(globalPos);
                if(enter->enterSubItem(loclPos)){
                    if (enter->testAttribute(LWidgetAttribute::SetCursor) && (enter == leave)) {
                        enter->setCursorSys(true);
                    }
                }
            }
        }
    }

    if(enter == leave){
        return;
    }

    std::list<std::shared_ptr<LWidget> > leaveList;
    std::list<std::shared_ptr<LWidget> >  enterList;

    bool sameWindow = (leave != nullptr) &&  (enter != nullptr) && leave->window() == enter->window();
    if ((leave != nullptr) && !sameWindow) {
        std::shared_ptr<LWidget> w = leave;
        do {
            leaveList.push_back(w);
        } while (!w->isWindow() && (w = w->parentT<LWidget>()));
    }
    if (enter != nullptr && !sameWindow) {
        std::shared_ptr<LWidget> w = enter;
        do {
            enterList.push_back(w);
        } while (!w->isWindow() && (w = w->parentT<LWidget>()));
    }
    if (sameWindow) {
        int enterDepth = 0;
        int leaveDepth = 0;
        std::shared_ptr<LWidget> e = enter;
        while (!e->isWindow() && (e = e->parentT<LWidget>())){
            enterDepth++;
        }
        std::shared_ptr<LWidget> l = leave;
        while (!l->isWindow() && (l = l->parentT<LWidget>())){
            leaveDepth++;
        }
        std::shared_ptr<LWidget> wenter = enter;
        std::shared_ptr<LWidget> wleave = leave;
        while (enterDepth > leaveDepth) {
            wenter = wenter->parentT<LWidget>();
            enterDepth--;
        }
        while (leaveDepth > enterDepth) {
            wleave = wleave->parentT<LWidget>();
            leaveDepth--;
        }
        while (!wenter->isWindow() && wenter != wleave) {
            wenter = wenter->parentT<LWidget>();
            wleave = wleave->parentT<LWidget>();
        }

        for (std::shared_ptr<LWidget> w = leave; w != wleave; w = w->parentT<LWidget>()){
            leaveList.push_back(w);
        }

        for (std::shared_ptr<LWidget> w = enter; w != wenter; w = w->parentT<LWidget>()){
            enterList.push_back(w);
        }
    }

    std::shared_ptr<LEvent> leaveEvent = std::make_shared<LEvent>(LEvent::Leave);
    for (const std::shared_ptr<LWidget>& w: leaveList) {
        if ((LApplication::activeModalWidget() == nullptr) || LApplication::tryModalHelper(w) != nullptr) {
            LCoreApplication::sendEvent(w, leaveEvent);
        }
    }
    if (!enterList.empty()) {
        // Guard against QGuiApplicationPrivate::lastCursorPosition initialized to qInf(), qInf().
        const LPoint<int> globalPos = std::isinf(globalPosF.m_x)
                                          ? LPoint<int>(LWIDGETSIZE_MAX, LWIDGETSIZE_MAX)
                                          : globalPosF.toPoint();

        const LPoint<int> windowPos = enterList.back()->window()->mapFromGlobal(globalPos);
        for (auto it = enterList.crbegin(), end = enterList.crend(); it != end; ++it) {
            std::shared_ptr<LWidget> w = *it;
            if  ((LApplication::activeModalWidget() == nullptr) || LApplication::tryModalHelper(w) != nullptr) {
                const LPoint<int> localPos = w->mapFromGlobal(globalPos);
                std::shared_ptr<LEnterEvent> enterEvent
                    = std::make_shared<LEnterEvent>(localPos.toPointF(),
                                                    windowPos.toPointF(),
                                                    globalPosF);
                LCoreApplication::sendEvent(w, enterEvent);
            }
        }
    }

    // Update cursor for alien/graphics widgets.

    const bool enterOnAlien = ((enter!=nullptr) &&
                               (!enter->isWindow() || enter->testAttribute(LWidgetAttribute::DontShowOnScreen)));
    // Whenever we leave an alien widget, we need to reset its parentT<LWidget>()'s cursor.
    // This is not required on Windows as the cursor is reset on every single mouse move.
    std::shared_ptr<LWidget> parentOfLeavingCursor = nullptr;
    for (const std::shared_ptr<LWidget>& w: leaveList) {
        if (enter== nullptr || enter->isWindow()){
            break;
        }
        if (w->testAttribute(LWidgetAttribute::SetCursor)) {
            parentOfLeavingCursor = w->parentT<LWidget>();
        }
    }
    //check that we will not call enforce_cursor twice with the same native widget
    if (parentOfLeavingCursor && (!enterOnAlien
                                  || parentOfLeavingCursor->effectiveWinId() != enter->effectiveWinId())) {
        {
            if (enter == LApplication::desktop()) {
                enter->setCursorSys(true);
            } else {
                parentOfLeavingCursor->setCursorSys(true);
            }
        }
    }
    if (enterOnAlien) {
        std::shared_ptr<LWidget> cursorWidget = enter;
        while (!cursorWidget->isWindow() && !cursorWidget->isEnabled()){
            cursorWidget = cursorWidget->parentT<LWidget>();
        }

        if (cursorWidget != nullptr){
            cursorWidget->setCursorSys(true);
        }
    }
}

bool LApplication::sendMouseEvent(std::shared_ptr<LWidget> receiver, std::shared_ptr<LMouseEvent> event,
                                  std::shared_ptr<LWidget> alienWidget, std::shared_ptr<LWidget> native,
                                  std::weak_ptr<LWidget> &buttonDown, std::weak_ptr<LWidget> &lastMouseReceiver,
                                  bool spontaneous, bool onlyDispatchEnterLeave)
{

    if(alienWidget != nullptr){
        if(!alienWidget->isWindow()){
            alienWidget = nullptr;
        }
    }

    std::shared_ptr<LWidget> activePopupWidget = LApplication::activePopupWidget();

    bool graphicsWidget = false;
    if(native != nullptr){
        graphicsWidget = native->testAttribute(LWidgetAttribute::DontShowOnScreen);
    }

    bool widgetUnderMouse = receiver->rectF().contains(event->localPos());

    // Clear the obsolete leaveAfterRelease value, if mouse button has been released but
    // leaveAfterRelease has not been updated.
    // This happens e.g. when modal dialog or popup is shown as a response to button click.

    std::shared_ptr<LWidget> buttonDownLock = buttonDown.lock();
    std::shared_ptr<LWidget> lastMouseReceiverLock = lastMouseReceiver.lock();
    std::shared_ptr<LWidget> mouseGrab = LWidget::mouseGrabber();

    if ((!m_leaveAfterRelease.expired()) && (buttonDownLock == nullptr) && !event->buttons()){
        m_leaveAfterRelease.reset();
    }


    if (buttonDownLock != nullptr) {
        if (!graphicsWidget) {
            // Register the widget that shall receive a leave event
            // after the last button is released.
            if ((alienWidget != nullptr || !receiver->internalWinId())
                && (m_leaveAfterRelease.expired())
                && (mouseGrab == nullptr)){
                m_leaveAfterRelease = buttonDownLock;
            }
            if (event->type() == LEvent::MouseButtonRelease && (event->buttons() == event->button())){
                buttonDownLock = nullptr;
                buttonDown.reset();
            }
        }
    } else if ((lastMouseReceiverLock != nullptr) && widgetUnderMouse) {
        if (activePopupWidget) {
            if (mouseGrab == nullptr){
                dispatchEnterLeave(alienWidget != nullptr? alienWidget : native, lastMouseReceiverLock, event->screenPos());
            }
        }else{
            dispatchEnterLeave(receiver, lastMouseReceiverLock, event->screenPos());
        }
    }

    bool result = true;
    if (!onlyDispatchEnterLeave) {
        if (spontaneous){
            result = LApplication::sendSpontaneousEvent(receiver, event);
        }
        else{
            result = LCoreApplication::sendEvent(receiver, event);
        }
    }

    std::shared_ptr<LWidget> leaveAfterRelease = m_leaveAfterRelease.lock();
    if (!graphicsWidget && (leaveAfterRelease!=nullptr) && event->type() == LEvent::MouseButtonRelease
        && (event->buttons() == event->button()) && mouseGrab != leaveAfterRelease) {
        // Dispatch enter/leave if:
        // 1) the mouse grabber is an alien widget
        // 2) the button is released on an alien widget
        std::shared_ptr<LWidget> enter = nullptr;
        if (native != nullptr){
            enter = (alienWidget != nullptr) ? alienWidget : native;
        }
        else{ // The receiver is typically deleted on mouse release with drag'n'drop.
            enter = LApplication::widgetAt(event->globalPos());
        }
        dispatchEnterLeave(enter, leaveAfterRelease, event->screenPos());
        m_leaveAfterRelease.reset();
        lastMouseReceiver = enter;
    }
    else if (leaveAfterRelease == nullptr) {
        if (activePopupWidget) {
            if (mouseGrab == nullptr){
                lastMouseReceiver = alienWidget != nullptr ? alienWidget : (native != nullptr ? native : nullptr);
            }
        } else {
            lastMouseReceiver = receiver != nullptr ? receiver : LApplication::widgetAt(event->globalPos());
        }
    }
    return result;
}

bool LApplication::isBlockedByModal(std::shared_ptr<LWidget> widget)
{
    widget = widget->window();
    std::shared_ptr<LWindow> window = widget->windowHandle();
    if(window != nullptr){
        return guiInst()->windowBlocked(window) != nullptr;
    }
    return false;
}

bool LApplication::tryModal(std::shared_ptr<LWidget> widget, LEvent::Type type)
{
    std::shared_ptr<LWidget> top = tryModalHelper(widget);
    if(top == nullptr){
        return true;
    }

    switch (type) {
    case LEvent::MouseButtonPress:                        // disallow mouse/key events
    case LEvent::MouseButtonRelease:
    case LEvent::MouseMove:
    case LEvent::KeyPress:
    case LEvent::KeyRelease:{
        if (top->parentT<LWidget>() == nullptr){
            top->raise();
        }
        return false;
    }
    default:{
        return true;
    }
    }
}

std::shared_ptr<LWidget> LApplication::tryModalHelper(std::shared_ptr<LWidget> widget)
{
    // the active popup widget always gets the input event
    if (LApplication::activePopupWidget() != nullptr){
        return nullptr;
    }

    if(isBlockedByModal(widget->window())){
        return LApplication::activeModalWidget();
    }else{
        return nullptr;
    }
}

std::shared_ptr<LWidget> LApplication::pickMouseReceiver(std::shared_ptr<LWidget> candidate,
                                                         const LPoint<int> &windowPos, LPoint<int> *pos,
                                                         LEvent::Type type, LFlags<LMouseButton> buttons,
                                                         std::shared_ptr<LWidget> buttonDown,
                                                         std::shared_ptr<LWidget> alienWidget)
{
    std::shared_ptr<LWidget> mouseGrabber = LWidget::mouseGrabber();
    if (((type == LEvent::MouseMove && buttons) || (type == LEvent::MouseButtonRelease))
        && (buttonDown == nullptr)&& (mouseGrabber == nullptr)) {
        return nullptr;
    }


    if (alienWidget && alienWidget->internalWinId()){
        alienWidget = nullptr;
    }

    std::shared_ptr<LWidget> receiver = candidate;

    if (mouseGrabber == nullptr){
        mouseGrabber = ((buttonDown!=nullptr) && !isBlockedByModal(buttonDown)) ? buttonDown : alienWidget;
    }

    if (mouseGrabber && mouseGrabber != candidate) {
        receiver = mouseGrabber;
        if(pos != nullptr){
            *pos = receiver->mapFromGlobal(candidate->mapToGlobal(windowPos));
        }
    }

    return receiver;
}


void LApplication::giveFocusAccordingToFocusPolicy(std::shared_ptr<LWidget> widget,
                                                   std::shared_ptr<LEvent> event,
                                                   LPoint<int> localPos)
{
    LFocusPolicy focusPolicy = LFocusPolicy::Click;
    static std::weak_ptr<LWidget> focusedWidgetOnTouchBegin;

    switch (event->type()) {
    case LEvent::MouseButtonPress:
    case LEvent::MouseButtonDblClick:
    case LEvent::TouchBegin:{
        focusedWidgetOnTouchBegin = LApplication::focusWidget();
        break;
    }
    case LEvent::Wheel:{
        focusPolicy = LFocusPolicy::Wheel;
        break;
    }
    default:{
        return;
    }
    }

    std::shared_ptr<LWidget> focusWidget = widget;
    while (focusWidget != nullptr) {
        if (focusWidget->isEnabled()
            && focusWidget->rect().contains(localPos)
            && LApplication::shouldSetFocus(focusWidget, focusPolicy)) {
            focusWidget->setFocus(LFocusReason::Mouse);
            break;
        }
        if (focusWidget->isWindow()){
            break;
        }

        // find out whether this widget (or its proxy) already has focus
        std::shared_ptr<LWidget> f = focusWidget;
        if (focusWidget->m_pExtraData != nullptr){
            std::shared_ptr<LWidget> p = focusWidget->m_pExtraData->focusProxy.lock();
            if(p != nullptr){
                f = p;
            }
        }
        // if it has, stop here.
        // otherwise a click on the focused widget would remove its focus if ClickFocus isn't set
        if (f->hasFocus()){
            break;
        }

        localPos += focusWidget->pos();
        focusWidget = focusWidget->parentT<LWidget>();
    }
}

std::shared_ptr<LWidget> LApplication::focusNextPrevChildHelper(std::shared_ptr<LWidget> toplevel,
                                                                bool next,
                                                                bool *wrappingOccurred)
{
    uint8_t focusFlag = tabAllWidgets() ? uint32_t(LFocusPolicy::Tab): uint8_t(LFocusPolicy::Strong);
    std::shared_ptr<LWidget> f = toplevel->focusWidget();
    if (f == nullptr){
        f = toplevel;
    }

    std::shared_ptr<LWidget> w = f;
    std::shared_ptr<LWidget> test = f->m_focusNext.lock();
    bool seenWindow = false;
    bool focusWidgetAfterWindow = false;
    while ((test !=nullptr) && test != f) {
        if (test->isWindow()){
            seenWindow = true;
        }

        // If the next focus widget has a focus proxy, we need to check to ensure
        // that the proxy is in the correct parent-child direction (according to
        // \a next). This is to ensure that we can tab in and out of compound widgets
        // without getting stuck in a tab-loop between parent and child.
        std::shared_ptr<LWidget> focusProxy = test->deepestFocusProxy();

        bool canTakeFocus = false;
        bool composites = false;

        if(focusProxy != nullptr){
            canTakeFocus = (uint8_t(focusProxy->focusPolicy()) & focusFlag) == focusFlag;

            if(next){
                composites = focusProxy->isAncestorOf(test);
            }else{
                composites = test->isAncestorOf(focusProxy);
            }
        }else{
            canTakeFocus = (uint8_t(test->focusPolicy()) & focusFlag) == focusFlag;
        }

        if (canTakeFocus && !composites
            && test->isVisibleTo(toplevel) && test->isEnabled()
            && !(w->windowType() == LWindowType::SubWindow && !w->isAncestorOf(test))
            && (toplevel->windowType() != LWindowType::SubWindow || toplevel->isAncestorOf(test))
            && f != focusProxy) {
            w = test;
            if (seenWindow){
                focusWidgetAfterWindow = true;
            }
            if (next){
                break;
            }
        }
        test = test->m_focusNext.lock();
    }

    if (wrappingOccurred != nullptr){
        *wrappingOccurred = next ? focusWidgetAfterWindow : !focusWidgetAfterWindow;
    }

    if (w == f) {
        if (m_inTabKeyEvent) {
            w->window()->setAttribute(LWidgetAttribute::KeyboardFocusChange);
            w->update();
        }
        return nullptr;
    }
    return w;

}

bool LApplication::notifyWidgetHelper(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> e)
{

    bool consumed = false;
    bool filtered = false;
    // send to all application event filters
    if (threadRequiresCoreApplication()
        && receiver->thread() == LThreadDataRegistry::instance().mainThread()
        && sendThroughApplicationEventFilters(receiver, e)) {
        filtered = true;
        return filtered;
    }

    if (receiver->isWidgetType()) {
        std::shared_ptr<LWidget> widget = std::dynamic_pointer_cast<LWidget>(receiver);

        if ((e->type() == LEvent::Enter || e->type() == LEvent::DragEnter) &&
            (!LApplication::activePopupWidget() || LApplication::activePopupWidget() == widget->window())){
            widget->setAttribute(LWidgetAttribute::UnderMouse, true);
        }
        else if (e->type() == LEvent::Leave || e->type() == LEvent::DragLeave){
            widget->setAttribute(LWidgetAttribute::UnderMouse, false);
        }

        std::shared_ptr<LLayout> layout=widget->m_layout;
        if (layout != nullptr) {
            layout->widgetEvent(e);
        }
    }

    // send to all receiver event filters
    if (sendThroughObjectEventFilters(receiver, e)) {
        filtered = true;
        return filtered;
    }

    // deliver the event
    consumed = receiver->handleEvent(e);
    e->setSpontaneous(false);
    return consumed;
}

void LApplication::insertWidget(std::shared_ptr<LWidget> widget)
{
    if (widget == nullptr){
        return;
    }
    /** 应该不会重复，而且移除的时候，就算相同也都会移除，就不对比检查了
     * 实际上释放后，这里的弱指针变空，移除删除的是所有的空指针
    **/
#ifdef _CHECK_SECURE
    auto it = std::find_if(m_allWidgets.begin(),
                           m_allWidgets.end(),
                           [widget](std::weak_ptr<LWidget> w) { return w.lock() == widget; });
    if (it == m_allWidgets.end())
    {
#endif
      m_allWidgets.push_back(widget);
#ifdef _CHECK_SECURE
    }
#endif
}

void LApplication::removeWidget(std::shared_ptr<LWidget> w)
{
    auto isWidget = [w](std::weak_ptr<LWidget> ww) { return ww.lock() == w; };
    m_allWidgets.remove_if(isWidget);
}

bool LApplication::notify(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> e)
{
    if (closingDown()){
        return true;
    }

    if (receiver == nullptr) {
        lWarn() << "LApplication::notify: Unexpected null receiver";
        return true;
    }

    const bool isWindowType = receiver->isWindowType();
    if (isWindowType) {
        std::shared_ptr<LWindow> window = receiver->getPtrT<LWindow>();
        if (LGuiApplication::sendWindowEventToPlatformWindow(window, e)) {
            return true; // Platform plugin ate the event
        }
    }

    LGuiApplication::captureGlobalModifierState(e);
    const bool isWidgetType = receiver->isWidgetType();

    switch (e->type()) {
    case LEvent::ApplicationDeactivate:{
        // Close all popups (triggers when switching applications
        // by pressing ALT-TAB on Windows, which is not receive as key event.
        closeAllPopups();
        break;
    }
    case LEvent::Wheel: // User input and window activation makes tooltips sleep
    case LEvent::ActivationChange:
    case LEvent::KeyPress:
    case LEvent::KeyRelease:
    case LEvent::FocusOut:
    case LEvent::FocusIn:
    case LEvent::MouseButtonPress:
    case LEvent::MouseButtonRelease:
    case LEvent::MouseButtonDblClick:{
        //d->toolTipFallAsleep.stop();
        //L_FALLTHROUGH();
        break;
    }
    case LEvent::Leave:{
        //d->toolTipWakeUp.stop();
        break;
    }
    default:{
        break;
    }
    }

    switch (e->type()) {
    case LEvent::KeyPress: {
        std::shared_ptr<LKeyEvent> keyEvent = std::dynamic_pointer_cast<LKeyEvent>(e);
        const int key = keyEvent->key();
        // When a key press is received which is not spontaneous then it needs to
        // be manually sent as a shortcut override event to ensure that any
        // matching shortcut is triggered first. This enables emulation/playback
        // of recorded events to still have the same effect.
        if (!e->spontaneous() && isWidgetType) {
            //std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(receiver);
            if (LWindowSystemInterface::sendShortcutOverrideEvent(receiver, keyEvent->timestamp(),
                                                                 key, keyEvent->modifiers(), keyEvent->text(),
                                                                 keyEvent->isAutoRepeat(), keyEvent->count())){
                return true;
            }
        }
        LApplication::m_inTabKeyEvent = (key == int(LKey::Backtab)
                                   || key == int(LKey::Tab)
                                   || key == int(LKey::Left)
                                   || key == int(LKey::Up)
                                   || key == int(LKey::Right)
                                   || key == int(LKey::Down));
        break;
    }
    default:{
        break;
    }
    }

    bool res = false;
    if (!isWidgetType) {
        res = notifyWidgetHelper(receiver, e);
    } else {
        switch (e->type()) {
        case LEvent::ShortcutOverride:
        case LEvent::KeyPress:
        case LEvent::KeyRelease:
        {
            //bool isWidget = receiver->isWidgetType();
            std::shared_ptr<LKeyEvent> key = std::dynamic_pointer_cast<LKeyEvent>(e);
            bool def = key->isAccepted();
            std::shared_ptr<LObject> pr = receiver;
            bool isWidget = true;
            std::shared_ptr<LWidget> parentWidget = nullptr;
            std::shared_ptr<LWidget> w = nullptr;
            bool isHeadNode = false;
            while (receiver) { //自下往上推消息
                if (def){
                    key->accept();
                }
                else{
                    key->ignore();
                }

                res = notifyWidgetHelper(receiver, e);

                w = nullptr;
                isWidget = receiver->isWidgetType();
                parentWidget = nullptr;
                isHeadNode = false;
                if(isWidget){
                    w = std::dynamic_pointer_cast<LWidget>(receiver);
                    parentWidget =  w->parentT<LWidget>();
                    isHeadNode= w->isWindow() || (parentWidget == nullptr);
                }

                if ((res && key->isAccepted())
                    || (pr == nullptr)
                    || isHeadNode) {
                    break;
                }
                receiver = w->parent();
            }
            LApplication::m_inTabKeyEvent = false;
            break;
        }
        case LEvent::MouseButtonPress:
        case LEvent::MouseButtonRelease:
        case LEvent::MouseButtonDblClick:
        case LEvent::MouseMove:
        {
            std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(receiver);
            std::shared_ptr<LMouseEvent> mouse = std::dynamic_pointer_cast<LMouseEvent>(e);
            LPoint<int> relpos = mouse->pos();

            if (e->spontaneous()) {
                if (e->type() != LEvent::MouseMove){
                    LApplication::giveFocusAccordingToFocusPolicy(w, e, relpos);
                }

                // ### These dynamic tool tips should be an OPT-IN feature. Some platforms
                // like OS X (probably others too), can optimize their views by not
                // dispatching mouse move events. We have attributes to control hover,
                // and mouse tracking, but as long as we are deciding to implement this
                // feature without choice of opting-in or out, you ALWAYS have to have
                // tracking enabled. Therefore, the other properties give a false sense of
                // performance enhancement.
                if (e->type() == LEvent::MouseMove
                    && mouse->buttons() == uint32_t(0)
                    && w->rect().contains(relpos)) { // Outside due to mouse grab?
                    m_toolTipWidget = w;
                    m_toolTipPos = relpos;
                    m_toolTipGlobalPos = mouse->globalPos();
                    //Todo
                    //std::shared_ptr<LStyle> s = m_toolTipWidget->style();
                    //int wakeDelay = s->styleHint(LStyle::SH_ToolTip_WakeUpDelay, nullptr, m_toolTipWidget, nullptr);
                    //toolTipWakeUp.start(d->toolTipFallAsleep.isActive() ? 20 : wakeDelay, this);
                }
            }

            bool eventAccepted = mouse->isAccepted();
            while (w != nullptr) {
                std::shared_ptr<LMouseEvent> me
                    = std::make_shared<LMouseEvent>(mouse->type(),
                                                    relpos.toPointF(),
                                                    mouse->windowPos(),
                                                    mouse->globalPos().toPointF(),
                                                    mouse->button(),
                                                    mouse->buttons(),
                                                    mouse->modifiers(),
                                                    mouse->source());
                me->setSpontaneous(mouse->spontaneous());
                me->setTimestamp(mouse->timestamp());
                LGuiApplication::setMouseEventFlags(me.get(), mouse->flags());

                // throw away any mouse-tracking-only mouse events
                if (!w->hasMouseTracking()
                    && mouse->type() == LEvent::MouseMove && mouse->buttons() == uint32_t(0)) {
                    // but still send them through all application event filters (normally done by notify_helper)
                    sendThroughApplicationEventFilters(w, w == receiver ? mouse : me);
                    res = true;
                } else {
                    w->setAttribute(LWidgetAttribute::NoMouseReplay, false);
                    res = notifyWidgetHelper(w, w == receiver ? mouse : me);
                    e->setSpontaneous(false);
                }
                eventAccepted = (w == receiver ? mouse : me)->isAccepted();
                if (res && eventAccepted){
                    break;
                }
                if (w->isWindow() || w->testAttribute(LWidgetAttribute::NoMousePropagation)){
                    break;
                }
                relpos += w->pos();
                w = w->parentT<LWidget>();
            }

            mouse->setAccepted(eventAccepted);
            m_hoverGlobalPos = mouse->globalPos();
            break;
        }
        case LEvent::RequestSoftwareInputPanel:{
            std::shared_ptr<LInputMethod> t = inputMethod();
            t->show();
            break;
        }
        case LEvent::CloseSoftwareInputPanel:{
            std::shared_ptr<LInputMethod> t = inputMethod();
            t->hide();
            break;
        }
        default:{
            res = notifyWidgetHelper(receiver, e);
            break;
        }
        }
    }
    return res;

}

void LApplication::init()
{
    LGuiApplication::init();
    LEditDoc::initGlobal();

    std::shared_ptr<LTimer> blinker = LApplication::blinkTimer();
    blinker->sigTimeout.connect(this, &LApplication::focusBlink, ConnectionType::Queued);
}

void LApplication::notifyActiveWindowChange(std::shared_ptr<LWindow> previous)
{
    std::shared_ptr<LWindow> wnd = LGuiApplication::m_focusWindow.lock();
    if (!m_popupWidgets.empty()){ // some delayed focus event to ignore
        return;
    }
    std::shared_ptr<LWidget> tlw = topLevelWidgetForWindow(wnd);
    LApplication::setActiveWindow(tlw);
}

std::shared_ptr<LWindow> LApplication::windowBlocked(std::shared_ptr<LWindow> window) const
{
    std::shared_ptr<LWindow> blockingWindow = nullptr;
    if (window == nullptr) {
        return blockingWindow;
    }

    if (m_modelWindows.empty()) {
        return blockingWindow;
    }
    std::shared_ptr<LWidget> popupWidget = LApplication::activePopupWidget();
    std::shared_ptr<LWindow> popupWindow = popupWidget ? popupWidget->windowHandle() : nullptr;
    if (popupWindow == window || (!popupWindow && window->isPopup())) {
        return blockingWindow;
    }

    std::shared_ptr<LWidget> groupLeaderForWidget = nullptr;
    bool hasFindGroupLeaderForWidget = false;

    for (const std::weak_ptr<LWindow>& mww : m_modelWindows) {
        std::shared_ptr<LWindow>  modalWindow = mww.lock();
        if(modalWindow == nullptr){
            continue;
        }

        // A window is not blocked by another modal window if the two are
        // the same, or if the window is a child of the modal window.
        if (window == modalWindow || modalWindow->isAncestorOf(window, LWindow::IncludeTransients)) {
            return blockingWindow;
        }

        LWindowModality windowModality = modalWindow->modality();
        std::shared_ptr<LWidgetWindow> modalWidgetWindow =
            std::dynamic_pointer_cast<LWidgetWindow>(modalWindow);
        if (windowModality == LWindowModality::NonModal) {
            // determine the modality type if it hasn't been set on the
            // modalWindow's widget, this normally happens when waiting for a
            // native dialog. use WindowModal if we are the child of a group
            // leader; otherwise use ApplicationModal.
            std::shared_ptr<LWidget> m = modalWidgetWindow ? modalWidgetWindow->widget() : nullptr;
            while (m && !m->testAttribute(LWidgetAttribute::GroupLeader)) {
                m = m->parentT<LWidget>();
                if (m){
                    m = m->window();
                }
            }
            windowModality = (m && m->testAttribute(LWidgetAttribute::GroupLeader))
                                 ? LWindowModality::WindowModal
                                 : LWindowModality::ApplicationModal;
        }

        switch (windowModality) {
        case LWindowModality::ApplicationModal:
        {
            if(!hasFindGroupLeaderForWidget){
                std::shared_ptr<LWidgetWindow> widgetWindow =
                    std::dynamic_pointer_cast<LWidgetWindow>(window);
                groupLeaderForWidget = widgetWindow ? widgetWindow->widget() : nullptr;
                while (groupLeaderForWidget && !groupLeaderForWidget->testAttribute(LWidgetAttribute::GroupLeader)){
                    groupLeaderForWidget = groupLeaderForWidget->parentT<LWidget>();
                }
                hasFindGroupLeaderForWidget = true;
            }


            if (groupLeaderForWidget) {
                // if \a widget has WA_GroupLeader, it can only be blocked by ApplicationModal children
                std::shared_ptr<LWidget> m = modalWidgetWindow ? modalWidgetWindow->widget() : nullptr;
                while (m && m != groupLeaderForWidget && !m->testAttribute(LWidgetAttribute::GroupLeader))
                    m = m->parentT<LWidget>();
                if (m == groupLeaderForWidget) {
                    blockingWindow = m->windowHandle();
                    return blockingWindow;
                }
            } else if (modalWindow != window) {
                blockingWindow = modalWindow;
                return blockingWindow;
            }
            break;
        }
        case LWindowModality::WindowModal:
        {
            std::shared_ptr<LWindow> w = window;
            do {
                std::shared_ptr<LWindow>  m = modalWindow;
                do {
                    if (m == w) {
                        blockingWindow = m;
                        return blockingWindow;
                    }
                    std::shared_ptr<LWindow> p = m->parent();
                    if (p == nullptr){
                        p = m->transientParent();
                    }
                    m = p;
                } while (m != nullptr);

                std::shared_ptr<LWindow> p = w->parent();
                if (p == nullptr){
                    p = w->transientParent();
                }
                w = p;
            } while (w != nullptr);
            break;
        }
        default:{
            break;
        }
        }
    }
    return blockingWindow;

}

void LApplication::focusBlink()
{
    std::shared_ptr<LWidget> focusW = focusWidget();
    if(focusW != nullptr){
        focusW->blink();
    }
}

void LApplication::sendSyntheticEnterLeave(std::shared_ptr<LWidget> widget)
{
    if(widget == nullptr){
        return;
    }

    if (widget->isWindow()){
        return;
    }

    const bool widgetInShow = widget->isVisible();

    std::shared_ptr<LWidget> lastMouseReceiver = LWidgetWindow::m_lastMouseReceiver.lock();
    if (!widgetInShow && widget != lastMouseReceiver){
        return; // Widget was not under the cursor when it was hidden/deleted.
    }

    std::shared_ptr<LWidget> parent = widget->parentT<LWidget>();
    if (widgetInShow){
        if(parent != nullptr && parent->m_widgetData.inShow){
            return; // Ingore recursive show.
        }
    }

    std::shared_ptr<LWidget> mouseGrabber = LWidget::mouseGrabber();
    if (mouseGrabber!= nullptr && mouseGrabber != widget){
        return; // Someone else has the grab; enter/leave should not occur.
    }

    std::shared_ptr<LWidget> tlw = widget->window();
    if (tlw->m_widgetData.isClosing){
        return; // Closing down the business.
    }

    if (widgetInShow && ((lastMouseReceiver == nullptr) || lastMouseReceiver->window() != tlw)){
        return; // Mouse cursor not inside the widget's top-level.
    }

    const LPoint<int> globalPos(LCursor::pos());
    LPoint<int> windowPos = tlw->mapFromGlobal(globalPos);

    // Find the current widget under the mouse. If this function was called from
    // the widget's destructor, we have to make sure childAt() doesn't take into
    // account widgets that are about to be destructed.
    std::shared_ptr<LWidget> widgetUnderCursor = tlw->childAt(windowPos);
    if (!widgetUnderCursor){
        widgetUnderCursor = tlw;
    }
    LPoint<int> pos = widgetUnderCursor->mapFrom(tlw, windowPos);

    if (widgetInShow && widgetUnderCursor != widget && !widget->isAncestorOf(widgetUnderCursor)){
        return; // Mouse cursor not inside the widget or any of its children.
    }
    if (widget->m_widgetData.inDestructor && LWidgetWindow::m_buttonDown.lock() == widget){
        LWidgetWindow::m_buttonDown.reset();
    }

    // A mouse move is not actually sent, but we utilize the sendMouseEvent() call to send the
    // enter/leave events as appropriate
    std::shared_ptr<LMouseEvent> e
        = std::make_shared<LMouseEvent>(LEvent::MouseMove, pos.toPointF(),
                                        windowPos.toPointF(), globalPos.toPointF(),
                                        LMouseButton::No,
                                        LMouseButton::No,
                                        LKeyModifier::No);
    sendMouseEvent(widgetUnderCursor, e, widgetUnderCursor, tlw,
                   LWidgetWindow::m_buttonDown,
                   LWidgetWindow::m_lastMouseReceiver,
                   true, true);
}

LK_NAMESPACE_END
