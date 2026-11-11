#include "lplatformwindow.h"
#include "lkgui/lscreen.h"
#include "lkgui/lguiapplication.h"
LK_NAMESPACE_BEGIN

static LSize<int> fixInitialSize(LSize<int> size, const std::shared_ptr<LWindow> w,
                   int defaultWidth, int defaultHeight)
{
    if (size.width() == 0) {
        const int minWidth = w->minimumWidth();
        size.setWidth(minWidth > 0 ? minWidth : defaultWidth);
    }
    if (size.height() == 0) {
        const int minHeight = w->minimumHeight();
        size.setHeight(minHeight > 0 ? minHeight : defaultHeight);
    }
    return size;
}


static inline const std::shared_ptr<LScreen> effectiveScreen(const std::shared_ptr<LWindow> window)
{
    if (window == nullptr){
        return LGuiApplication::primaryScreen();
    }
    const std::shared_ptr<LScreen> screen = window->screen();
    if (screen == nullptr){
        return LGuiApplication::primaryScreen();
    }
    return screen;
}

LPlatformWindow::LPlatformWindow(std::shared_ptr<LWindow> win):LPlatformSurface(win)
{
    m_rect = LHighDpi::toNativePixels(win->geometry(), win.get());
}

LPlatformWindow::~LPlatformWindow()
{

}

void LPlatformWindow::initialize()
{

}

bool LPlatformWindow::windowEvent(std::shared_ptr<LEvent> event) {
    return false;
}

LPoint<int> LPlatformWindow::mapToGlobal(const LPoint<int> &pos) const
{
    std::shared_ptr<const LPlatformWindow> p = getPtrT<LPlatformWindow>();
    LPoint<int> result = pos;
    while (p) {
        result += p->geometry().topLeft();
        p = p->parent();
    }
    return result;
}

LPoint<int> LPlatformWindow::mapFromGlobal(const LPoint<int> &pos) const
{
    std::shared_ptr<const LPlatformWindow> p = getPtrT<LPlatformWindow>();
    LPoint<int> result = pos;
    while (p) {
        result -= p->geometry().topLeft();
        p = p->parent();
    }
    return result;
}

void LPlatformWindow::requestActivateWindow()
{

}

void LPlatformWindow::propagateSizeHints()
{

}

LMargins<int> LPlatformWindow::frameMargins() const
{
    return LMargins<int>();
}


void LPlatformWindow::setFrameStrutEventsEnabled(bool enabled)
{

}

bool LPlatformWindow::frameStrutEventsEnabled() const
{
    return false;
}

bool LPlatformWindow::setWindowModified(bool modified)
{
    return false;
}

void LPlatformWindow::setMask(std::shared_ptr<LRegion> region)
{

}

void LPlatformWindow::setAlertState(bool enabled)
{

}

bool LPlatformWindow::isAlertState() const
{
    return false;
}

bool LPlatformWindow::close()
{
    return LWindowSystemInterface::handleCloseEvent(window(), LDeliverType::Sync);
}

void LPlatformWindow::raise()
{

}

void LPlatformWindow::lower()
{

}

std::shared_ptr<LScreen> LPlatformWindow::initialGeometry(const std::shared_ptr<LWindow> w,
                                            const LRect<int> &initGeo,
                                            int defaultWidth,
                                            int defaultHeight,
                                            LRect<int> &retRect)
{
    std::shared_ptr<LScreen> ret = w->screen();

    if (!w->isTopLevel()) {
        const double factor = LHighDpiScaling::factor(w.get());
        const LSize<int> size = fixInitialSize(LHighDpi::fromNative(initGeo.size(), factor),
                                          w, defaultWidth, defaultHeight);
        retRect = LRect<int>(initGeo.m_y, initGeo.m_x, LHighDpi::toNative(size, factor));
        return ret;
    }

    const bool position = w->m_positionAutomatic && w->type() != LWindowType::Popup;
    if (!position && !w->m_resizeAutomatic){
        retRect = initGeo;
        return ret;
    }
    const std::shared_ptr<LScreen> screen = w->m_positionAutomatic
                                        ? effectiveScreen(w)
                                        : LGuiApplication::screenAt(initGeo.center());
    if (screen == nullptr){
        retRect = initGeo;
        return ret;
    }

    ret = screen;
    // initialGeometry refers to window's screen
    LRect<int> rect(LHighDpi::fromNativePixels(initGeo, w.get()));
    if (w->m_resizeAutomatic){
        rect.setSize(fixInitialSize(rect.size(), w, defaultWidth, defaultHeight));
    }
    if (position) {
        const LRect<int> availableGeometry = screen->m_availableGeometry;
        // Center unless the geometry ( + unknown window frame) is too large for the screen).
        if (rect.height() < (availableGeometry.height() * 8) / 9
            && rect.width() < (availableGeometry.width() * 8) / 9) {
            const std::shared_ptr<LWindow> tp = w->transientParent();
            if (tp != nullptr) {
                // A transient window should be centered w.r.t. its transient parent.
                rect.moveCenter(tp->geometry().center());
            } else {
                // Center the window on the screen.  (Only applicable on platforms
                // which do not provide a better way.)
                rect.moveCenter(availableGeometry.center());
            }
        }
    }
    retRect = LHighDpi::toNativePixels(rect, screen.get());
    return ret;
}

LRect<double> LPlatformWindow::closestAcceptableGeometry(const std::shared_ptr<LWindow> w,
                                                          const LRect<double> &nativeRect)
{
    const LRect<double> rectF = LHighDpi::fromNativePixels(nativeRect, w.get());
    const LRect<double> correctedGeometryF = w->closestAcceptableGeometry(rectF);
    return !correctedGeometryF.isEmpty() && rectF != correctedGeometryF
               ? LHighDpi::toNativePixels(correctedGeometryF, w.get()) : nativeRect;
    return LRect<double>();
}

std::shared_ptr<LPlatformScreen> LPlatformWindow::screenForGeometry(const LRect<int> &newGeometry) const
{
    std::shared_ptr<LPlatformScreen>  currentScreen = screen();
    std::shared_ptr<LPlatformScreen>  fallback = currentScreen;
    // LRect::center can return a value outside the rectangle if it's empty.
    // Apply mapToGlobal() in case it is a foreign/embedded window.
    LPoint<int> center = newGeometry.isEmpty() ? newGeometry.topLeft() : newGeometry.center();
    if (isForeignWindow()){
        center = mapToGlobal(center - newGeometry.topLeft());
    }

//    if (!parent() && currentScreen && !currentScreen->geometry().contains(center)) {
//        const auto screens = currentScreen->virtualSiblings();
//        for (LPlatformScreen* screen : screens) {
//            const LRect<int> screenGeometry = screen->geometry();
//            if (screenGeometry.contains(center)){
//                return screen;
//            }
//            if (screenGeometry.intersects(newGeometry)){
//                fallback = screen;
//            }
//        }
//    }
    return fallback;

}

std::shared_ptr<LWindow> LPlatformWindow::window() const
{
    return std::dynamic_pointer_cast<LWindow>(m_surface.lock());
}

std::shared_ptr<LPlatformWindow> LPlatformWindow::parent() const
{
    std::shared_ptr<LWindow> win = window();
    if(win != nullptr){
        std::shared_ptr<LWindow> winP = win->parent();
        if(winP != nullptr){
            return winP->handle();
        }
    }
    return  nullptr;
}

std::shared_ptr<LPlatformScreen> LPlatformWindow::screen() const
{
    std::shared_ptr<LWindow> win = window();
    if(win != nullptr){
        std::shared_ptr<LScreen>  scr = win->screen();
        return scr ? scr->handle() : nullptr;
    }
    return nullptr;
}

LSurfaceFormat LPlatformWindow::format() const
{
    return LSurfaceFormat();
}

WId LPlatformWindow::winId() const
{
    return WId(1);
}

const LRect<int> &LPlatformWindow::geometry() const
{
    return m_rect;
}

void LPlatformWindow::setGeometry(const LRect<int> &rect)
{
    m_rect = rect;
}

LRect<int> LPlatformWindow::normalGeometry() const
{
    return LRect<int>();
}

double LPlatformWindow::devicePixelRatio() const
{
    return 1.0;
}

bool LPlatformWindow::isActive() const
{
    return false;
}

bool LPlatformWindow::isExposed() const
{
    std::shared_ptr<LWindow> win = window();
    if(win != nullptr){
        win->visible();
    }
    return false;
}

bool LPlatformWindow::isAncestorOf(const std::shared_ptr<LPlatformWindow> child) const
{
    for (std::shared_ptr<LPlatformWindow> parent = child->parent(); parent != nullptr; parent = parent->parent()) {
        if (parent.get() == this){
            return true;
        }
    }
    return false;
}

void LPlatformWindow::setParent(const std::shared_ptr<LPlatformWindow> window)
{
}

void LPlatformWindow::setWindowTitle(const std::wstring &title)
{

}

void LPlatformWindow::setWindowIcon(const std::shared_ptr<LTexture> icon)
{

}

void LPlatformWindow::setWindowFlags(LFlags<LWindowType> flags)
{

}

void LPlatformWindow::setWindowStates(LFlags<LWindowState> states)
{

}

void LPlatformWindow::setOpacity(double level)
{

}

bool LPlatformWindow::setMouseGrabEnabled(bool grab)
{
    return false;
}

bool LPlatformWindow::setKeyboardGrabEnabled(bool grab)
{
    return false;
}

void LPlatformWindow::setVisible(bool visible)
{
    LRect<int> rect(0, 0, geometry().size());
    std::shared_ptr<LWindow> w =window();
    std::shared_ptr<LRegion> r = std::make_shared<LRegion>(rect);
    LWindowSystemInterface::handleExposeEvent(w, r);
    LWindowSystemInterface::flushWinSysEvents();
}




LK_NAMESPACE_END
