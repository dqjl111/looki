#include "lguiapplication.h"
#include "lscreen.h"
#include "lplatformwindow.h"
#include "lkcore/ldebug.h"
#include "linputmethod.h"
#include "lplatformcursor.h"
#include "lkgui/paint/lcolortrclut.h"
#include "lkgui/text/lfontdatabase.h"
#include <iterator>

LK_NAMESPACE_BEGIN

std::shared_ptr<LClipboard> LGuiApplication::m_clipboard;
std::list< std::shared_ptr<LScreen> > LGuiApplication::m_screens;
std::list<std::weak_ptr<LWindow> > LGuiApplication::m_windows;
double LGuiApplication::m_maxDevicePixelRatio = 0;
bool LGuiApplication::m_highDpiScalingUpdated = false;
std::weak_ptr<LWindow> LGuiApplication::m_focusWindow;
std::weak_ptr<LWindow> LGuiApplication::m_currentMouseWindow;
std::weak_ptr<LWindow> LGuiApplication::m_currentMousePressWindow;
std::shared_ptr<LFont> LGuiApplication::m_font;

uint64_t LGuiApplication::m_mousePressTime = 0;
LMouseButton LGuiApplication::m_mousePressButton = LMouseButton::No;
int LGuiApplication::m_mousePressX = 0;
int LGuiApplication::m_mousePressY = 0;
LPoint<double> LGuiApplication::m_lastCursorPosition;
LApplicationState LGuiApplication::m_applicationState = LApplicationState::Inactive;

LFlags<LKeyModifier> LGuiApplication::m_modifierButtons;
LFlags<LMouseButton> LGuiApplication::m_mouseButtons;

std::shared_ptr<LTexture> LGuiApplication::m_appIcon;
LThemeHints LGuiApplication::m_themeHints;
std::shared_ptr<LPalette> LGuiApplication::m_palette = nullptr;
LCursor  LGuiApplication::m_cursor;
LCursor  LGuiApplication::m_lastCursor;

bool LGuiApplication::m_antialiasingDisabled = false;
LLayoutDirection LGuiApplication::m_layoutDirection = LLayoutDirection::Auto;

static double fontSmoothingGamma = 1.7;


enum class LMouseMasks : uint32_t{
    CapsMask = 0xFF,
    SourceMaskDst = 0xFF00,
    SourceMaskSrc = CapsMask,
    SourceShift = 8,
    FlagsCapsMask = 0xFF0000,
    FlagsShift = 16
};


LGuiApplication::LGuiApplication(int &aargc, char **aargv) :LCoreApplication(aargc, aargv){
}

std::shared_ptr<LScreen> LGuiApplication::primaryScreen()
{
    if (m_screens.empty()){
        return nullptr;
    }
    return m_screens.front();
}

std::list<std::shared_ptr<LScreen> > LGuiApplication::screens()
{
    return m_screens;
}

std::shared_ptr<LScreen> LGuiApplication::screenAt(int index, int defaultIndex)
{
    if (m_screens.empty()){
        return nullptr;
    }
    if(m_screens.size() >index && index >= 0){
        return *std::next(m_screens.cbegin(), index);
    }

    if(m_screens.size() >defaultIndex && defaultIndex >= 0){
        return *std::next(m_screens.cbegin(), defaultIndex);
    }
    return nullptr;
}

std::shared_ptr<LScreen>  LGuiApplication::screenAt(const LPoint<int> &point)
{
    for (const std::shared_ptr<LScreen>& sl : m_screens) {
        if(sl != nullptr){
            if(sl->m_geometry.contains(point)){
                return sl;
            }
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<LWindow> > LGuiApplication::allWindows()
{
    std::vector<std::shared_ptr<LWindow> >  ret;
    for (const std::weak_ptr<LWindow>& winI : m_windows) {
        std::shared_ptr<LWindow> window = winI.lock();
        if(window != nullptr){
            ret.push_back(window);
        }
    }
    return ret;
}

std::vector<std::shared_ptr<LWindow> > LGuiApplication::topLevelWindows()
{
    std::vector<std::shared_ptr<LWindow> >  topLevelWindows;
    for (const std::weak_ptr<LWindow>& winI : m_windows) {
        std::shared_ptr<LWindow> window = winI.lock();
        if(window != nullptr){
            if (!window->isTopLevel()){
                continue;
            }

            if (window->type() == LWindowType::Desktop){
                continue;
            }

            std::shared_ptr<LPlatformWindow> plat = window->handle();
            if (plat && plat->isEmbedded()){
                continue;
            }
            topLevelWindows.push_back(window);
        }
    }

    return topLevelWindows;
}

std::shared_ptr<LWindow> LGuiApplication::topLevelAt(const LPoint<int> &pos)
{
    if (std::shared_ptr<LScreen> windowScreen = screenAt(pos)) {
        const LPoint<int> devicePosition =LHighDpi::toNativePixels(pos, windowScreen.get());
        return windowScreen->handle()->topLevelAt(devicePosition);
    }
    return nullptr;
}

std::shared_ptr<LTexture> LGuiApplication::windowIcon()
{
    return m_appIcon;
}

const LCursor& LGuiApplication::overrideCursor()
{
    return m_cursor;
}

void LGuiApplication::setOverrideCursor(const LCursor &c)
{
    if(c.isValid()){
        if(m_cursor != c){
            m_lastCursor = m_cursor;
            applyOverrideCursor(c);
        }
    }
}

void LGuiApplication::restoreOverrideCursor(const LCursor &c)
{
    if(c.isValid()){
        if(m_lastCursor == c){
            if(m_cursor != c){
                applyOverrideCursor(c);
            }
        }else{
            if(m_cursor.shape() != LCursorShape::Arrow){
                applyOverrideCursor(LCursor(LCursorShape::Arrow));
            }
        }
    }
}

void LGuiApplication::applyOverrideCursor(const LCursor &c)
{
    if(c.isValid()){
        m_cursor = c;
        for (const std::shared_ptr<LScreen>& screen : m_screens) {
            std::shared_ptr<LPlatformCursor> cursor = screen->handle()->cursor();
            if (cursor != nullptr){
                cursor->setOverrideCursor(c);
            }
        }
    }
}


std::shared_ptr<LWindow> LGuiApplication::modalWindow()
{
    return nullptr;
}

void LGuiApplication::showModalWindow(std::shared_ptr<LWindow> window)
{

}

void LGuiApplication::hideModalWindow(std::shared_ptr<LWindow> window)
{

}


static void updateBlockedStatusRecursion(std::shared_ptr<LWindow> window, bool shouldBeBlocked)
{
    if (window->blockedByModalWindow() != shouldBeBlocked) {
        window->setBlockedByModalWindow(shouldBeBlocked);
        std::shared_ptr<LEvent> e
            = std::make_shared<LEvent>(shouldBeBlocked ? LEvent::WindowBlocked : LEvent::WindowUnblocked);
        LGuiApplication::sendEvent(window, e);
        const std::vector<std::weak_ptr<LWindow> >& children = window->children();
        for (const std::weak_ptr<LWindow>& w : children) {
            std::shared_ptr<LWindow> wp = w.lock();
            if (wp != nullptr){
                updateBlockedStatusRecursion(wp, shouldBeBlocked);
            }
        }
    }
}

void LGuiApplication::updateBlockedStatus(std::shared_ptr<LWindow> window)
{
    bool shouldBeBlocked = false;
    const bool popupType = (window->type() == LWindowType::ToolTip) || (window->type() == LWindowType::Popup);
    if (!popupType && !lGuiApp->m_modelWindows.empty()){
        shouldBeBlocked = lGuiApp->windowBlocked(window) != nullptr;
    }
    updateBlockedStatusRecursion(window, shouldBeBlocked);
}

std::shared_ptr<LWindow> LGuiApplication::windowBlocked(std::shared_ptr<LWindow> window) const
{
    std::shared_ptr<LWindow> blockingWindow = nullptr;
    if (m_modelWindows.empty()) {
        return blockingWindow;
    }

    for (const std::weak_ptr<LWindow>& mww : m_modelWindows) {
        std::shared_ptr<LWindow>  modalWindow = mww.lock();
        if(modalWindow == nullptr){
            continue;
        }

        // A window is not blocked by another modal window if the two are
        // the same, or if the window is a child of the modal window.
        if (window == modalWindow || modalWindow->isAncestorOf(window, LWindow::IncludeTransients)) {
            blockingWindow = nullptr;
            return blockingWindow;
        }

        LWindowModality windowModality = modalWindow->modality();
        switch (windowModality) {
        case LWindowModality::ApplicationModal:
        {
            if (modalWindow != window) {
                blockingWindow = modalWindow;
                return blockingWindow;
            }
            break;
        }
        case LWindowModality::WindowModal:
        {
            std::shared_ptr<LWindow> w = window;
            do {
                std::shared_ptr<LWindow> m = modalWindow;
                do {
                    if (m == w) {
                        blockingWindow = m;
                        return blockingWindow;
                    }
                    std::shared_ptr<LWindow> p = m->parent();
                    if (p== nullptr){
                        p = m->transientParent();
                    }
                    m = p;
                } while (m != nullptr);

                std::shared_ptr<LWindow> p = w->parent();
                if (p == nullptr){
                    p = w->transientParent();
                }
                w = p;
            } while (w!= nullptr);
            break;
        }
        default:{
            lFatal() << "internal error, a modal widget cannot be modeless";
            break;
        }
        }
    }
    return blockingWindow;
}

bool LGuiApplication::notify(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event) {
    if (receiver->isWindowType()) {
        std::shared_ptr<LWindow> window = receiver->getPtrT<LWindow>();
        if (LGuiApplication::sendWindowEventToPlatformWindow(window, event)) {
            return true; // Platform plugin ate the event
        }
    }
    LGuiApplication::captureGlobalModifierState(event);
    return LCoreApplication::notify(receiver, event);
}

void LGuiApplication::init()
{
    LCoreApplication::init();
    m_isAppRunning = false;

    //Todo
    m_isAppRunning = true;
}

void LGuiApplication::notifyActiveWindowChange(std::shared_ptr<LWindow> previous)
{

}

std::shared_ptr<LColorTrcLut> LGuiApplication::colorProfileForA8Text()
{
    if (!m_a8ColorProfile){
        m_a8ColorProfile = LColorTrcLut::fromGamma(2.31); // This is a hard-coded thing for Windows text rendering
    }
    return m_a8ColorProfile;
}

std::shared_ptr<LColorTrcLut> LGuiApplication::colorProfileForA32Text()
{
    if (!m_a32ColorProfile) {
        m_a32ColorProfile = LColorTrcLut::fromGamma(fontSmoothingGamma);
    }
    return m_a32ColorProfile;
}

std::shared_ptr<LObject> LGuiApplication::focusObject()
{
    std::shared_ptr<LWindow>  win = focusWindow();
    if (win != nullptr){
        return win->focusObject();
    }
    return nullptr;
}

LMouseEventSource LGuiApplication::mouseEventSource(const LMouseEvent *event)
{
    return LMouseEventSource((event->m_caps & uint32_t(LMouseMasks::SourceMaskDst)) >> uint32_t(LMouseMasks::SourceShift));
}

void LGuiApplication::setMouseEventSource(LMouseEvent *event, LMouseEventSource source)
{
    uint32_t value = uint32_t(source);
    event->m_caps &= ~uint32_t(LMouseMasks::SourceMaskDst);
    event->m_caps |= (value & uint32_t(LMouseMasks::SourceMaskSrc)) << uint32_t(LMouseMasks::SourceShift);
}

LFlags<LMouseEventFlag> LGuiApplication::mouseEventFlags(const LMouseEvent *event)
{
    return LFlags<LMouseEventFlag>((event->m_caps & uint32_t(LMouseMasks::CapsMask)) >> uint32_t(LMouseMasks::FlagsShift));
}

void LGuiApplication::setMouseEventFlags(LMouseEvent *event, LFlags<LMouseEventFlag> flags)
{
    uint32_t value = flags;
    event->m_caps &= ~uint32_t(LMouseMasks::CapsMask);
    event->m_caps |= (value & uint32_t(LMouseEventFlag::Mask)) << uint32_t(LMouseMasks::FlagsShift);
}

std::shared_ptr<LPalette> LGuiApplication::palette()
{
    if(m_palette == nullptr){
        m_palette = std::make_shared<LPalette>();
        m_palette->initDefaultPalette();
    }
    return m_palette;
}

bool LGuiApplication::processNativeEvent(std::shared_ptr<LWindow> window,
                                         const std::string &eventType,
                                         void *message,
                                         lintptr *result)
{
    return window->nativeEvent(eventType, message, result);
}


bool LGuiApplication::sendWindowEventToPlatformWindow(std::shared_ptr<LWindow> window, std::shared_ptr<LEvent> event) {
    if (window == nullptr){
        return false;
    }
    std::shared_ptr<LPlatformWindow> platformWindow = window->handle();
    if (!platformWindow) {
        return false;
    }
    // spontaneous events come from the platform integration already, we don't need to send the events back
    if (event->spontaneous()) {
        return false;
    }
    // let the platform window do any handling it needs to as well
    return platformWindow->windowEvent(event);

}


std::shared_ptr<LEventDispatcher> LGuiApplication::createEventDispatcher()
{
    return platformIntegration()->createEventDispatcher();
}

void LGuiApplication::eventDispatcherReady()
{
    platformIntegration()->initialize();
    if (!LGuiApplication::m_highDpiScalingUpdated){
        LHighDpiScaling::updateHighDpiScaling();
    }
}

void LGuiApplication::updateFocusObject(std::shared_ptr<LObject> object)
{
    std::shared_ptr<LPlatformInputCtx> inputContext = platformIntegration()->inputContext();
    const bool enabled = (inputContext!=nullptr) && LInputMethod::objectAcceptsInputMethod(object);

    LPlatformInputCtx::setInputMethodAccepted(enabled);
    if (inputContext != nullptr){
        inputContext->setFocusObject(object); //根据可编辑性使能输入法
    }
}

void LGuiApplication::updateFilteredScreenOrientation(std::shared_ptr<LScreen> s)
{
    LScreenOrientation o = s->m_orientation;
    if (o == LScreenOrientation::Primary){
        o = s->m_primaryOrientation;
    }
    o = (LScreenOrientation) ((s->orientationUpdateMask() & o).value());
    if (o == LScreenOrientation::Primary){
        return;
    }
    if (o == s->m_filteredOrientation){
        return;
    }
    s->m_filteredOrientation = o;
    reportScreenOrientationChange(s);
}

void LGuiApplication::resetCachedDevicePixelRatio()
{
    m_maxDevicePixelRatio = 0.0;
}

const std::shared_ptr<LFont> LGuiApplication::font()
{
    if(m_font == nullptr){
        m_font = LFontDatabase::systemFont();
    }
    return m_font;
}

void LGuiApplication::setFont(const std::shared_ptr<LFont> f)
{
    m_font = f;
}

void LGuiApplication::setQuitOnLastWindowClosed(bool quit)
{
    LCoreApplication::setQuitLockEnabled(quit);
}

bool LGuiApplication::quitOnLastWindowClosed()
{
    return LCoreApplication::isQuitLockEnabled();
}

void LGuiApplication::setLayoutDirection(LLayoutDirection direction)
{
    if (m_layoutDirection == direction || direction == LLayoutDirection::Auto)
        return;

    m_layoutDirection = direction;

    //Todo
//    std::shared_ptr<LGuiApplication> gui = guiShare();
//    if (gui != nullptr) {
//        gui->layoutDirectionChanged.emit(direction);
//        gui->notifyLayoutDirectionChange();
//    }
}

LLayoutDirection LGuiApplication::layoutDirection()
{
    return m_layoutDirection == LLayoutDirection::Auto ? LLayoutDirection::LeftToRight : m_layoutDirection;
}

double LGuiApplication::devicePixelRatio()
{
    if (!lFuzzyIsZero(LGuiApplication::m_maxDevicePixelRatio)){
        return LGuiApplication::m_maxDevicePixelRatio;
    }

    LGuiApplication::m_maxDevicePixelRatio = 1.0; // make sure we never return 0.
    for (const std::shared_ptr<LScreen>& screen : m_screens){
        LGuiApplication::m_maxDevicePixelRatio = std::max<double>(LGuiApplication::m_maxDevicePixelRatio, screen->devicePixelRatio());
    }
    return LGuiApplication::m_maxDevicePixelRatio;
}

const LFlags<LKeyModifier>& LGuiApplication::keyboardModifiers()
{
    return m_modifierButtons;
}

const LFlags<LMouseButton>& LGuiApplication::mouseButtons()
{
    return m_mouseButtons;
}

std::list<uint32_t> LGuiApplication::possibleKeys(std::shared_ptr<LKeyEvent> e)
{
    std::list<uint32_t>  result;

    if (!e->nativeScanCode()) {
        if (e->key() && (e->key() != (uint32_t)LKey::Unknown)){
            result.push_back(e->key() + (uint32_t)e->modifiers());
        }
        else if (!e->text().empty())
            result.push_back((uint32_t)e->text().at(0) + (uint32_t)e->modifiers());
        return result;
    }else{

        result = platformIntegration()->possibleKeys(e);
        if (!result.empty()){
            return result;
        }

        if (e->key() && (e->key() != (uint32_t)LKey::Unknown)){
            result.push_back(e->key() + (uint32_t)e->modifiers());
        }
        else if (!e->text().empty()){
            result.push_back((uint32_t)e->text().at(0) + (uint32_t)e->modifiers());
        }
        return result;
    }
}

std::shared_ptr<LClipboard> LGuiApplication::clipboard()
{
    if(m_clipboard == nullptr){
        m_clipboard = makeInit<LClipboard>();
    }
    return m_clipboard;
}


void LGuiApplication::reportScreenOrientationChange(std::shared_ptr<LScreen> s)
{
    //emit s->orientationChanged(s->orientation());

    std::shared_ptr<LScreenOrientationChangeEvent> event
        = std::make_shared<LScreenOrientationChangeEvent>(s, s->m_orientation);
    LCoreApplication::sendEvent(lAppShare, event);
}

void LGuiApplication::captureGlobalModifierState(std::shared_ptr<LEvent> e)
{
    if (e->spontaneous()) {
        switch (e->type()) {
        case LEvent::MouseButtonPress:
        case LEvent::MouseButtonDblClick: {
            std::shared_ptr<LMouseEvent> me = std::dynamic_pointer_cast<LMouseEvent>(e);
            LGuiApplication::m_modifierButtons = me->modifiers();
            LGuiApplication::m_mouseButtons |= me->button();
            break;
        }
        case LEvent::MouseButtonRelease: {
            std::shared_ptr<LMouseEvent> me = std::dynamic_pointer_cast<LMouseEvent>(e);
            LGuiApplication::m_modifierButtons = me->modifiers();
            LGuiApplication::m_mouseButtons &= ~uint32_t(me->button());
            break;
        }
        case LEvent::KeyPress:
        case LEvent::KeyRelease:
        case LEvent::MouseMove:
        case LEvent::TouchBegin:
        case LEvent::TouchUpdate:
        case LEvent::TouchEnd:
        {
            std::shared_ptr<LInputEvent> ie = std::dynamic_pointer_cast<LInputEvent>(e);
            LGuiApplication::m_modifierButtons = ie->modifiers();
            break;
        }
        default:
            break;
        }
    }
}

void LGuiApplication::insertWin(std::shared_ptr<LWindow> w)
{
    if (w == nullptr){
        return;
    }
    //应该不会重复，就不比了
//    auto it = std::find_if(m_windows.begin(),
//                           m_windows.end(),
//                           [w](std::weak_ptr<LWindow> w) { return w.lock() == w; });
//    if (it == m_windows.end())
//    {
    m_windows.push_back(w);
//    }
}

void LGuiApplication::removeWin(std::shared_ptr<LWindow> w)
{
    auto isWin = [w](std::weak_ptr<LWindow> ww) { return ww.lock() == w; };
    m_windows.remove_if(isWin);
    if(!LGuiApplication::m_isAppClosing){
        LGuiApplication *gui = guiInst();
        if(gui != nullptr ){
            gui->m_modelWindows.remove_if(isWin);
        }
    }
}

//void LGuiApplication::removeScreen(std::shared_ptr<LScreen> w)
//{
//    m_screens.remove(w);
//}

std::shared_ptr<LInputMethod> LGuiApplication::inputMethod()
{
    if(guiInst()->m_inputMethod == nullptr){
        guiInst()->m_inputMethod = std::make_shared<LInputMethod>();
    }
    return guiInst()->m_inputMethod;
}

LApplicationState LGuiApplication::applicationState()
{
    return m_applicationState;
}

void LGuiApplication::setApplicationState(LApplicationState state, bool forcePropagate)
{
    if ((m_applicationState == state) && !forcePropagate){
        return;
    }

    m_applicationState = state;

    std::shared_ptr<LGuiApplication>  app = lGuiShare;
    switch (state) {
    case LApplicationState::Active: {
        std::shared_ptr<LEvent> appActivate = std::make_shared<LEvent>(LEvent::ApplicationActivate);
        LCoreApplication::sendSpontaneousEvent(app, appActivate);
        break; }
    case LApplicationState::Inactive: {
        std::shared_ptr<LEvent> appDeactivate = std::make_shared<LEvent>(LEvent::ApplicationDeactivate);
        LCoreApplication::sendSpontaneousEvent(app, appDeactivate);
        break;
    }
    default:
        break;
    }

    std::shared_ptr<LEvent> event =
        std::make_shared<LApplicationStateChangeEvent>(m_applicationState);
    LCoreApplication::sendSpontaneousEvent(app, event);
    LGuiApplication::platformIntegration()->applicationStateChanged(m_applicationState);
}

void LGuiApplication::processWindowSystemEvent(std::shared_ptr<LWinSysEvent> e)
{
    switch(e->m_type) {
    case LWinSysEvent::Mouse:{
        std::shared_ptr<wse::LMouseEvent> te = std::dynamic_pointer_cast<wse::LMouseEvent>(e);
        LGuiApplication::processMouseEvent(te);
        break;
    }
    case LWinSysEvent::Wheel:{
        std::shared_ptr<wse::LWheelEvent> te = std::dynamic_pointer_cast<wse::LWheelEvent>(e);
        LGuiApplication::processWheelEvent(te);
        break;
    }
    case LWinSysEvent::Key:{
        std::shared_ptr<wse::LKeyEvent> te = std::dynamic_pointer_cast<wse::LKeyEvent>(e);
        LGuiApplication::processKeyEvent(te);
        break;
    }
    case LWinSysEvent::Touch:{
//        std::shared_ptr<wse::LTouchEvent> te = std::dynamic_pointer_cast<wse::TouchEvent>(e);
//        LGuiApplication::processTouchEvent(te);
        break;
    }
    case LWinSysEvent::GeometryChange:{
        std::shared_ptr<wse::LGeometryChangeEvent> te = std::dynamic_pointer_cast<wse::LGeometryChangeEvent>(e);
        LGuiApplication::processGeometryChangeEvent(te);
        break;
    }
    case LWinSysEvent::Enter:{
        std::shared_ptr<wse::LEnterEvent> te = std::dynamic_pointer_cast<wse::LEnterEvent>(e);
        LGuiApplication::processEnterEvent(te);
        break;
    }
    case LWinSysEvent::Leave:{
        std::shared_ptr<wse::LWinEvent> te = std::dynamic_pointer_cast<wse::LWinEvent>(e);
        LGuiApplication::processLeaveEvent(te);
        break;
    }
    case LWinSysEvent::ActivatedWindow:{
        std::shared_ptr<wse::LActivatedWindowEvent> te = std::dynamic_pointer_cast<wse::LActivatedWindowEvent>(e);
        LGuiApplication::processActivatedEvent(te);
        break;
    }
    case LWinSysEvent::WindowStateChanged:{
        std::shared_ptr<wse::LWindowStateChangedEvent> te = std::dynamic_pointer_cast<wse::LWindowStateChangedEvent>(e);
        LGuiApplication::processWindowStateChangedEvent(te);
        break;
    }
    case LWinSysEvent::WindowScreenChanged:{
        std::shared_ptr<wse::LWindowScreenChangedEvent> te = std::dynamic_pointer_cast<wse::LWindowScreenChangedEvent>(e);
        LGuiApplication::processWindowScreenChangedEvent(te);
        break;
    }
    case LWinSysEvent::SafeAreaMarginsChanged:{
        std::shared_ptr<wse::LWinEvent> te = std::dynamic_pointer_cast<wse::LWinEvent>(e);
        LGuiApplication::processSafeAreaMarginsChangedEvent(te);
        break;
    }
    case LWinSysEvent::ApplicationStateChanged: {
        std::shared_ptr<wse::LApplicationStateChangedEvent> te = std::dynamic_pointer_cast<wse::LApplicationStateChangedEvent>(e);
        LGuiApplication::setApplicationState(te->m_newState, te->m_forcePropagate);
    }
    break;
    case LWinSysEvent::ApplicationTermination:{
        LGuiApplication::processApplicationTermination(e);
        break;
    }
    case LWinSysEvent::FlushEvents: {
        LWindowSystemInterface::deferredFlushWinSysEvents();
        break;
    }
    case LWinSysEvent::Close:{
        std::shared_ptr<wse::LWinEvent> te = std::dynamic_pointer_cast<wse::LWinEvent>(e);
        LGuiApplication::processCloseEvent(te);
        break;
    }
    case LWinSysEvent::ScreenOrientation:{
        std::shared_ptr<wse::LScreenOrientationEvent> te =
            std::dynamic_pointer_cast<wse::LScreenOrientationEvent>(e);
        LGuiApplication::processScreenOrientationChange(te);
        break;
    }
    case LWinSysEvent::ScreenGeometry:{
        std::shared_ptr<wse::LScreenGeometryEvent> te =
            std::dynamic_pointer_cast<wse::LScreenGeometryEvent>(e);
        LGuiApplication::processScreenGeometryChange(te);
        break;
    }
    case LWinSysEvent::ScreenLogicalDotsPerInch:{
        std::shared_ptr<wse::LScreenLogicalDotsPerInchEvent> te =
            std::dynamic_pointer_cast<wse::LScreenLogicalDotsPerInchEvent>(e);
        LGuiApplication::processScreenLogicalDotsPerInchChange(te);
        break;
    }
    case LWinSysEvent::ScreenRefreshRate:{
        std::shared_ptr<wse::LScreenRefreshRateEvent> te =
            std::dynamic_pointer_cast<wse::LScreenRefreshRateEvent>(e);
        LGuiApplication::processScreenRefreshRateChange(te);
        break;
    }
    case LWinSysEvent::ThemeChange:{
        std::shared_ptr<wse::LWinEvent> te = std::dynamic_pointer_cast<wse::LWinEvent>(e);
        LGuiApplication::processThemeChanged(te);
        break;
    }
    case LWinSysEvent::Expose:{
        std::shared_ptr<wse::LExposeEvent> te = std::dynamic_pointer_cast<wse::LExposeEvent>(e);
        LGuiApplication::processExposeEvent(te);
        break;
    }
    case LWinSysEvent::Tablet:{
        std::shared_ptr<wse::LTabletEvent> te = std::dynamic_pointer_cast<wse::LTabletEvent>(e);
        LGuiApplication::processTabletEvent(te);
        break;
    }
    case LWinSysEvent::TabletEnterProximity:{
        std::shared_ptr<wse::LTabletProximityEvent> te = std::dynamic_pointer_cast<wse::LTabletProximityEvent>(e);
        LGuiApplication::processTabletEnterProximityEvent(te);
        break;
    }
    case LWinSysEvent::TabletLeaveProximity:{
        std::shared_ptr<wse::LTabletProximityEvent> te = std::dynamic_pointer_cast<wse::LTabletProximityEvent>(e);
        LGuiApplication::processTabletLeaveProximityEvent(te);
        break;
    }
//    case LWinSysEvent::Gesture:{
//        std::shared_ptr<wse::LGestureEvent> te = std::dynamic_pointer_cast<wse::LGestureEvent>(e);
//        LGuiApplication::processGestureEvent(te);
//        break;
//    }
    case LWinSysEvent::PlatformPanel:{
        std::shared_ptr<wse::LWinEvent> te = std::dynamic_pointer_cast<wse::LWinEvent>(e);
        LGuiApplication::processPlatformPanelEvent(te);
        break;
    }
//    case LWinSysEvent::FileOpen:
//        LGuiApplication::processFileOpenEvent(
//            static_cast<QWindowSystemInterfacePrivate::FileOpenEvent *>(e));
//        break;
    case LWinSysEvent::ContextMenu:{
        std::shared_ptr<wse::LContextMenuEvent> te = std::dynamic_pointer_cast<wse::LContextMenuEvent>(e);
        LGuiApplication::processContextMenuEvent(te);
        break;
    }
    case LWinSysEvent::EnterWhatsThisMode:{
        std::shared_ptr<LEvent> evt = std::make_shared<LEvent>(LEvent::EnterWhatsThisMode);
        std::shared_ptr<LObject> obj = LGuiApplication::instance()->objPtr();
        LGuiApplication::postEvent(obj, evt);
        break;
    }
    default:{
        lWarn() << "Unknown user input event type:" << e->m_type;
        break;
    }
    }

}

void LGuiApplication::processMouseEvent(std::shared_ptr<wse::LMouseEvent> e)
{
    LEvent::Type type = LEvent::None;
    LMouseButton button = LMouseButton::No;
    std::shared_ptr<LWindow> window = e->m_window;
    bool positionChanged = LGuiApplication::m_lastCursorPosition != e->m_globalPos;
    bool mouseMove = false;
    bool mousePress = false;
    bool doubleClick = false;

    if (std::isnan(e->m_globalPos.m_x) || std::isnan(e->m_globalPos.m_y)) {
        return;
    }

    if (e->m_buttonType != LEvent::None) {
        type = e->m_buttonType;
        button = e->m_button;

        if (type == LEvent::NonClientAreaMouseMove || type == LEvent::MouseMove){
            mouseMove = true;
        }
        else if (type == LEvent::NonClientAreaMouseButtonPress || type == LEvent::MouseButtonPress){
            mousePress = true;
        }else if(type == LEvent::NonClientAreaMouseButtonDblClick || type == LEvent::MouseButtonDblClick){
            doubleClick = true;
        }

        if (!mouseMove && positionChanged) {
            std::shared_ptr<wse::LMouseEvent> moveEvent
                = std::make_shared<wse::LMouseEvent>(window, e->m_timestamp,
                                                    e->m_localPos, e->m_globalPos,
                                                    e->m_buttons ^ button,
                                                    e->m_modifiers, LMouseButton::No,
                                                    e->m_nonClientArea ? LEvent::NonClientAreaMouseMove : LEvent::MouseMove,
                                                    e->m_source, e->m_nonClientArea);
            if (e->synthetic()){
                moveEvent->m_flags |= LWinSysEvent::Synthetic;
            }
            processMouseEvent(moveEvent); // mouse move excluding state change
            processMouseEvent(e); // the original mouse event
            return;
        }
        if (mouseMove && !positionChanged) {
            // On Windows, and possibly other platforms, a touchpad can send a mouse move
            // that does not change position, between a press and a release. This may
            // confuse applications, so we always filter out these mouse events for
            // consistent behavior among platforms.
            return;
        }
    } else {
        LFlags<LMouseButton> stateChange = e->m_buttons ^ m_mouseButtons;
        if (positionChanged && (stateChange != LMouseButton::No)) {
            std::shared_ptr<wse::LMouseEvent> moveEvent
                = std::make_shared<wse::LMouseEvent>(window, e->m_timestamp, e->m_localPos,
                                                e->m_globalPos, m_mouseButtons, e->m_modifiers,
                                                LMouseButton::No, LEvent::None, e->m_source,
                                                e->m_nonClientArea);
            if (e->synthetic()){
                moveEvent->m_flags |= LWinSysEvent::Synthetic;
            }
            processMouseEvent(moveEvent); // mouse move excluding state change
            processMouseEvent(e); // the original mouse event
            return;
        }

        // In the compatibility path we deduce event type and button that caused the event
        if (positionChanged) {
            mouseMove = true;
            type = e->m_nonClientArea ? LEvent::NonClientAreaMouseMove : LEvent::MouseMove;
        } else {
            // Check to see if a new button has been pressed/released.
            for (uint32_t mask = (uint32_t)LMouseButton::Left; mask <= (uint32_t)LMouseButton::MaxMouse; mask <<= 1) {
                if (stateChange & mask) {
                    button = LMouseButton(mask);
                    break;
                }
            }
            if (button == LMouseButton::No) {
                // Ignore mouse events that don't change the current state. This shouldn't
                // really happen, getting here can only mean that the stored button state
                // is out of sync with the actual physical button state.
                return;
            }
            if (button & e->m_buttons) {
                mousePress = true;
                type = e->m_nonClientArea ? LEvent::NonClientAreaMouseButtonPress
                                        : LEvent::MouseButtonPress;
            } else {
                type = e->m_nonClientArea ? LEvent::NonClientAreaMouseButtonRelease
                                        : LEvent::MouseButtonRelease;
            }
        }
    }

    m_modifierButtons = e->m_modifiers;
    LPoint<double> localPoint = e->m_localPos;
    LPoint<double> globalPoint = e->m_globalPos;

    bool createDoubleClick = false;
    if (mouseMove) {
        LGuiApplication::m_lastCursorPosition = globalPoint;
        const auto doubleClickDistance = (e->m_source == LMouseEventSource::NotSynthesized) ?
                                             m_themeHints.mouseDoubleClickDistance() :
                                             m_themeHints.touchDoubleTapDistance();

        if (std::abs(globalPoint.m_x - m_mousePressX) > doubleClickDistance ||
            std::abs(globalPoint.m_y - m_mousePressY) > doubleClickDistance){
            m_mousePressButton = LMouseButton::No;
        }
    } else {
        m_mouseButtons = e->m_buttons;
        if (mousePress || doubleClick) {
            if(!doubleClick){
                int doubleClickInterval = m_themeHints.mouseDoubleClickInterval();
                doubleClick = e->m_timestamp - m_mousePressTime < doubleClickInterval && button == m_mousePressButton;
                createDoubleClick =  doubleClick;
            }
            m_mousePressTime = e->m_timestamp;
            m_mousePressButton = button;
            const LPoint<int> point = LGuiApplication::m_lastCursorPosition.toPoint();
            m_mousePressX = point.m_x;
            m_mousePressY = point.m_y;
        }
    }

    if (e->nullWindow()) {
        window = LGuiApplication::topLevelAt(globalPoint.toPoint());
        if (window) {
            // Moves and the release following a press must go to the same
            // window, even if the cursor has moved on over another window.
            std::shared_ptr<LWindow> currentMousePressWin = currentMousePressWindow();
            if (e->m_buttons != LMouseButton::No) {                            
                if (currentMousePressWin == nullptr){
                    m_currentMousePressWindow = window;
                }
                else{
                    window = currentMousePressWin;
                }
            } else if (currentMousePressWin != nullptr) {
                window = currentMousePressWin;
                m_currentMousePressWindow.reset();
            }
            LPoint<int> gIntPoint = globalPoint.toPoint();
            LPoint<double> delta = globalPoint - gIntPoint.toPointF();
            localPoint = window->mapFromGlobal(gIntPoint).toPointF() + delta;
        }
    }

    if (window == nullptr){
        return;
    }

    if(platformIntegration()->hasPointerEventCursor()){
        if (!e->synthetic()) {
            const std::shared_ptr<LScreen> screen = window->screen();
            if (screen != nullptr){
                std::shared_ptr<LPlatformCursor> cursor = screen->handle()->cursor();
                if (cursor != nullptr) {
                    const LPoint<double> nativeLocalPoint = LHighDpi::toNativePixels(localPoint, screen.get());
                    const LPoint<double> nativeGlobalPoint = LHighDpi::toNativePixels(globalPoint, screen.get());
                    std::shared_ptr<LMouseEvent> ev=
                                    std::make_shared<LMouseEvent>(type, nativeLocalPoint, nativeLocalPoint, nativeGlobalPoint,
                                                                  button, e->m_buttons, e->m_modifiers, e->m_source);
                    ev->setTimestamp(e->m_timestamp);
                    cursor->pointerEvent(ev);
                }
            }
        }
    }

    if (window->blockedByModalWindow() && !lGuiApp->popupActive()) {
        // a modal window is blocking this window, don't allow mouse events through
        return;
    }

    std::shared_ptr<LMouseEvent> ev
        = std::make_shared<LMouseEvent>(type, localPoint, localPoint, globalPoint, button,
                                        e->m_buttons, e->m_modifiers, e->m_source);
    ev->setTimestamp(e->m_timestamp);

    if (createDoubleClick && (ev->type() == LEvent::MouseButtonPress)) {
        // used to suppress delivery in lwidgetwindow.cpp
        setMouseEventFlags(ev.get(), ev->flags() | LMouseEventFlag::CreatedDoubleClick);
    }

    LGuiApplication::sendSpontaneousEvent(window, ev);
    e->m_eventAccepted = ev->isAccepted();
    //Todo handle touch event

    if (createDoubleClick && (ev->type() == LEvent::MouseButtonPress)) {
        m_mousePressButton = LMouseButton::No;
        if(window != nullptr){
            if (e->nullWindow()) {
                const LEvent::Type doubleClickType = e->m_nonClientArea ? LEvent::NonClientAreaMouseButtonDblClick : LEvent::MouseButtonDblClick;
                std::shared_ptr<LMouseEvent> dblClickEvent =
                    std::make_shared<LMouseEvent>(doubleClickType, localPoint, localPoint, globalPoint,
                                                  button, e->m_buttons, e->m_modifiers, e->m_source);
                dblClickEvent->setTimestamp(e->m_timestamp);
                LGuiApplication::sendSpontaneousEvent(window, dblClickEvent);
            }
        }
    }
}

void LGuiApplication::processWheelEvent(std::shared_ptr<wse::LWheelEvent> e)
{
    std::shared_ptr<LWindow> window = e->m_window;
    LPoint<double> globalPoint = e->m_globalPos;
    LPoint<double> localPoint = e->m_localPos;

    if (e->nullWindow()) {
        LPoint<int> gpInt = globalPoint.toPoint();
        window = LGuiApplication::topLevelAt(gpInt);
        if (window != nullptr) {
            LPoint<double> delta = globalPoint - gpInt.toPointF();
            localPoint = window->mapFromGlobal(gpInt).toPointF() + delta;
        }
    }

    if (window == nullptr){
        return;
    }

    LGuiApplication::m_lastCursorPosition = globalPoint;
    m_modifierButtons = e->m_modifiers;

    if (window->blockedByModalWindow() ) {
        // a modal window is blocking this window, don't allow wheel events through
        return;
    }

    std::shared_ptr<LWheelEvent> ev=
            std::make_shared<LWheelEvent>(localPoint, globalPoint,
                                          e->m_pixelDelta, e->m_angleDelta,
                                          m_mouseButtons, e->m_modifiers, e->m_phase,
                                          e->m_inverted, e->m_source);
    ev->setTimestamp(e->m_timestamp);
    LGuiApplication::sendSpontaneousEvent(window, ev);
}

void LGuiApplication::processKeyEvent(std::shared_ptr<wse::LKeyEvent> e)
{
    std::shared_ptr<LWindow> window = e->m_window;
    m_modifierButtons = e->m_modifiers;
    if (e->nullWindow()){
        window = LGuiApplication::focusWindow();
    }

    if(window == nullptr){
        return;
    }

    if (e->m_keyType == LEvent::KeyPress) {
        if (LWindowSystemInterface::handleShortcutEvent(window, e->m_timestamp,
                                                        e->m_key, e->m_modifiers,
                                                        e->m_nativeScanCode, e->m_nativeVirtualKey,
                                                        e->m_nativeModifiers, e->m_text,
                                                        e->m_repeat, e->m_repeatCount)) {
            return;
        }
    }

    if (!window->blockedByModalWindow()){
        std::shared_ptr<LKeyEvent> ev =
            std::make_shared<LKeyEvent>(e->m_keyType, e->m_key, e->m_modifiers,
                     e->m_nativeScanCode, e->m_nativeVirtualKey, e->m_nativeModifiers,
                     e->m_text, e->m_repeat, e->m_repeatCount);
        ev->setTimestamp(e->m_timestamp);

        LGuiApplication::sendSpontaneousEvent(window, ev);
        e->m_eventAccepted = ev->isAccepted();
    }
}

void LGuiApplication::processGeometryChangeEvent(std::shared_ptr<wse::LGeometryChangeEvent> e)
{
    if (e->m_window == nullptr){
        return;
    }

    std::shared_ptr<LWindow> window = e->m_window;
    const LRect<int> lastReportedGeometry = window->m_geometry;
    const LRect<int> requestedGeometry = e->m_requestedGeometry;
    const LRect<int> actualGeometry = e->m_newGeometry;

    // We send size and move events only if the geometry has changed from
    // what was last reported, or if the user tried to set a new geometry,
    // but the window manager responded by keeping the old geometry. In the
    // latter case we send move/resize events with the same geometry as the
    // last reported geometry, to indicate that the window wasn't moved or
    // resized. Note that this logic does not apply to the property changes
    // of the window, as we don't treat them as part of this request/response
    const bool isResize = actualGeometry.size() != lastReportedGeometry.size()
                          || requestedGeometry.size() != actualGeometry.size();
    const bool isMove = actualGeometry.topLeft() != lastReportedGeometry.topLeft()
                        || requestedGeometry.topLeft() != actualGeometry.topLeft();

    window->m_geometry = actualGeometry;

    if (isResize || window->m_resizeEventPending) {
        std::shared_ptr<LResizeEvent> e
            = std::make_shared<LResizeEvent>(actualGeometry.size(), lastReportedGeometry.size());
        LGuiApplication::sendSpontaneousEvent(window, e);

        window->m_resizeEventPending = false;
    }

    if (isMove) {
        //### frame geometry
        std::shared_ptr<LMoveEvent> e
            = std::make_shared<LMoveEvent>(actualGeometry.topLeft(), lastReportedGeometry.topLeft());
        LGuiApplication::sendSpontaneousEvent(window, e);
    }

}

void LGuiApplication::processEnterEvent(std::shared_ptr<wse::LEnterEvent> e)
{
    if (e->m_window == nullptr){
        return;
    }
    if (e->m_window->blockedByModalWindow()) {
        // a modal window is blocking this window, don't allow enter events through
        return;
    }

    m_currentMouseWindow = e->m_window;

    std::shared_ptr<LEnterEvent> event
        = std::make_shared<LEnterEvent>(e->m_localPos, e->m_localPos, e->m_globalPos);
    LCoreApplication::sendSpontaneousEvent(e->m_window, event);
}

void LGuiApplication::processLeaveEvent(std::shared_ptr<wse::LWinEvent> e)
{
    if (e->m_window == nullptr){
        return;
    }
    if (e->m_window->blockedByModalWindow()) {
        // a modal window is blocking this window, don't allow enter events through
        return;
    }

    m_currentMouseWindow.reset();

    std::shared_ptr<LEvent> event
        = std::make_shared<LEvent>(LEvent::Leave);
    LCoreApplication::sendSpontaneousEvent(e->m_window, event);
}

void LGuiApplication::processActivatedEvent(std::shared_ptr<wse::LActivatedWindowEvent> e)
{
    std::shared_ptr<LWindow> previous = LGuiApplication::m_focusWindow.lock();
    std::shared_ptr<LWindow> newFocus = e->m_window;

    if (previous == newFocus){
        return;
    }

    if (newFocus != nullptr){
        std::shared_ptr<LPlatformWindow> platformWindow = newFocus->handle();
        if (platformWindow != nullptr){
            if (platformWindow->isAlertState()){
                platformWindow->setAlertState(false);
            }
        }
    }

    std::shared_ptr<LObject> previousFocusObject = nullptr;
    if (previous != nullptr) {
        previousFocusObject = previous->focusObject();

        std::shared_ptr<LFocusEvent> focusAboutToChange
            = std::make_shared<LFocusEvent>(LEvent::FocusAboutToChange);
        LCoreApplication::sendSpontaneousEvent(previous, focusAboutToChange);
    }

    LGuiApplication::m_focusWindow = newFocus;
    if (lApp == nullptr){
        return;
    }

    if (previous != nullptr) {
        LFocusReason r = e->m_reason;
        if(newFocus != nullptr){
            if ((r == LFocusReason::Other || r == LFocusReason::ActiveWindow) &&
                (newFocus->flags() & LWindowType::Popup) == LWindowType::Popup){
                r = LFocusReason::Popup;
            }
        }

        std::shared_ptr<LFocusEvent> focusOut
            = std::make_shared<LFocusEvent>(LEvent::FocusOut, r);
        LCoreApplication::sendSpontaneousEvent(previous, focusOut);
        //previous->sigFocusObjectChanged.disconnect(lApp, &LGuiApplication::updateFocusObject);
    } else if (!platformIntegration()->hasCapability(LPlatformCapability::ApplicationState)) {
        setApplicationState(LApplicationState::Active);
    }

    if (newFocus != nullptr) {
        LFocusReason r = e->m_reason;
        if(previous != nullptr){
            if ((r == LFocusReason::Other || r == LFocusReason::ActiveWindow) &&
                (previous->flags() & LWindowType::Popup) == LWindowType::Popup){
                r = LFocusReason::Popup;
            }
        }
        std::shared_ptr<LFocusEvent> focusIn
            = std::make_shared<LFocusEvent>(LEvent::FocusIn, r);
        LCoreApplication::sendSpontaneousEvent(newFocus, focusIn);
        //newFocus->sigFocusObjectChanged.connect(lApp, &LGuiApplication::updateFocusObject);
    } else if (!platformIntegration()->hasCapability(LPlatformCapability::ApplicationState)) {
        setApplicationState(LApplicationState::Inactive);
    }

    if (lGuiApp != nullptr) {
        lGuiApp->notifyActiveWindowChange(previous);

        if (previousFocusObject != lGuiApp->focusObject()){
            lGuiApp->updateFocusObject(lGuiApp->focusObject());
        }
    }

    lGuiApp->sigFocusWindowChanged.emt(newFocus);

}

void LGuiApplication::processWindowStateChangedEvent(std::shared_ptr<wse::LWindowStateChangedEvent> e)
{
    if (e->m_window != nullptr) {
        std::shared_ptr<LWindowStateChangeEvent> te
            = std::make_shared<LWindowStateChangeEvent>(e->m_oldState);
        e->m_window->m_windowState = e->m_newState;
        LGuiApplication::sendSpontaneousEvent(e->m_window, te);
    }
}

void LGuiApplication::processWindowScreenChangedEvent(std::shared_ptr<wse::LWindowScreenChangedEvent> wse)
{
    std::shared_ptr<LWindow> window  = wse->m_window;
    if (wse->m_window != nullptr) {
        if (wse->m_window->screen() == wse->screen){
            return;
        }

        std::shared_ptr<LWindow> topLevelWindow = wse->m_window->topLevelWindow(LWindow::ExcludeTransients);
        if (topLevelWindow != nullptr) {
            if (wse->screen != nullptr){
                topLevelWindow->setTopLevelScreen(wse->screen);
            }else{ // Fall back to default behavior, and try to find some appropriate screen
                topLevelWindow->setScreen(nullptr);
            }
        }
        // we may have changed scaling, so trigger resize event if needed
        if (window->handle()) {
            std::shared_ptr<wse::LGeometryChangeEvent> gce
                = std::make_shared<wse::LGeometryChangeEvent>(window,
                                                              LHighDpi::fromNativePixels(window->handle()->geometry(), window.get()));
            processGeometryChangeEvent(gce);
        }
    }
}

void LGuiApplication::processSafeAreaMarginsChangedEvent(std::shared_ptr<wse::LWinEvent> e)
{
    if (e->m_window == nullptr){
        return;
    }
    e->m_window->processSafeAreaMarginsChanged();
}

void LGuiApplication::processApplicationTermination(std::shared_ptr<LWinSysEvent> e)
{
    std::shared_ptr<LEvent> event = std::make_shared<LEvent>(LEvent::Quit);
    LGuiApplication::sendSpontaneousEvent(LGuiApplication::instShare(), event);
    e->m_eventAccepted = event->isAccepted();
}

void LGuiApplication::processCloseEvent(std::shared_ptr<wse::LWinEvent> e)
{
    if (e->m_window == nullptr){
        return;
    }
    if (e->m_window->blockedByModalWindow()) {
        // a modal window is blocking this window, don't allow close events through
        return;
    }

    std::shared_ptr<LEvent> event = std::make_shared<LEvent>(LEvent::Close);
    LGuiApplication::sendSpontaneousEvent(e->m_window, event);
    e->m_eventAccepted = event->isAccepted();

}

void LGuiApplication::processScreenOrientationChange(std::shared_ptr<wse::LScreenOrientationEvent> e)
{
    if (startingUp()){
        return;
    }

    if (e->m_screen == nullptr){
        return;
    }

    e->m_screen->m_orientation = e->m_orientation;
    updateFilteredScreenOrientation(e->m_screen);

}

void LGuiApplication::processScreenGeometryChange(std::shared_ptr<wse::LScreenGeometryEvent> e)
{
    // This operation only makes sense after the QGuiApplication constructor runs
    if (startingUp()){
        return;
    }

    if (e->m_screen == nullptr){
        return;
    }

    std::shared_ptr<LScreen> s = e->m_screen;

    bool geometryChanged = e->m_geometry != s->m_geometry;
    s->m_geometry = e->m_geometry;

    bool availableGeometryChanged = e->m_availableGeometry != s->m_availableGeometry;
    s->m_availableGeometry = e->m_availableGeometry;

    const LScreenOrientation primaryOrientation = s->m_primaryOrientation;
    if (geometryChanged){
        s->updatePrimaryOrientation();
    }

    s->emitGeometryChangeSignals(geometryChanged, availableGeometryChanged);

    if (geometryChanged) {
        s->sigPhysicalSizeChanged.emt(s->physicalSize());
        s->sigLogicalDotsPerInchChanged.emt(s->logicalDotsPerInch());

        if (s->m_primaryOrientation != primaryOrientation){
            s->sigPrimaryOrientationChanged.emt(s->m_primaryOrientation);
        }

        if (s->m_orientation == LScreenOrientation::Primary){
            updateFilteredScreenOrientation(s);
        }
    }

    resetCachedDevicePixelRatio();
}

void LGuiApplication::processScreenLogicalDotsPerInchChange(std::shared_ptr<wse::LScreenLogicalDotsPerInchEvent> e)
{
    // This operation only makes sense after the QGuiApplication constructor runs
    if (startingUp()){
        return;
    }

    LHighDpiScaling::updateHighDpiScaling();

    if (e->m_screen == nullptr){
        return;
    }

    e->m_screen->m_logicalDpi = std::pair<double, double>(e->m_dpiX, e->m_dpiY);
    e->m_screen->sigLogicalDotsPerInchChanged.emt(e->m_screen->logicalDotsPerInch());
    e->m_screen->updateGeometriesWithSignals();
    resetCachedDevicePixelRatio();
}

void LGuiApplication::processScreenRefreshRateChange(std::shared_ptr<wse::LScreenRefreshRateEvent> e)
{
    // This operation only makes sense after the QGuiApplication constructor runs
    if (startingUp()){
        return;
    }

    if (e->m_screen == nullptr){
        return;
    }

    double rate = e->m_rate;
    // safeguard ourselves against buggy platform behavior...
    if (rate < 1.0){
        rate = 60.0;
    }
    if (!lFuzzyCompare(e->m_screen->m_refreshRate, rate)) {
        e->m_screen->m_refreshRate = rate;
        e->m_screen->sigRefreshRateChanged.emt(rate);
    }
}

void LGuiApplication::processThemeChanged(std::shared_ptr<wse::LWinEvent> e)
{
//    if (m_self){
//        m_self->notifyThemeChanged();
//    }
    if (e->m_window != nullptr) {
        std::shared_ptr<LEvent> le =
            std::make_shared<LEvent>(LEvent::ThemeChange);
        LCoreApplication::sendSpontaneousEvent(e->m_window, le);
    }
}

void LGuiApplication::processExposeEvent(std::shared_ptr<wse::LExposeEvent> e)
{
    if (e->m_window == nullptr){
        return;
    }

    std::shared_ptr<LWindow> w = e->m_window;

    if (!w->m_receivedExpose) {
        if (w->m_resizeEventPending) {
            // as a convenience for plugins, send a resize event before the first expose event if they haven't done so
            // window->geometry() should have a valid size as soon as a handle exists.
            std::shared_ptr<LResizeEvent> le
                = std::make_shared<LResizeEvent>(w->geometry().size(), w->m_geometry.size());
            LCoreApplication::sendSpontaneousEvent(w, le);
            w->m_resizeEventPending = false;
        }
        w->m_receivedExpose = true;
    }

    w->m_exposed = e->m_isExposed && (w->screen() != nullptr);
    std::shared_ptr<LExposeEvent> exposeEvent
        = std::make_shared<LExposeEvent>(e->m_region);
    LCoreApplication::sendSpontaneousEvent(w, exposeEvent);
}

void LGuiApplication::processTabletEvent(std::shared_ptr<wse::LTabletEvent> e)
{

}

void LGuiApplication::processTabletEnterProximityEvent(std::shared_ptr<wse::LTabletProximityEvent> e)
{

}

void LGuiApplication::processTabletLeaveProximityEvent(std::shared_ptr<wse::LTabletProximityEvent> e)
{

}

void LGuiApplication::processPlatformPanelEvent(std::shared_ptr<wse::LWinEvent> e)
{
    if (e->m_window == nullptr){
        return;
    }

    if (e->m_window->m_blockedByModalWindow) {
        // a modal window is blocking this window, don't allow events through
        return;
    }

    std::shared_ptr<LEvent> ev
        = std::make_shared<LEvent>(LEvent::PlatformPanel);
    LCoreApplication::sendSpontaneousEvent(e->m_window, ev);
}

void LGuiApplication::processContextMenuEvent(std::shared_ptr<wse::LContextMenuEvent> e)
{
    if (e->m_window == nullptr){
        return;
    }

    if (e->m_mouseTriggered || e->m_window->m_blockedByModalWindow){
        return;
    }


    std::shared_ptr<LContextMenuEvent> ev
        = std::make_shared<LContextMenuEvent>(LContextMenuEvent::Keyboard,
                                              e->m_pos,
                                              e->m_globalPos,
                                              e->m_modifiers);
    LCoreApplication::sendSpontaneousEvent(e->m_window, ev);
}



LK_NAMESPACE_END
