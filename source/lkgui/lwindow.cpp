#include "lwindow.h"
#include "lscreen.h"
#include "lguiapplication.h"
#include "lkcore/ldebug.h"
#include "lplatformwindow.h"
#include "lplatformintegration.h"
#include "lhighdpiscaling.h"

LK_NAMESPACE_BEGIN

static std::shared_ptr<LWindow> nonDesktopParent(std::shared_ptr<LWindow> parent)
{
    if (parent && parent->type() == LWindowType::Desktop) {
        return nullptr;
    }
    return parent;
}


LWindow::LWindow(): LSurface(LSurface::Window)
{
}

LWindow::~LWindow()
{
    LGuiApplication::removeWin(nullptr); //清除变空的weak_ptr
}

std::shared_ptr<LWindow> LWindow::fromWinId(WId id)
{
    if (!LGuiApplication::platformIntegration()->hasCapability(LPlatformCapability::ForeignWindows)) {
        lWarn() << "LWindow::fromWinId(): does not support foreign windows.";
        return nullptr;
    }

    std::shared_ptr<LWindow> window = makeInit<LWindow>();
    window->create(false, id);
    if (window->handle() == nullptr) {
        return nullptr;
    }
    return window;
}

void LWindow::setFlag(LWindowType flag, bool on)
{
    if (on){
        setFlags(m_windowFlags | flag);
    }
    else{
        setFlags(m_windowFlags & (~LFlags<LWindowType>(flag)));
    }
}

std::shared_ptr<LWindow> LWindow::parent(AncestorMode mode) const
{
    std::shared_ptr<LWindow> p = m_parentWindow.lock();
    if(p == nullptr){
        if(mode == IncludeTransients){
            p = m_transientParent.lock();
        }
    }
    return p;
}

void LWindow::setParent(std::shared_ptr<LWindow> parent)
{
    parent = nonDesktopParent(parent);

    if (m_parentWindow.lock() == parent){
        return;
    }

    std::shared_ptr<LScreen> newScreen = parent!=nullptr ? parent->screen() : screen();
    m_parentWindow = parent;

    if (parent){
        m_topLevelScreen.reset();
    }
    else{
        m_topLevelScreen = newScreen;
    }


    if (visible() && ((parent== nullptr) || (parent->handle()!=nullptr))){
        setVisible(true);
    }

    if (m_platformWindow) {
        if (parent){
            parent->create();
        }

        m_platformWindow->setParent(parent ? parent->m_platformWindow : nullptr);
    }

    std::shared_ptr<LWindow> tThis = getPtrT<LWindow>();
    LGuiApplication::updateBlockedStatus(tThis);

}

std::shared_ptr<LScreen> LWindow::screen() const
{
    std::shared_ptr<LWindow> p = m_parentWindow.lock();
    if(p != nullptr){
        return p->screen();
    }else{
        return m_topLevelScreen.lock();
    }
}

void LWindow::setScreen(std::shared_ptr<LScreen> screen)
{
    if (screen == nullptr){
        screen = LGuiApplication::primaryScreen();
    }
    setTopLevelScreen(screen);
}

std::shared_ptr<LPlatformWindow> LWindow::handle() const
{
    return m_platformWindow;
}

void LWindow::setVisible(bool isVisible)
{
    std::shared_ptr<LPlatformWindow> platWin = m_platformWindow;
    if (m_visible != isVisible) {
        m_visible = isVisible;
        //emit q->visibleChanged(visible);
        updateVisibility();
    } else if (platWin != nullptr) {
        return;
    }

    if (platWin != nullptr) {
        std::shared_ptr<LWindow> pWin = m_parentWindow.lock();
        if (pWin && pWin->handle() != nullptr){
            return;
        }

        // We only need to create the window if it's being shown
        if (m_visible){
            create();
        }
    }

    std::shared_ptr<LWindow> tThis = getPtrT<LWindow>();

    if (m_visible) {
        // remove posted quit events when showing a new window
        //LCoreApplication::removePostedEvents(qApp, LEvent::Quit);

        if (m_type == Window) {
            /*LGuiApplication *app_priv = LGuiApplication::guiInst();
            std::string &firstWindowTitle = app_priv->m_firstWindowTitle;
            if (!firstWindowTitle.isEmpty()) {
                setTitle(firstWindowTitle);
                firstWindowTitle = "";
            }
            if (!app_priv->m_forcedWindowIcon.isNull())
                setIcon(app_priv->m_forcedWindowIcon);

            // Handling of the -qwindowgeometry, -geometry command line arguments
            static bool geometryApplied = false;
            if (!geometryApplied) {
                geometryApplied = true;
                LGuiApplication::applyWindowGeometrySpecificationTo(q);
            }*/
        }

        std::shared_ptr<LEvent> showEvent = std::make_shared<LEvent>(LEvent::Show);
        LGuiApplication::sendEvent(tThis, showEvent);
    }

    if (isModal()) {
        if (m_visible){
            LGuiApplication::showModalWindow(tThis);
        }
        else{
            LGuiApplication::hideModalWindow(tThis);
        }
    } else if (m_visible && LGuiApplication::modalWindow()){
        LGuiApplication::updateBlockedStatus(tThis);
    }

    if (m_visible){
        applyCursor();
    }

    if (m_platformWindow != nullptr){
        m_platformWindow->setVisible(m_visible);
    }

    if (!m_visible) {
        std::shared_ptr<LEvent> hideEvent = std::make_shared<LEvent>(LEvent::Hide);
        LGuiApplication::sendEvent(tThis, hideEvent);
    }
}

void LWindow::create()
{
    create(false, 0);
}

bool LWindow::isExposed() const
{
    return m_exposed;
}

bool LWindow::isTopLevel() const
{
    return m_parentWindow.lock() == nullptr;
}

WId LWindow::winId()
{
    if(m_platformWindow == nullptr){
        create(false, 0);
    }
    return m_platformWindow->winId();
}

std::shared_ptr<LWindow> LWindow::topLevelWindow(AncestorMode mode) const
{
    std::shared_ptr<LWindow>  window = getPtrT<LWindow>();
    while (window != nullptr) {
        std::shared_ptr<LWindow> parent = window->parent(mode);
        if (parent== nullptr){
            break;
        }
        window = parent;
    }
    return window;
}

bool LWindow::isActive() const
{
    if (m_platformWindow == nullptr){
        return false;
    }

    std::shared_ptr<LWindow> focus = LGuiApplication::focusWindow();

    // Means the whole application lost the focus
    if (focus == nullptr){
        return false;
    }

    if (focus.get() == this){
        return true;
    }

    std::shared_ptr<LWindow> p = parent(IncludeTransients);
    if (p!= nullptr){
        return p->isActive();
    }else{
        return isAncestorOf(focus);
    }
}

void LWindow::setModality(LWindowModality modality)
{
    if (m_modality == modality){
        return;
    }
    m_modality = modality;
    //emit modalityChanged(modality);
}

const LRect<int> LWindow::geometry() const
{
    //std::shared_ptr<LPlatformWindow> m_plat = m_platformWindow;
    if (m_platformWindow != nullptr) {
        const LRect<int>& nativeGeometry = m_platformWindow->geometry();
        return isTopLevel()
                   ? LHighDpi::fromNativePixels(nativeGeometry, this)
                   : LHighDpi::fromNativeLocalPosition(nativeGeometry, this);
    }
    return m_geometry;
}

LMargins<int> LWindow::frameMargins() const
{
    if (m_platformWindow != nullptr){
        return LHighDpi::fromNativePixels(m_platformWindow->frameMargins(), this);
    }
    return LMargins<int>();
}

LRect<int> LWindow::frameGeometry() const
{
    if (m_platformWindow != nullptr) {
        LMargins<int> m = frameMargins();
        LRect<int>  t = LHighDpi::fromNativePixels(m_platformWindow->geometry(), this);
        return t.adjusted(-m.m_left, -m.m_top, m.m_right, m.m_bottom);
    }
    return m_geometry;
}

void LWindow::setPosition(const LPoint<int> &pt)
{
    setGeometry(LRect<int>(pt, size()));
}

void LWindow::setPosition(int posx, int posy)
{
    setGeometry(LRect<int>(posx, posy, size()));
}

void LWindow::setGeometry(const LRect<int> &rect)
{
    m_positionAutomatic = false;
    const LRect<int> oldRect = geometry();
    if (rect == oldRect){
        return;
    }

    m_positionPolicy = LWindow::WindowFrameExclusive;
    if (m_platformWindow != nullptr) {
        LRect<int> nativeRect;
        std::shared_ptr<LScreen> newScreen = screenForGeometry(rect);
        if (newScreen && isTopLevel()){
            nativeRect = LHighDpi::toNativePixels(rect, newScreen.get());
        }
        else{
            nativeRect = LHighDpi::toNativeLocalPosition(rect, newScreen.get());
        }
        m_platformWindow->setGeometry(nativeRect);
    } else {
        m_geometry = rect;

//        if (rect.x() != oldRect.x())
//            emit xChanged(rect.x());
//        if (rect.y() != oldRect.y())
//            emit yChanged(rect.y());
//        if (rect.width() != oldRect.width())
//            emit widthChanged(rect.width());
//        if (rect.height() != oldRect.height())
//            emit heightChanged(rect.height());
    }

}

bool LWindow::close()
{
    if (parent() != nullptr){
        return false;
    }

    if (m_platformWindow == nullptr){
        return true;
    }

    return m_platformWindow->close();
}

void LWindow::raise()
{
    //updateSiblingPosition(LWindow::PositionTop);

    if (m_platformWindow != nullptr){
        m_platformWindow->raise();
    }
}

void LWindow::lower()
{
    //updateSiblingPosition(LWindow::PositionBottom);
    if (m_platformWindow != nullptr){
        m_platformWindow->lower();
    }
}

LWindowState LWindow::windowState() const
{
    return effectiveState(m_windowState);
}

LFlags<LWindowState> LWindow::windowStates() const
{
    return m_windowState;
}

void LWindow::setWindowState(LWindowState state)
{
    setWindowStates(state);
}

void LWindow::setWindowStates(LFlags<LWindowState> state)
{
    if (state & LWindowState::Active) {
        lWarn() << "LWindow::setWindowStates does not accept LWindowState::Active";
        state &= ~uint8_t(LWindowState::Active);
    }

    if (m_platformWindow != nullptr){
        m_platformWindow->setWindowStates(state);
    }
    m_windowState = state;
    //emit windowStateChanged(effectiveState(m_windowState));
    updateVisibility();
}

void LWindow::setTransientParent(std::shared_ptr<LWindow> parent)
{
    if ((parent != nullptr) && !parent->isTopLevel()) {
        lWarn() << "must be a top level window.";
        return;
    }
    if (parent.get() == this) {
        lWarn() << "transient parent cannot be same as window";
        return;
    }

    m_transientParent = parent;
    std::shared_ptr<LWindow> q = std::dynamic_pointer_cast<LWindow>(objPtr());
    LGuiApplication::updateBlockedStatus(q);
    //emit transientParentChanged(parent);
}

std::shared_ptr<LWindow> LWindow::transientParent() const
{
    return m_transientParent.lock();
}

bool LWindow::isAncestorOf(const std::shared_ptr<LWindow> child, AncestorMode mode) const
{
    std::shared_ptr<LWindow> cp = child->parent();
    if (cp.get() == this){
        return true;
    }

    if(mode == IncludeTransients){
        cp = child->transientParent();
        if (cp.get() == this){
            return true;
        }
    }

    cp = child->parent(mode);
    if (cp != nullptr) {
        if (isAncestorOf(cp, mode)){
            return true;
        }
    } else{
        std::shared_ptr<LPlatformWindow>  ch = child->handle();
        if(m_platformWindow != nullptr && ch != nullptr){
            if (m_platformWindow->isAncestorOf(ch)){
                return true;
            }
        }
    }
    return false;
}

void LWindow::setOpacity(double level)
{
    if (level == m_opacity){
        return;
    }
    m_opacity = level;
    if (m_platformWindow != nullptr) {
        m_platformWindow->setOpacity(level);
        //emit opacityChanged(level);
    }
}

double LWindow::opacity() const
{
    return m_opacity;
}

void LWindow::setTitle(const std::wstring &title)
{
    if (m_title != title) {
        m_title = title;
    }

    if(m_platformWindow != nullptr){
        if (type() != LWindowType::Desktop){
            m_platformWindow->setWindowTitle(title);
        }
    }
}

LPoint<int> LWindow::mapToGlobal(const LPoint<int> &pos) const
{
    if (m_platformWindow != nullptr){
        if(m_platformWindow->isForeignWindow() || m_platformWindow->isEmbedded()) {
            return LHighDpi::fromNativeLocalPosition(m_platformWindow->mapToGlobal(LHighDpi::toNativeLocalPosition(pos, this)), this);
        }
    }

    if (LHighDpiScaling::isActive()){
        return LHighDpiScaling::mapPositionToGlobal(pos, globalPosition(), this);
    }
    return pos + globalPosition();
}

LPoint<int> LWindow::mapFromGlobal(const LPoint<int> &pos) const
{
    if (m_platformWindow != nullptr){
        if(m_platformWindow->isForeignWindow() || m_platformWindow->isEmbedded()) {
            return LHighDpi::fromNativeLocalPosition(m_platformWindow->mapFromGlobal(LHighDpi::toNativeLocalPosition(pos, this)), this);
        }
    }

    if (LHighDpiScaling::isActive()){
        return LHighDpiScaling::mapPositionFromGlobal(pos, globalPosition(), this);
    }
    return pos - globalPosition();

}

LPoint<int> LWindow::globalPosition() const
{
    LPoint<int> offset = position();
    for (std::shared_ptr<LWindow> p = parent(); p != nullptr; p = p->parent()) {
        std::shared_ptr<LPlatformWindow> pw = p->handle();
        if (pw!= nullptr){
            if(pw->isForeignWindow() || pw->isEmbedded()) {
            // Use mapToGlobal() for foreign windows
                offset += p->mapToGlobal(LPoint<int>(0, 0));
                break;
            }
        } else {
            offset += p->position();
        }
    }
    return offset;
}

void LWindow::setIcon(const std::shared_ptr<LTexture> icon)
{
    m_windowIcon = icon;
    if (m_platformWindow != nullptr){
        m_platformWindow->setWindowIcon(icon);
    }
    std::shared_ptr<LEvent> e= std::make_shared<LEvent>(LEvent::WindowIconChange);
    std::shared_ptr<LObject> tThis = objPtr();
    LCoreApplication::sendEvent(tThis, e);
}

std::shared_ptr<LTexture> LWindow::icon() const
{
    if (m_windowIcon == nullptr){
        return LGuiApplication::windowIcon();
    }
    return m_windowIcon;
}

void LWindow::requestActivate()
{
    if (m_windowFlags & LWindowType::WindowDoesNotAcceptFocus) {
        return;
    }
    if (m_platformWindow != nullptr){
        m_platformWindow->requestActivateWindow();
    }

}

void LWindow::setTopLevelScreen(std::shared_ptr<LScreen> newScreen)
{
    std::shared_ptr<LWindow> p = m_parentWindow.lock();
    if (p != nullptr) {
        lWarn() << "Attempt to set a screen on a child window.";
        return;
    }
    if (newScreen != m_topLevelScreen.lock()) {
        //Todo
        m_topLevelScreen = newScreen;
    }
}

void LWindow::updateVisibility()
{
    Visibility old = m_visibility;

    if (!m_visible){
        m_visibility = Hidden;
    }
    else if (m_windowState & LWindowState::Minimized){
        m_visibility = Minimized;
    }
    else if (m_windowState & LWindowState::FullScreen){
        m_visibility = FullScreen;
    }
    else if (m_windowState & LWindowState::Maximized){
        m_visibility = Maximized;
    }
    else{
        m_visibility = Windowed;
    }

    if (m_visibility != old){
        //emit q->visibilityChanged(visibility);
        //todo;
    }
}

void LWindow::create(bool recursive, WId nativeHandle)
{
    if (m_platformWindow != nullptr){
        return;
    }

    // avoid losing update requests when re-creating
    const bool needsUpdate = m_updateRequestPending;
    // the platformWindow, if there was one, is now gone, so make this flag reflect reality now
    m_updateRequestPending = false;

    std::shared_ptr<LWindow> p = m_parentWindow.lock();
    if (p != nullptr){
        p->create();
    }

    LPlatformIntegration *platformIntegration = LGuiApplication::platformIntegration();

    std::shared_ptr<LWindow> q = getPtrT<LWindow>();
    if(nativeHandle){
        //Todo
    }else{
        m_platformWindow = platformIntegration->createPlatformWindow(q);
    }

    if (m_platformWindow == nullptr) {
        lWarn() << "Failed to create platform window";
        return;
    }

    m_platformWindow->initialize();

    std::vector<std::weak_ptr<LWindow> > childWins = children();
    for (std::weak_ptr<LWindow> ch: childWins) {
        std::shared_ptr<LWindow> chWin = ch.lock();
        if(chWin == nullptr){
            continue;
        }

        if (recursive){
            chWin->create(recursive);
        }

        // The child may have had deferred creation due to this window not being created
        // at the time setVisible was called, so we re-apply the visible state, which
        // may result in creating the child, and emitting the appropriate signals.
        if (chWin->visible()){
            chWin->setVisible(true);
        }

        if (chWin->m_platformWindow!= nullptr){
            chWin->m_platformWindow->setParent(m_platformWindow);
        }
    }

//    LPlatformSurfaceEvent e(QPlatformSurfaceEvent::SurfaceCreated);
//    LGuiApplication::sendEvent(q, &e);

    if (needsUpdate){
//        requestUpdate();
    }
}

std::shared_ptr<LPlatformSurface> LWindow::surfaceHandle() const
{
    return m_platformWindow;
}

std::shared_ptr<LScreen> LWindow::screenForGeometry(const LRect<int> &newGeometry) const
{
    return screen();
//    std::shared_ptr<LScreen> currentScreen = screen();
//    std::shared_ptr<LScreen> fallback = currentScreen;
//    LPoint<int> center = newGeometry.center();
//    if (!parent() && currentScreen && !currentScreen->geometry().contains(center)) {
//        const auto screens = currentScreen->virtualSiblings();
//        for (std::shared_ptr<LScreen> screen : screens) {
//            if (screen->geometry().contains(center))
//                return screen;
//            if (screen->geometry().intersects(newGeometry))
//                fallback = screen;
//        }
//    }
    //    return fallback;
}

bool LWindow::applyCursor()
{
    std::shared_ptr<LScreen> s = screen();
    if (s!= nullptr) {
        const std::shared_ptr<LPlatformCursor> platformCursor = s->handle()->cursor();
        if (platformCursor != nullptr) {
            if (m_platformWindow== nullptr){
                return true;
            }

            std::shared_ptr<LWindow> tThis = getPtrT<LWindow>();
            platformCursor->changeCursor(cursor(), tThis);
            return true;
        }
    }
    return false;
}

void LWindow::maybeQuitOnLastWindowClosed()
{
    if (LCoreApplication::instance() == nullptr){
        return;
    }


    if (!isTopLevel()){
        return;
    }
    // Attempt to close the application only if this has WA_QuitOnClose set and a non-visible parent
    bool quitOnClose = LGuiApplication::quitOnLastWindowClosed() && (parent() == nullptr);
    std::vector<std::shared_ptr<LWindow> >  vec = LGuiApplication::topLevelWindows();
    bool lastWindowClosed = true;
    for (std::shared_ptr<LWindow> w: vec) {
        if (!w->visible() || w->transientParent() || w->type() == LWindowType::ToolTip){
            continue;
        }
        lastWindowClosed = false;
        break;
    }
    if (lastWindowClosed) {
        //LGuiApplication::emitLastWindowClosed();
        if (quitOnClose) {
           LCoreApplication::instance()->maybeQuit();
        }
    }
}

bool LWindow::handleEvent(std::shared_ptr<LEvent> ev)
{
    switch (ev->type()) {
    case LEvent::MouseMove:{
        std::shared_ptr<LMouseEvent> evt = std::dynamic_pointer_cast<LMouseEvent>(ev);
        mouseMoveEvent(evt);
        break;
    }
    case LEvent::MouseButtonPress:{
        std::shared_ptr<LMouseEvent> evt = std::dynamic_pointer_cast<LMouseEvent>(ev);
        mousePressEvent(evt);
        break;
    }
    case LEvent::MouseButtonRelease:{
        std::shared_ptr<LMouseEvent> evt = std::dynamic_pointer_cast<LMouseEvent>(ev);
        mouseReleaseEvent(evt);
        break;
    }
    case LEvent::MouseButtonDblClick:{
        std::shared_ptr<LMouseEvent> evt = std::dynamic_pointer_cast<LMouseEvent>(ev);
        mouseDoubleClickEvent(evt);
        break;
    }
/*    case LEvent::TouchBegin:
    case LEvent::TouchUpdate:
    case LEvent::TouchEnd:
    case LEvent::TouchCancel:{
        std::shared_ptr<LTouchEvent> evt = std::dynamic_pointer_cast<LTouchEvent>(ev);
        touchEvent(evt);
        break;
    }*/
    case LEvent::Move:{
        std::shared_ptr<LMoveEvent> evt = std::dynamic_pointer_cast<LMoveEvent>(ev);
        moveEvent(evt);
        break;
    }
    case LEvent::Resize:{
        std::shared_ptr<LResizeEvent> evt = std::dynamic_pointer_cast<LResizeEvent>(ev);
        resizeEvent(evt);
        break;
    }
    case LEvent::KeyPress:{
        std::shared_ptr<LKeyEvent> evt = std::dynamic_pointer_cast<LKeyEvent>(ev);
        keyPressEvent(evt);
        break;
    }
    case LEvent::KeyRelease:{
        std::shared_ptr<LKeyEvent> evt = std::dynamic_pointer_cast<LKeyEvent>(ev);
        keyReleaseEvent(evt);
        break;
    }
    case LEvent::FocusIn: {
        std::shared_ptr<LFocusEvent> evt = std::dynamic_pointer_cast<LFocusEvent>(ev);
        focusInEvent(evt);
        break;
    }
    case LEvent::FocusOut: {
        std::shared_ptr<LFocusEvent> evt = std::dynamic_pointer_cast<LFocusEvent>(ev);
        focusOutEvent(evt);
        break;
    }
    case LEvent::Wheel:{
        std::shared_ptr<LWheelEvent> evt = std::dynamic_pointer_cast<LWheelEvent>(ev);
        wheelEvent(evt);
        break;
    }
    case LEvent::Close:{
        if (ev->isAccepted()) {
            bool wasVisible = visible();
            destroy();
            if (wasVisible) {
                maybeQuitOnLastWindowClosed();
            }
        }
        break;
    }
    case LEvent::Expose:{
        std::shared_ptr<LExposeEvent> evt = std::dynamic_pointer_cast<LExposeEvent>(ev);
        exposeEvent(evt);
        break;
    }
    case LEvent::Show:{
        showEvent(ev);
        break;
    }
    case LEvent::Hide:{
        hideEvent(ev);
        break;
    }
    case LEvent::ApplicationWindowIconChange:{
        setIcon(icon());
        break;
    }
    case LEvent::WindowStateChange: {
//        emit windowStateChanged(effectiveState(m_windowState));
        updateVisibility();
        break;
    }

    case LEvent::PlatformSurface: {
        break;
    }
    default:
        return LObject::handleEvent(ev);
    }
    return true;
}

void LWindow::mouseMoveEvent(std::shared_ptr<LMouseEvent> ev)
{
    ev->ignore();
}

void LWindow::mousePressEvent(std::shared_ptr<LMouseEvent> ev)
{
    ev->ignore();
}

void LWindow::mouseReleaseEvent(std::shared_ptr<LMouseEvent> ev)
{
    ev->ignore();
}

void LWindow::mouseDoubleClickEvent(std::shared_ptr<LMouseEvent> ev)
{
    ev->ignore();
}

void LWindow::moveEvent(std::shared_ptr<LMoveEvent> ev)
{
    ev->ignore();
}

void LWindow::resizeEvent(std::shared_ptr<LResizeEvent> ev)
{
    ev->ignore();
}

void LWindow::keyPressEvent(std::shared_ptr<LKeyEvent> ev)
{
    ev->ignore();
}

void LWindow::keyReleaseEvent(std::shared_ptr<LKeyEvent> ev)
{
    ev->ignore();
}

void LWindow::focusInEvent(std::shared_ptr<LFocusEvent> ev)
{
    ev->ignore();
}

void LWindow::focusOutEvent(std::shared_ptr<LFocusEvent> ev)
{
    ev->ignore();
}

void LWindow::wheelEvent(std::shared_ptr<LWheelEvent> ev)
{
    ev->ignore();
}

void LWindow::exposeEvent(std::shared_ptr<LExposeEvent> ev)
{
    ev->ignore();
}

void LWindow::showEvent(std::shared_ptr<LEvent> ev)
{
    ev->ignore();
}

void LWindow::hideEvent(std::shared_ptr<LEvent> ev)
{
    ev->ignore();
}

LWindowState LWindow::effectiveState(LFlags<LWindowState> state)
{
    if (state & LWindowState::Minimized){
        return LWindowState::Minimized;
    }
    else if (state & LWindowState::FullScreen){
        return LWindowState::FullScreen;
    }
    else if (state & LWindowState::Maximized){
        return LWindowState::Maximized;
    }
    return LWindowState::NoState;
}

void LWindow::setFormat(const LSurfaceFormat &format)
{
    m_requestedFormat = format;
}

LSurfaceFormat LWindow::format() const
{
    if (m_platformWindow != nullptr){
        return m_platformWindow->format();
    }
    return m_requestedFormat;
}

LSurfaceFormat LWindow::requestedFormat() const
{
    return m_requestedFormat;
}

void LWindow::setSurfaceType(SurfaceType surfaceType)
{
    m_surfaceType = surfaceType;
}

LSurface::SurfaceType LWindow::surfaceType() const
{
    return m_surfaceType;
}

void LWindow::setMask(std::shared_ptr<LRegion> region)
{
    if (m_platformWindow != nullptr){
        std::shared_ptr<LRegion> t = LHighDpi::toNativeLocalRegion(region, this);
        return m_platformWindow->setMask(t);
    }
    m_mask = region;
}

std::shared_ptr<LRegion> LWindow::mask() const
{
    return m_mask;
}

void LWindow::resize(const LSize<int> &newSize)
{
    m_positionPolicy = LWindow::WindowFrameExclusive;
    if (m_platformWindow != nullptr) {
        m_platformWindow->setGeometry(LHighDpi::toNativePixels(LRect<int>(position(), newSize), this));
    } else {
//        const LSize<int> oldSize = m_geometry.size();
        m_geometry.setSize(newSize);
//        if (newSize.width() != oldSize.width()){
//            emit widthChanged(newSize.width());
//        }
//        if (newSize.height() != oldSize.height()){
//            emit heightChanged(newSize.height());
//        }
    }

}

void LWindow::destroy()
{
    if (m_platformWindow ==nullptr){
        return;
    }


    for (std::weak_ptr<LWindow> object: m_children) {
        std::shared_ptr<LWindow> w = object.lock();
        if (w != nullptr) {
            w->destroy();
        }
    }

    std::shared_ptr<LWindow> tThis = getPtrT<LWindow>();

    if (LGuiApplication::m_focusWindow.lock() == tThis){
        LGuiApplication::m_focusWindow.reset();
    }
    if (LGuiApplication::m_currentMouseWindow.lock() == tThis){
        LGuiApplication::m_currentMouseWindow.reset();
    }
    if (LGuiApplication::m_currentMousePressWindow.lock() == tThis){
        LGuiApplication::m_currentMousePressWindow.reset();
    }

//    for (int i = 0; i < LGuiApplication::m_tabletDevicePoints.size(); ++i)
//        if (LGuiApplication::m_tabletDevicePoints.at(i).target == this)
//            LGuiApplication::m_tabletDevicePoints[i].target = parent();
//      bool wasVisible = visible();
//      m_visibilityOnDestroy = wasVisible && (m_platformWindow != nullptr);
    setVisible(false);

    std::shared_ptr<LPlatformSurfaceEvent> e
        = std::make_shared<LPlatformSurfaceEvent>(LPlatformSurfaceEvent::SurfaceAboutToBeDestroyed);
    LGuiApplication::sendEvent(tThis, e);

    m_platformWindow = nullptr;

    m_resizeEventPending = true;
    m_receivedExpose = false;
    m_exposed = false;

}

double LWindow::devicePixelRatio() const
{
    if (m_platformWindow == nullptr){
        std::shared_ptr<LScreen> s =screen();
        if(s != nullptr){
            return s->devicePixelRatio();
        }
    }
    return m_platformWindow->devicePixelRatio() * LHighDpiScaling::factor(this);
}

bool LWindow::setKeyboardGrabEnabled(bool grab)
{
    if (m_platformWindow != nullptr){
        return m_platformWindow->setKeyboardGrabEnabled(grab);
    }
    return false;
}

bool LWindow::setMouseGrabEnabled(bool grab)
{
    if (m_platformWindow != nullptr){
        return m_platformWindow->setMouseGrabEnabled(grab);
    }
    return false;
}

void LWindow::init()
{
    initWin(nullptr);
}


bool LWindow::nativeEvent(const std::string &eventType, void *message, lintptr *result)
{
    return false;
}

LRect<double> LWindow::closestAcceptableGeometry(const LRect<double> &rect) const
{
    return LRect<double>();
}

bool LWindow::isWidgetWindow()
{
    return false;
}

const LCursor& LWindow::cursor() const
{
    if(m_cursor.isValid()){
        return m_cursor;
    }
    return LGuiApplication::overrideCursor();
}

void LWindow::setCursor(const LCursor &c)
{
    if(c != m_cursor){
        m_cursor = c;
        applyCursor();
    }
}

void LWindow::unsetCursor()
{
    if(m_cursor.isValid()){
        m_cursor.reset();
        applyCursor();
    }
}


void LWindow::initWin(std::shared_ptr<LScreen> targetScreen)
{
    std::shared_ptr<LWindow> p =m_parentWindow.lock();
    if (p ==nullptr){
        if(targetScreen == nullptr){
            targetScreen = LGuiApplication::primaryScreen();
        }
        m_topLevelScreen = targetScreen;
    }
    // If your application aborts here, you are probably creating a LWindow
    // before the screen list is populated.
    if ((p == nullptr) && (m_topLevelScreen.lock() == nullptr)) {
        lFatal() << "Cannot create window: no screens available";
        exit(1);
    }
    std::shared_ptr<LWindow> tThis = getPtrT<LWindow>();
    LGuiApplication::insertWin(tThis);
}

LK_NAMESPACE_END
