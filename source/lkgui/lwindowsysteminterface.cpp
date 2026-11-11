#include "lwindowsysteminterface.h"
#include "lguiapplication.h"
#include "lscreen.h"
#include "lshortcutman.h"
#include "lplatformwindow.h"
#include "lplatformscreen.h"
#include "lkcore/ldebug.h"
LK_NAMESPACE_BEGIN

bool LWindowSystemInterface::m_isDefaultSyncWinSysEvents = false;
//std::atomic_int LWindowSystemInterface::m_eventAccepted = 0;
LWinSysEventList LWindowSystemInterface::m_winSysEventQueue;
std::condition_variable LWindowSystemInterface::m_eventsFlushed;
std::mutex LWindowSystemInterface::m_flushEventMutex;
LElapsedTimer LWindowSystemInterface::m_eventTime(true);

namespace wse{
bool LTabletEvent::m_platformSynthesizesMouse = true;


LExposeEvent::LExposeEvent(std::shared_ptr<LWindow> w, const std::shared_ptr<LRegion> region)
    : LWinEvent(Expose, w)
    , m_isExposed(false)
    , m_region(region)
{
    if(w != nullptr){
        std::shared_ptr<LPlatformWindow> pw = w->handle();
        if(pw != nullptr){
            m_isExposed = pw->isExposed();
        }
    }
}

LGeometryChangeEvent::LGeometryChangeEvent(std::shared_ptr<LWindow> w, const LRect<int> &newGeometry)
    : LWinEvent(GeometryChange, w)
    , m_newGeometry(newGeometry)
{
    if(w != nullptr){
        std::shared_ptr<LPlatformWindow> pw = w->handle();
        if(pw != nullptr){
            const auto nativeGeometry = pw->LPlatformWindow::geometry();
            m_requestedGeometry = w->isTopLevel()
                                    ? LHighDpi::fromNativePixels(nativeGeometry, w.get())
                                    : LHighDpi::fromNativeLocalPosition(nativeGeometry, w.get());
        }
    }
}

LWheelEvent::LWheelEvent(std::shared_ptr<LWindow> w, uint32_t time,
                         const LPoint<double> &local, const LPoint<double> &global,
                         LPoint<int> pixelD, LPoint<int> angleD,
                         LFlags<LKeyModifier> mods, LScrollPhase phase,
                         LMouseEventSource src, bool inverted)
    : LInputEvent(w, time, Wheel, mods), m_pixelDelta(pixelD), m_angleDelta(angleD),
    m_localPos(local), m_globalPos(global), m_phase(phase), m_source(src), m_inverted(inverted)
{

}


void LTabletEvent::setPlatformSynthesizesMouse(bool v)
{
    m_platformSynthesizesMouse = v;
}

}


LWindowSystemInterface::LWindowSystemInterface()
{

}

bool LWindowSystemInterface::sendWinSysEvents(LFlags<LProcessEvents>  flags)
{
    if(flags & LProcessEvents::ExcludeUserInput){
        std::list<std::shared_ptr<LWinSysEvent> > list =  m_winSysEventQueue.takeAllNonUserInput();
        for(std::shared_ptr<LWinSysEvent> event: list){
            LGuiApplication::processWindowSystemEvent(event);
        }
        return list.size();
    }else{
        int nevents = 0;
        while (!m_winSysEventQueue.isEmpty()) {
            std::shared_ptr<LWinSysEvent> event = m_winSysEventQueue.takeFirstOrReturnNull();
            if (event == nullptr){
                continue;
            }

            nevents++;
            LGuiApplication::processWindowSystemEvent(event);
        }
        return (nevents > 0);
    }
}

bool LWindowSystemInterface::handleNativeEvent(std::shared_ptr<LWindow> window, const std::string &eventType, void *message, lintptr *result)
{
    return LGuiApplication::processNativeEvent(window, eventType, message, result);
}

bool LWindowSystemInterface::handleWinSysEvent(std::shared_ptr<LWinSysEvent> ev, LDeliverType deliverType)
{
    bool accepted = true;
    switch(deliverType){
    case LDeliverType::Sync:{
       if (LThreadDataRegistry::instance().currentThreadData() == LGuiApplication::instance()->threadData()) {
            // Process the event immediately on the current thread and return the accepted state.
            LGuiApplication::processWindowSystemEvent(ev);
            accepted = ev->m_eventAccepted;
        } else {
            m_winSysEventQueue.append(ev);
            if (std::shared_ptr<LEventDispatcher> dispatcher = LGuiApplication::coreEventDispatcher()){
                dispatcher->wakeUp();
            }
            accepted = LWindowSystemInterface::flushWinSysEvents();
        }
        break;
    }
    case LDeliverType::Async:{
        m_winSysEventQueue.append(ev);
        if (std::shared_ptr<LEventDispatcher> dispatcher = LGuiApplication::coreEventDispatcher()){
            dispatcher->wakeUp();
        }
        break;
    }
    default:{
        if(m_isDefaultSyncWinSysEvents){
            m_winSysEventQueue.append(ev);
            if (std::shared_ptr<LEventDispatcher> dispatcher = LGuiApplication::coreEventDispatcher()){
                dispatcher->wakeUp();
            }
        }else{
            if (LThreadDataRegistry::instance().currentThreadData() == LGuiApplication::instance()->threadData()) {
                // Process the event immediately on the current thread and return the accepted state.
                LGuiApplication::processWindowSystemEvent(ev);
                accepted = ev->m_eventAccepted;
            } else {
                m_winSysEventQueue.append(ev);
                if (std::shared_ptr<LEventDispatcher> dispatcher = LGuiApplication::coreEventDispatcher()){
                    dispatcher->wakeUp();
                }
                accepted = LWindowSystemInterface::flushWinSysEvents();
            }
        }
        break;
    }
    }
    return accepted;
}

void LWindowSystemInterface::handleExposeEvent(std::shared_ptr<LWindow> window, const std::shared_ptr<LRegion> region, LDeliverType deliverType)
{
    std::shared_ptr<LRegion> t = LHighDpi::fromNativeLocalExposedRegion(region, window.get());
    std::shared_ptr<wse::LExposeEvent> e = std::make_shared<wse::LExposeEvent>(window, t);
    LWindowSystemInterface::handleWinSysEvent(e, deliverType);
}

void LWindowSystemInterface::handleGeometryChange(std::shared_ptr<LWindow> window, const LRect<int> &newRect, LDeliverType deliverType)
{
    const LRect<int> newRectDi = window->isTopLevel()
                               ? LHighDpi::fromNativePixels(newRect, window.get())
                               : LHighDpi::fromNativeLocalPosition(newRect, window.get());

    std::shared_ptr<LPlatformWindow> pw = window->handle();
    if (pw != nullptr) {
        pw->LPlatformWindow::setGeometry(newRect);
    }
    //lWarn() << "rectDi: " << newRectDi.m_width << "," << newRectDi.m_height;
    std::shared_ptr<wse::LGeometryChangeEvent> e = std::make_shared<wse::LGeometryChangeEvent>(window, newRectDi);
    LWindowSystemInterface::handleWinSysEvent(e, deliverType);
}

bool LWindowSystemInterface::handleCloseEvent(std::shared_ptr<LWindow> window, LDeliverType deliverType)
{
    std::shared_ptr<wse::LWinEvent> e =
        std::make_shared<wse::LWinEvent>(LWinSysEvent::Close, window);
    return LWindowSystemInterface::handleWinSysEvent(e, deliverType);
}

void LWindowSystemInterface::handleScreenAdded(std::shared_ptr<LPlatformScreen> ps,
                                               bool isPrimary)
{
    std::shared_ptr<LScreen> scr = std::make_shared<LScreen>();
    LScreen::init(ps, scr);

    if (isPrimary){
        LGuiApplication::m_screens.push_front(scr);
    }
    else{
        LGuiApplication::m_screens.push_back(scr);
    }
    LGuiApplication::resetCachedDevicePixelRatio();
    //emit qGuiApp->screenAdded(screen);
    if (isPrimary){
        //emit qGuiApp->primaryScreenChanged(screen);
    }
}

void LWindowSystemInterface::handleScreenRemoved(std::shared_ptr<LPlatformScreen> screen)
{
    std::shared_ptr<LScreen> scr = screen->screen();
    auto iter = LGuiApplication::m_screens.begin();
    for(; iter != LGuiApplication::m_screens.end(); ){
        if(*iter == scr){
            iter = LGuiApplication::m_screens.erase(iter);
        }else{
            ++iter;
        }
    }

    LGuiApplication::resetCachedDevicePixelRatio();
    if (LGuiApplication::closingDown()){
        return;
    }

    bool movingFromVirtualSibling = false;
    std::shared_ptr<LScreen> primaryScreen = LGuiApplication::primaryScreen();

    if(primaryScreen != nullptr){
        std::shared_ptr<LPlatformScreen> psh = primaryScreen->handle();
        std::list<std::shared_ptr<LPlatformScreen> > siblings = psh->virtualSiblings();
        auto it = find(siblings.begin(), siblings.end(), screen);
        if (it != siblings.end())
        {
            movingFromVirtualSibling = true;
        }
    }

    // Move any leftover windows to the primary screen
    const auto allWindows = LGuiApplication::allWindows();
    for (std::shared_ptr<LWindow> window : allWindows) {
        if (!window->isTopLevel() || window->screen() != scr){
            continue;
        }
        const bool wasVisible = window->visible();
        window->setScreen(primaryScreen);
        // Re-show window if moved from a virtual sibling screen. Otherwise
        // leave it up to the application developer to show the window.
        if (movingFromVirtualSibling){
            window->setVisible(wasVisible);
        }
    }
}

void LWindowSystemInterface::handlePrimaryScreenChanged(std::shared_ptr<LPlatformScreen> newPrimary)
{
    std::shared_ptr<LScreen> newPrimaryScreen = newPrimary->screen();

    if(!LGuiApplication::m_screens.empty()){
        if(LGuiApplication::m_screens.front() == newPrimaryScreen){
            return;
        }
    }

    for(std::shared_ptr<LScreen>& s:  LGuiApplication::m_screens){
        if(s == newPrimaryScreen){
            LGuiApplication::m_screens.front().swap(s);
            break;
        }
    }
}

void LWindowSystemInterface::handleScreenOrientationChange(std::shared_ptr<LScreen> screen, LScreenOrientation orientation)
{
    std::shared_ptr<wse::LScreenOrientationEvent> e = std::make_shared<wse::LScreenOrientationEvent>(screen, orientation);
    handleWinSysEvent(e);
}

void LWindowSystemInterface::handleScreenGeometryChange(std::shared_ptr<LScreen> screen, const LRect<int> &geometry, const LRect<int> &availableGeometry)
{
    std::shared_ptr<wse::LScreenGeometryEvent> e = std::make_shared<wse::LScreenGeometryEvent>(screen,
                                                                                               LHighDpi::fromNativeScreenGeometry(geometry, screen.get()),
                                                                                               LHighDpi::fromNative(availableGeometry, screen.get(), geometry.topLeft()));
    handleWinSysEvent(e);
}

void LWindowSystemInterface::handleScreenLogicalDotsPerInchChange(std::shared_ptr<LScreen> screen, double dpiX, double dpiY)
{
    const std::pair<double, double> effectiveDpi = LPlatformScreen::overrideDpi(std::pair<double, double>(dpiX, dpiY));
    std::shared_ptr<wse::LScreenLogicalDotsPerInchEvent> e = std::make_shared<wse::LScreenLogicalDotsPerInchEvent>(screen, effectiveDpi.first, effectiveDpi.second);
    handleWinSysEvent(e);
}

void LWindowSystemInterface::handleScreenRefreshRateChange(std::shared_ptr<LScreen> screen, double newRefreshRate)
{

}

void LWindowSystemInterface::handleWindowScreenChanged(std::shared_ptr<LWindow> window,
                                                       std::shared_ptr<LScreen> screen,
                                                       LDeliverType deliverType)
{
    std::shared_ptr<wse::LWindowScreenChangedEvent> e =
        std::make_shared<wse::LWindowScreenChangedEvent>(window, screen);
    handleWinSysEvent(e, deliverType);
}

void LWindowSystemInterface::handleWindowStateChanged(std::shared_ptr<LWindow> window,
                                                      LFlags<LWindowState> newState,
                                                      int oldState,
                                                      LDeliverType deliverType)
{
    if (oldState < 0){
        oldState = window->windowStates();
    }

    std::shared_ptr<wse::LWindowStateChangedEvent> e =
        std::make_shared<wse::LWindowStateChangedEvent>(window, newState, LFlags<LWindowState>(oldState));
    handleWinSysEvent(e, deliverType);
}

bool LWindowSystemInterface::handleMouseEvent(std::shared_ptr<LWindow> window,
                                              const LPoint<double> &local,
                                              const LPoint<double> &global,
                                              LFlags<LMouseButton> state,
                                              LMouseButton button,
                                              LEvent::Type type,
                                              LFlags<LKeyModifier> mods,
                                              LMouseEventSource source,
                                              LDeliverType deliverType)
{
    uint64_t time = LWindowSystemInterface::m_eventTime.msecsElapsed();

    LPoint<double> localPos = LHighDpi::fromNativeLocalPosition(local, window.get());
    LPoint<double> globalPos = LHighDpi::fromNativePixels(global, window.get());

    std::shared_ptr<wse::LMouseEvent> e =
        std::make_shared<wse::LMouseEvent>(window, time, localPos, globalPos,
                                           state, mods, button, type, source);
    return LWindowSystemInterface::handleWinSysEvent(e, deliverType);
}

bool LWindowSystemInterface::handleFrameStrutMouseEvent(std::shared_ptr<LWindow> window,
                                                        const LPoint<double> &local,
                                                        const LPoint<double> &global,
                                                        LFlags<LMouseButton> state,
                                                        LMouseButton button,
                                                        LEvent::Type type,
                                                        LFlags<LKeyModifier> mods,
                                                        LMouseEventSource source)
{
    uint64_t time = LWindowSystemInterface::m_eventTime.msecsElapsed();

    LPoint<double> localPos = LHighDpi::fromNativeLocalPosition(local, window.get());
    LPoint<double> globalPos = LHighDpi::fromNativePixels(global, window.get());

    std::shared_ptr<wse::LMouseEvent> e =
        std::make_shared<wse::LMouseEvent>(window, time, localPos, globalPos,
                                           state, mods, button, type, source, true);

    return LWindowSystemInterface::handleWinSysEvent(e);
}

void LWindowSystemInterface::handleEnterEvent(std::shared_ptr<LWindow> window,
                                              const LPoint<double> &local,
                                              const LPoint<double> &global,
                                              LDeliverType deliverType)
{
    if (window != nullptr) {
        std::shared_ptr<wse::LEnterEvent> e
            = std::make_shared<wse::LEnterEvent>(window,
                                                LHighDpi::fromNativeLocalPosition(local, window.get()),
                                                LHighDpi::fromNativePixels(global, window.get()));
        LWindowSystemInterface::handleWinSysEvent(e, deliverType);
    }
}

void LWindowSystemInterface::handleLeaveEvent(std::shared_ptr<LWindow> window, LDeliverType deliverType)
{
    std::shared_ptr<wse::LWinEvent> e
        = std::make_shared<wse::LWinEvent>(LWinSysEvent::Leave, window);

    LWindowSystemInterface::handleWinSysEvent(e, deliverType);
}

void LWindowSystemInterface::handleEnterLeaveEvent(std::shared_ptr<LWindow> enter,
                                                   std::shared_ptr<LWindow> leave,
                                                   const LPoint<double> &local,
                                                   const LPoint<double> &global)
{
    handleLeaveEvent(leave, LDeliverType::Async);
    handleEnterEvent(enter, local, global);
}

void LWindowSystemInterface::handleWindowActivated(std::shared_ptr<LWindow> window, LFocusReason r)
{

    std::shared_ptr<wse::LActivatedWindowEvent> e
        = std::make_shared<wse::LActivatedWindowEvent>(window, r);
    LWindowSystemInterface::handleWinSysEvent(e);
}

bool LWindowSystemInterface::sendShortcutOverrideEvent(std::shared_ptr<LObject> o, uint64_t timestamp,
                                                       int k, LFlags<LKeyModifier> mods, const std::wstring &text,
                                                       bool autorep, uint16_t count)
{
    // FIXME: This method should not allow targeting a specific object, but should
    // instead forward the event to a window, which then takes care of normal event
    // propagation. We need to fix a lot of tests before we can refactor this (the
    // window needs to be exposed and active and have a focus object), so we leave
    // it as is for now.

    LGuiApplication::m_modifierButtons = mods;

    std::shared_ptr<LKeyEvent> event
        = std::make_shared<LKeyEvent>(LEvent::ShortcutOverride, k, mods, text, autorep, count);
    event->setTimestamp(timestamp);

    LShortcutMan &shortcutMan = LGuiApplication::guiInst()->m_shortcutMan;
    if (shortcutMan.state() == LKeySequence::NoMatch) {
        // Try sending as KeyEvent::ShortcutOverride first
        LCoreApplication::sendEvent(o, event);
        if (event->isAccepted()){
            return false;
        }
    }
    // Then as LShortcutEvent
    return shortcutMan.tryShortcut(event);
}

bool LWindowSystemInterface::handleShortcutEvent(std::shared_ptr<LWindow> window, uint64_t timestamp,
                                                 int k, LFlags<LKeyModifier> mods,
                                                 uint32_t nativeScanCode, uint32_t nativeVirtualKey,
                                                 uint32_t nativeModifiers, const std::wstring &text,
                                                 bool autorep, uint16_t count)
{
    if (window == nullptr){
        window = LGuiApplication::focusWindow();
    }

    LShortcutMan &shortcutMan = LGuiApplication::guiInst()->m_shortcutMan;
    if (shortcutMan.state() == LKeySequence::NoMatch) {
        // Check if the shortcut is overridden by some object in the event delivery path (typically the focus object).
        // If so, we should not look up the shortcut in the shortcut map, but instead deliver the event as a regular
        // key event, so that the target that accepted the shortcut override event can handle it. Note that we only
        // do this if the shortcut map hasn't found a partial shortcut match yet. If it has, the shortcut can not be
        // overridden.
        std::shared_ptr<wse::LKeyEvent> shortcutOverrideEvent =
            std::make_shared<wse::LKeyEvent>(window, timestamp,
                                            LEvent::ShortcutOverride, k,
                                            mods, nativeScanCode, nativeVirtualKey,
                                            nativeModifiers, text, autorep, count);

        {
            if (handleWinSysEvent(shortcutOverrideEvent, LDeliverType::Sync)){
                return false;
            }
        }
    }

    // The shortcut event is dispatched as a LShortcutEvent, not a QKeyEvent, but we use
    // the LKeyEvent as a container for the various properties that the shortcut map needs
    // to inspect to determine if a shortcut matched the keys that were pressed.
    std::shared_ptr<LKeyEvent> keyEvent
        = std::make_shared<LKeyEvent>(LEvent::ShortcutOverride, k, mods, nativeScanCode,
                                    nativeVirtualKey, nativeModifiers, text, autorep, count);

    return shortcutMan.tryShortcut(keyEvent);
}

bool LWindowSystemInterface::handleExtendedKeyEvent(std::shared_ptr<LWindow> window,
                                                    LEvent::Type type, int key,
                                                    LFlags<LKeyModifier> modifiers, uint32_t nativeScanCode,
                                                    uint32_t nativeVirtualKey, uint32_t nativeModifiers,
                                                    const std::wstring &text, bool autorep, uint16_t count)
{
    uint64_t time = LWindowSystemInterface::m_eventTime.msecsElapsed();
    return handleExtendedKeyEvent(window, time, type, key, modifiers, nativeScanCode, nativeVirtualKey, nativeModifiers,
                                  text, autorep, count);
}

bool LWindowSystemInterface::handleExtendedKeyEvent(std::shared_ptr<LWindow> window, uint64_t timestamp,
                                                    LEvent::Type type, int key,
                                                    LFlags<LKeyModifier> modifiers, uint32_t nativeScanCode,
                                                    uint32_t nativeVirtualKey, uint32_t nativeModifiers,
                                                    const std::wstring &text, bool autorep, uint16_t count)
{
    std::shared_ptr<wse::LKeyEvent> e =
        std::make_shared<wse::LKeyEvent>(window, timestamp, type, key, modifiers,
                                         nativeScanCode, nativeVirtualKey, nativeModifiers, text, autorep, count);
    return LWindowSystemInterface::handleWinSysEvent(e);
}

bool LWindowSystemInterface::handleWheelEvent(std::shared_ptr<LWindow> window,
                                              const LPoint<double> &local, const LPoint<double> &global,
                                              LPoint<int> pixelDelta, LPoint<int> angleDelta,
                                              LFlags<LKeyModifier> mods,
                                              LScrollPhase phase, LMouseEventSource source)
{
    uint64_t time = LWindowSystemInterface::m_eventTime.msecsElapsed();
    return handleWheelEvent(window, time, local, global, pixelDelta, angleDelta, mods, phase, source);
}

bool LWindowSystemInterface::handleWheelEvent(std::shared_ptr<LWindow> window, uint64_t timestamp,
                                              const LPoint<double> &local, const LPoint<double> &global,
                                              LPoint<int> pixelDelta, LPoint<int> angleDelta,
                                              LFlags<LKeyModifier> mods,
                                              LScrollPhase phase, LMouseEventSource source,
                                              bool invertedScrolling)
{

    std::shared_ptr<wse::LWheelEvent> e =
        std::make_shared<wse::LWheelEvent>(window, timestamp,
                                          LHighDpi::fromNativeLocalPosition(local, window.get()),
                                          LHighDpi::fromNativePixels(global, window.get()),
                                          pixelDelta, angleDelta,
                                          mods, phase, source, invertedScrolling);

    return LWindowSystemInterface::handleWinSysEvent(e);
}

bool LWindowSystemInterface::flushWinSysEvents(LFlags<LProcessEvents> flags)
{
    int count = m_winSysEventQueue.count();
    if (count <=0){
        return false;
    }
    if (LGuiApplication::instance() == nullptr) {
        lWarn() << "LWindowSystemInterface::flushWindowSystemEvents() invoked without LGuiApplication.";
        m_winSysEventQueue.clear();
        return false;
    }

    if (LThreadDataRegistry::instance().currentThreadData() != LGuiApplication::instance()->threadData()) {
        std::unique_lock<std::mutex> lock(m_flushEventMutex);
        std::shared_ptr<LWinSysEvent> e = std::make_shared<LWinSysEvent>(LWinSysEvent::FlushEvents);
        LWindowSystemInterface::handleWinSysEvent(e, LDeliverType::Async);
        m_eventsFlushed.wait(lock);
        return false;
    } else {
        return sendWinSysEvents(flags);
    }
}

void LWindowSystemInterface::deferredFlushWinSysEvents(LFlags<LProcessEvents> flags)
{
    //std::assert(LThreadDataRegistry::instance().currentThreadData() == LGuiApplication::instance()->threadData());
    std::unique_lock<std::mutex> lock(m_flushEventMutex);
    sendWinSysEvents(flags);
    m_eventsFlushed.notify_one();
}

std::shared_ptr<LWinSysEvent> LWindowSystemInterface::peekWindowSystemEvent(LWinSysEvent::Type t)
{
    return m_winSysEventQueue.peekAtFirstOfType(t);
}

void LWindowSystemInterface::removeWindowSystemEvent(std::shared_ptr<LWinSysEvent> event)
{
    m_winSysEventQueue.remove(event);
}



LK_NAMESPACE_END
