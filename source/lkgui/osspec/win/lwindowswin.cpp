#include "lwindowswin.h"
#include "lwindowsctx.h"
#include "lkcore/lopersysversion.h"
#include "lkcore/ldebug.h"
#include "lkgui/lguiapplication.h"
#include "lkgui/osspec/win/lwindowsglhelper.h"

LK_NAMESPACE_BEGIN
bool LWindowsWin::m_screenForGLInitialized = false;

static inline bool equalDpi(const std::pair<double, double> &d1,
                            const std::pair<double, double> &d2)
{
    return lFuzzyCompare(d1.first, d2.first) && lFuzzyCompare(d1.second, d2.second);
}

static HICON createHIcon(const std::shared_ptr<LTexture> icon, int xSize, int ySize)
{
    if (icon != nullptr && !icon->isEmpty()) {
        const std::shared_ptr<LPixmap> pm = icon->getPixmap(icon->actualSize(LSize<int>(xSize, ySize)),
                                                            LTexture::Normal,
                                                            true);
        if (pm != nullptr && !pm->isNull()){
            return LWindowsWin::pixmapToWinHICON(pm);
        }
    }
    return nullptr;
}



static void setRestoreMaximizedFlag(HWND hwnd, bool set = true)
{
    // Let Windows know that we need to restore as maximized
    WINDOWPLACEMENT windowPlacement;
    windowPlacement.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(hwnd, &windowPlacement)) {
        if (set){
            windowPlacement.flags |= WPF_RESTORETOMAXIMIZED;
        }
        else{
            windowPlacement.flags &= ~WPF_RESTORETOMAXIMIZED;
        }
        SetWindowPlacement(hwnd, &windowPlacement);
    }
}

// Return the visibility of the Window (except full screen since it is not a window state).
static LWindow::Visibility windowVisibilitySys(HWND hwnd)
{
    if (!IsWindowVisible(hwnd)){
        return LWindow::Hidden;
    }
    WINDOWPLACEMENT windowPlacement;
    windowPlacement.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(hwnd, &windowPlacement)) {
        switch (windowPlacement.showCmd) {
        case SW_SHOWMINIMIZED:
        case SW_MINIMIZE:
        case SW_FORCEMINIMIZE:{
            return LWindow::Minimized;
        }
        case SW_SHOWMAXIMIZED:{
            return LWindow::Maximized;
        }
        default:
            break;
        }
    }
    return LWindow::Windowed;
}

static std::shared_ptr<LScreen> screenForName(const std::shared_ptr<LWindow> w, const std::string &name)
{
    std::shared_ptr<LScreen> winScreen = (w!=nullptr) ? w->screen() : LGuiApplication::primaryScreen();
//    if (winScreen && winScreen->name() != name) {
//        const auto screens = winScreen->virtualSiblings();
//        for (QScreen *screen : screens) {
//            if (screen->name() == name)
//                return screen;
//        }
//    }
    return winScreen;
}

static void setWindowOpacity(HWND hwnd,
                             LFlags<LWindowType> flags,
                             bool hasAlpha,
                             bool accelerated,
                             double level)
{
    if (LWindowsWin::setWindowLayered(hwnd, flags, hasAlpha, level)) {
        const BYTE alpha = BYTE(std::lround(255.0 * level));
        if (hasAlpha && !accelerated && (flags & LWindowType::FramelessWindowHint)) {
            // Non-GL windows with alpha: Use blend function to update.
            BLENDFUNCTION blend = {AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA};
            UpdateLayeredWindow(hwnd, nullptr, nullptr, nullptr, nullptr, nullptr, 0, &blend, ULW_ALPHA);
        } else {
            SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
        }
    } else if (IsWindowVisible(hwnd)) { // Repaint when switching from layered.
        InvalidateRect(hwnd, nullptr, TRUE);
    }
}


static LPoint<int> calcPosition(const std::shared_ptr<LWindow> w,
                            const std::shared_ptr<LWindowCreationCtx> context,
                            const LMargins<int> &invMargins)
{
    const LPoint<int> orgPos(context->frameX - invMargins.m_left, context->frameY - invMargins.m_top);

    if (!w || (!w->isTopLevel() && w->surfaceType() != LWindow::OpenGLSurface)){
        return orgPos;
    }

    const std::shared_ptr<LScreen> screenForGL = LWindowsWin::forcedScreenForGLWindow(w);
    if (screenForGL == nullptr){
        return orgPos;
    }

    const LPoint<int> posFrame(context->frameX, context->frameY);
    const LMargins<int> margins = context->margins;
    const LRect<int> scrGeo = screenForGL->handle()->availableGeometry();

    // Point is already in the required screen.
    if (scrGeo.contains(orgPos)){
        return orgPos;
    }

    // If the visible part of the window is already in the
    // required screen, just ignore the invisible offset.
    if (scrGeo.contains(posFrame)){
        return posFrame;
    }

    const std::shared_ptr<LScreen> orgScreen= screenForGL;
    // Find the original screen containing the coordinates.
//    const auto screens = screenForGL->virtualSiblings();
//    const QScreen *orgScreen = nullptr;
//    for (QScreen *screen : screens) {
//        if (screen->handle()->availableGeometry().contains(posFrame)) {
//            orgScreen = screen;
//            break;
//        }
//    }

    LPoint center = scrGeo.center();
    const LPoint<int> ctPos = LPoint<int>(std::max<int>(scrGeo.m_x, center.m_x
                                                        + (margins.m_right - margins.m_left - context->frameWidth)/2),
                                          std::max<int>(scrGeo.m_y, center.m_y
                                                       + (margins.m_bottom - margins.m_top - context->frameHeight)/2));

    // If initial coordinates were outside all screens, center the window on the required screen.
    if (orgScreen == nullptr)
        return ctPos;

    const LRect<int> orgGeo = orgScreen->handle()->availableGeometry();
    const LRect<int> orgFrame(context->frameX, context->frameY,
                              LSize<int>(context->frameWidth, context->frameHeight));

    // Window would be centered on orgScreen. Center it on the required screen.
    if (orgGeo.center() == orgFrame.marginsRemoved(margins).center()){
        return ctPos;
    }

    // Transform the coordinates to map them into the required screen.
    const LPoint<int> newPos(scrGeo.m_x + ((posFrame.m_x - orgGeo.m_x) * scrGeo.width()) / orgGeo.width(),
                        scrGeo.m_y + ((posFrame.m_y - orgGeo.m_y) * scrGeo.height()) / orgGeo.height());
    const LPoint<int> newPosNoMargin(newPos.m_x - invMargins.m_left, newPos.m_y - invMargins.m_top);

    return scrGeo.contains(newPosNoMargin) ? newPosNoMargin : newPos;
}


// Check whether to apply a new cursor. Either the window in question is
// currently under mouse, or it is the parent of the window under mouse and
// there is no other window with an explicitly set cursor in-between.
static inline bool canApplyNewCursor(const std::shared_ptr<LWindow> w)
{
    const std::shared_ptr<LWindow> underMouse = LWindowsCtx::instance()->windowUnderMouse();
    if (underMouse == w){
        return true;
    }
    for (std::shared_ptr<LWindow> p = underMouse; p!=nullptr ; p = p->parent()) {
        if (p == w){
            return true;
        }
        const std::shared_ptr<LWindowsWin> platformWindow = LWindowsWin::windowsWindowOf(p);
        if(platformWindow != nullptr){
            if (platformWindow->cursor() != nullptr){
                return false;
            }
        }
    }
    return false;
}

static std::shared_ptr<LCursorHandle> defaultCursor(const std::shared_ptr<LWindow> w)
{
    std::shared_ptr<LScreen> screen = w->screen();
    if (screen!= nullptr){
        std::shared_ptr<LPlatformScreen> ps = screen->handle();
        if (ps != nullptr ){
            std::shared_ptr<LWindowsCursor> cursor = std::dynamic_pointer_cast<LWindowsCursor>(ps->cursor());
            if (cursor != nullptr){
                return cursor->standardWindowCursor(LCursorShape::Arrow);
            }
        }
    }
    HCURSOR hc =LWindowsCursor::createCursorFromShape(LCursorShape::Arrow);
    std::shared_ptr<LCursorHandle> ret = std::make_shared<LCursorHandle>(hc);
    return ret;
}


LMargins<int> LWindowsGeometryHint::frameOnPrimaryScreen(DWORD style, DWORD exStyle)
{
    RECT rect = {0,0,0,0};
    style &= ~DWORD(WS_OVERLAPPED); // Not permitted, see docs.
    if (AdjustWindowRectEx(&rect, style, FALSE, exStyle) == FALSE){
        lFatal() << "AdjustWindowRectEx failed";
    }
    LMargins<int> result(std::abs(rect.left),
                       std::abs(rect.top),
                       std::abs(rect.right),
                       std::abs(rect.bottom));
    return result;
}

LMargins<int> LWindowsGeometryHint::frameOnPrimaryScreen(HWND hwnd)
{
    return frameOnPrimaryScreen(DWORD(GetWindowLongPtr(hwnd, GWL_STYLE)),
                                DWORD(GetWindowLongPtr(hwnd, GWL_EXSTYLE)));
}

LMargins<int> LWindowsGeometryHint::frame(DWORD style, DWORD exStyle, double dpi)
{
    if (LWindowsCtx::user32dll.adjustWindowRectExForDpi == nullptr){
        return frameOnPrimaryScreen(style, exStyle);
    }
    RECT rect = {0,0,0,0};
    style &= ~DWORD(WS_OVERLAPPED); // Not permitted, see docs.
    if (LWindowsCtx::user32dll.adjustWindowRectExForDpi(&rect, style, FALSE, exStyle,
                                                        unsigned(std::lround(dpi))) == FALSE) {
        lFatal()<<"AdjustWindowRectExForDpi failed";
    }
    LMargins<int> result(std::abs(rect.left),
                       std::abs(rect.top),
                       std::abs(rect.right),
                       std::abs(rect.bottom));
    return result;
}

LMargins<int> LWindowsGeometryHint::frame(HWND hwnd, DWORD style, DWORD exStyle)
{
    if (LWindowsScreenManager::isSingleScreen()){
        return frameOnPrimaryScreen(style, exStyle);
    }
    auto screenManager = LWindowsCtx::instance()->screenManager();
    auto screen = screenManager.screenForHwnd(hwnd);
    if (!screen){
        screen = screenManager.screens().at(0);
    }
    const auto dpi = screen ? screen->logicalDpi().first : double(96);
    return frame(style, exStyle, dpi);
}

LMargins<int> LWindowsGeometryHint::frame(const std::shared_ptr<LWindow> w, const LRect<int> &geometry, DWORD style, DWORD exStyle)
{
    if (!w->isTopLevel() || w->flags().testFlag(LWindowType::FramelessWindowHint)){
        return LMargins<int>();
    }
    if (!LWindowsCtx::user32dll.adjustWindowRectExForDpi
        || LWindowsScreenManager::isSingleScreen()
        || !LWindowsCtx::shouldHaveNonClientDpiScaling(w)) {
        return frameOnPrimaryScreen(style, exStyle);
    }
    double dpi = 96;
    auto screenManager = LWindowsCtx::instance()->screenManager();
    auto screen = screenManager.screenAtDp(geometry.center());
    if (!screen){
        screen = screenManager.screens().at(0);
    }
    if (screen){
        dpi = screen->logicalDpi().first;
    }
    return LWindowsGeometryHint::frame(style, exStyle, dpi);
}

bool LWindowsGeometryHint::handleCalculateSize(const LMargins<int> &customMargins, const MSG &msg, LRESULT *result)
{
    // NCCALCSIZE_PARAMS structure if wParam==TRUE
    if (!msg.wParam || customMargins.isNull()){
        return false;
    }
    *result = DefWindowProc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
    auto *ncp = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg.lParam);
    //const RECT oldClientArea = ncp->rgrc[0];
    ncp->rgrc[0].left += customMargins.m_left;
    ncp->rgrc[0].top += customMargins.m_top;
    ncp->rgrc[0].right -= customMargins.m_right;
    ncp->rgrc[0].bottom -= customMargins.m_bottom;
    result = nullptr;
    return true;
}

void LWindowsGeometryHint::applyToMinMaxInfo(const std::shared_ptr<LWindow> w,
                                             const std::shared_ptr<LScreen> screen,
                                             const LMargins<int> &margins,
                                             MINMAXINFO *mmi)
{
    LSize<int> minimumSize;
    LSize<int> maximumSize;
    frameSizeConstraints(w, screen, margins, &minimumSize, &maximumSize);

    if (minimumSize.width() > 0)
        mmi->ptMinTrackSize.x = minimumSize.width();
    if (minimumSize.height() > 0)
        mmi->ptMinTrackSize.y = minimumSize.height();

    if (maximumSize.width() < LWINDOWSIZE_MAX){
        mmi->ptMaxTrackSize.x = maximumSize.width();
    }
    if (maximumSize.height() < LWINDOWSIZE_MAX){
        mmi->ptMaxTrackSize.y = maximumSize.height();
    }
}

void LWindowsGeometryHint::applyToMinMaxInfo(const std::shared_ptr<LWindow> w,
                                             const LMargins<int> &margins,
                                             MINMAXINFO *mmi)
{
    applyToMinMaxInfo(w, w->screen(), margins, mmi);
}

// Scaling helpers for size constraints.
static LSize<int> toNativeSizeConstrained(LSize<int> dip, const std::shared_ptr<LScreen> s)
{
    if (LHighDpiScaling::isActive()) {
        const double factor = LHighDpiScaling::factor(s.get());
        if (!lFuzzyIsZero(factor- 1.0)) {
            if (dip.width() > 0 && dip.width() < LWINDOWSIZE_MAX){
                dip.setWidth(std::lround(double(dip.width()) * factor));
            }
            if (dip.height() > 0 && dip.height() < LWINDOWSIZE_MAX){
                dip.setHeight(std::lround(double(dip.height()) * factor));
            }
        }
    }
    return dip;
}

void LWindowsGeometryHint::frameSizeConstraints(const std::shared_ptr<LWindow> w, const std::shared_ptr<LScreen> screen, const LMargins<int> &margins, LSize<int> *minimumSize, LSize<int> *maximumSize)
{
    *minimumSize = toNativeSizeConstrained(w->minimumSize(), screen);
    *maximumSize = toNativeSizeConstrained(w->maximumSize(), screen);

    const int maximumWidth = std::max<int>(maximumSize->width(), minimumSize->width());
    const int maximumHeight = std::max<int>(maximumSize->height(), minimumSize->height());
    const int frameWidth = margins.m_left + margins.m_right;
    const int frameHeight = margins.m_top + margins.m_bottom;

    if (minimumSize->width() > 0){
        minimumSize->rwidth() += frameWidth;
    }
    if (minimumSize->height() > 0){
        minimumSize->rheight() += frameHeight;
    }
    if (maximumWidth < LWINDOWSIZE_MAX){
        maximumSize->setWidth(maximumWidth + frameWidth);
    }
    if (maximumHeight < LWINDOWSIZE_MAX){
        maximumSize->setHeight(maximumHeight + frameHeight);
    }
}

void LWindowsWin::clientToScreen(HWND hwnd, POINT *wP)
{
    if (isRtlLayout(hwnd)) {
        RECT clientArea;
        GetClientRect(hwnd, &clientArea);
        wP->x = clientArea.right - wP->x;
    }
    ClientToScreen(hwnd, wP);
}

void LWindowsWin::screenToClient(HWND hwnd, POINT *wP)
{
    ScreenToClient(hwnd, wP);
    if (LWindowsWin::isRtlLayout(hwnd)) {
        RECT clientArea;
        GetClientRect(hwnd, &clientArea);
        wP->x = clientArea.right - wP->x;
    }
}

LPoint<int> LWindowsGeometryHint::mapToGlobal(HWND hwnd, const LPoint<int> &lp)
{
    POINT p = { lp.m_x, lp.m_y };
    LWindowsWin::clientToScreen(hwnd, &p);
    return LPoint<int>(p.x, p.y);
}

LPoint<int> LWindowsGeometryHint::mapToGlobal(const std::shared_ptr<LWindow> w, const LPoint<int> &lp)
{
    return LWindowsGeometryHint::mapToGlobal(LWindowsWin::handleOf(w), lp);
}

LPoint<int> LWindowsGeometryHint::mapFromGlobal(const HWND hwnd, const LPoint<int> &lp)
{
    POINT p = { lp.m_x, lp.m_y };
    LWindowsWin::screenToClient(hwnd, &p);
    return LPoint<int>(p.x, p.y);
}

LPoint<int> LWindowsGeometryHint::mapFromGlobal(const std::shared_ptr<LWindow> w, const LPoint<int> &lp)
{
    return LWindowsGeometryHint::mapFromGlobal(LWindowsWin::handleOf(w), lp);
}

bool LWindowsGeometryHint::positionIncludesFrame(const std::shared_ptr<LWindow> w)
{
    return w->positionPolicy() == LWindow::WindowFrameInclusive;
}


struct LWindowCreationData
{
    using WindowData = LWindowsWindowData;
    enum Flags { ForceChild = 0x1, ForceTopLevel = 0x2 };

    void fromWindow(const std::shared_ptr<LWindow> w, const LFlags<LWindowType> flags, unsigned creationFlags = 0);
    inline WindowData create(const std::shared_ptr<LWindow> w, const WindowData &data, std::wstring title) const;
    inline void applyWindowFlags(HWND hwnd) const;
    void initialize(const std::shared_ptr<LWindow> w, HWND h, bool frameChange, double opacityLevel) const;

    LFlags<LWindowType> flags;
    HWND parentHandle = nullptr;
    LWindowType type = LWindowType::Widget;
    uint32_t style = 0;
    uint32_t exStyle = 0;
    bool topLevel = false;
    bool popup = false;
    bool dialog = false;
    bool tool = false;
    bool embedded = false;
    bool hasAlpha = false;
};


static LMargins<int> invisibleMargins(LPoint<int> screenPoint)
{
    if (LOperSysVersion::current() >= LOperSysVersion::Windows10) {
        POINT pt = {screenPoint.m_x, screenPoint.m_y};
        if (HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL)) {
            if (LWindowsCtx::shcoredll.isValid()) {
                UINT dpiX;
                UINT dpiY;
                if (SUCCEEDED(LWindowsCtx::shcoredll.getDpiForMonitor(hMonitor, 0, &dpiX, &dpiY))) {
                    const double sc = (dpiX - 96) / 96.0;
                    const int gap = 7 + std::lround(5*sc) - int(sc);
                    return LMargins<int>(gap, 0, gap, gap);
                }
            }
        }
    }
    return LMargins<int>();
}

static inline LSize<int> lSizeOfRect(const RECT &rect)
{
    return LSize<int>(rect.right -rect.left, rect.bottom - rect.top);
}

static inline LRect<int> lrectFromRECT(const RECT &rect)
{
    return LRect<int>(rect.left, rect.top, rect.right, rect.bottom, true);
}

static inline RECT RECTfromLRect(const LRect<int> &rect)
{
    RECT result = {rect.m_x, rect.m_y, rect.x2(), rect.y2()};
    return result;
}

static void setMinimizedGeometry(HWND hwnd, const LRect<int> &r)
{
    WINDOWPLACEMENT windowPlacement;
    windowPlacement.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(hwnd, &windowPlacement)) {
        windowPlacement.showCmd = SW_SHOWMINIMIZED;
        windowPlacement.rcNormalPosition = RECTfromLRect(r);
        SetWindowPlacement(hwnd, &windowPlacement);
    }
}

static LPoint<int> windowPlacementOffset(HWND hwnd, const LPoint<int> &point)
{
    if (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW){
        return LPoint<int>(0, 0);
    }
    const LWindowsScreenManager &screenManager = LWindowsCtx::instance()->screenManager();
    const std::shared_ptr<LWindowsScreen> screen = screenManager.screens().size() == 1
                                       ? screenManager.screens().front() : screenManager.screenAtDp(point);
    if (screen){
        return screen->availableGeometry().topLeft() - screen->geometry().topLeft();
    }
    return LPoint<int>(0, 0);
}

static inline bool windowIsAccelerated(const std::shared_ptr<LWindow> w)
{
    switch (w->surfaceType()) {
    case LSurface::OpenGLSurface:{
        return true;
    }
    case LSurface::RasterGLSurface:{
        return w->compositing();
    }
    case LSurface::VulkanSurface:{
        return true;
    }
    default:
        return false;
    }
}

static inline void fixTopLevelWindowFlags(LFlags<LWindowType> &flags)
{
    // Not supported on Windows, also do correction when it is set.
    flags &= ~uint32_t(LWindowType::WindowFullscreenButtonHint);
    switch (flags) {
    case (uint32_t)LWindowType::Window:{
        flags |= LWindowType::WindowTitleHint | LWindowType::WindowSystemMenuHint | LWindowType::WindowMinimizeButtonHint
                 |LWindowType::WindowMaximizeButtonHint|LWindowType::WindowCloseButtonHint;
        break;
    }
    case (uint32_t)LWindowType::Dialog:
    case (uint32_t)LWindowType::Tool:{
        flags |= LWindowType::WindowTitleHint | LWindowType::WindowSystemMenuHint | LWindowType::WindowCloseButtonHint;
        break;
    }
    default:
        break;
    }
    if ((flags & LWindowType::WindowTypeMask) == LWindowType::SplashScreen){
        flags |= LWindowType::FramelessWindowHint;
    }
}

static bool shouldShowMaximizeButton(const std::shared_ptr<LWindow> w, LFlags<LWindowType> flags)
{
    if ((flags & LWindowType::MSWindowsFixedSizeDialogHint) || !(flags & LWindowType::WindowMaximizeButtonHint)){
        return false;
    }
    // if the user explicitly asked for the maximize button, we try to add
    // it even if the window has fixed size.
    return (flags & LWindowType::CustomizeWindowHint) ||
           w->maximumSize() == LSize<int>(LWINDOWSIZE_MAX, LWINDOWSIZE_MAX);
}

static inline bool shouldApplyDarkFrame(const std::shared_ptr<LWindow> w)
{
    return w->isTopLevel() && !(w->flags() & LWindowType::FramelessWindowHint);
}


// Return the frame geometry relative to the parent
// if there is one.
static inline LRect<int> frameGeometry(HWND hwnd, bool topLevel)
{
    RECT rect = { 0, 0, 0, 0 };
    if (topLevel) {
        WINDOWPLACEMENT windowPlacement;
        windowPlacement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(hwnd, &windowPlacement);
        if (windowPlacement.showCmd == SW_SHOWMINIMIZED) {
            const LRect<int> result = lrectFromRECT(windowPlacement.rcNormalPosition);
            return result.translated(windowPlacementOffset(hwnd, result.topLeft()));
        }
    }
    GetWindowRect(hwnd, &rect); // Screen coordinates.
    const HWND parent = GetParent(hwnd);
    if (parent && !topLevel) {
        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        POINT leftTop = { rect.left, rect.top };
        LWindowsWin::screenToClient(parent, &leftTop);
        rect.left = leftTop.x;
        rect.top = leftTop.y;
        rect.right = leftTop.x + width;
        rect.bottom = leftTop.y + height;
    }
    return lrectFromRECT(rect);
}

LWindowCreationCtx::LWindowCreationCtx(const std::shared_ptr<LWindow> w,
                                       const std::shared_ptr<LScreen> s,
                                       const LRect<int> &geometryIn,
                                       const LRect<int> &geometry,
                                       const LMargins<int> &cm,
                                       DWORD style,
                                       DWORD exStyle):
    window(w),
    screen(s),
    requestedGeometryIn(geometryIn),
    requestedGeometry(geometry),
    obtainedPos(geometryIn.topLeft()),
    obtainedSize(geometryIn.size()),
    margins(LWindowsGeometryHint::frame(w, geometry, style, exStyle)),
    customMargins(cm)
{
    if (geometry.isValid() || (!w->resizeAutomatic())) {
        frameX = geometry.m_x;
        frameY = geometry.m_y;
        const LMargins<int> effectiveMargins = margins + customMargins;
        frameWidth = effectiveMargins.m_left + geometry.width() + effectiveMargins.m_right;
        frameHeight = effectiveMargins.m_top + geometry.height() + effectiveMargins.m_bottom;
//        if (LWindowsMenuBar::menuBarOf(w) != nullptr) { //Todo
//            menuHeight = GetSystemMetrics(SM_CYMENU);
//            frameHeight += menuHeight;
//        }
        const bool isDefaultPosition = !frameX && !frameY && w->isTopLevel();
        if (!LWindowsGeometryHint::positionIncludesFrame(w) && !isDefaultPosition) {
            frameX -= effectiveMargins.m_left;
            frameY -= effectiveMargins.m_top;
        }
    }
}

void LWindowCreationCtx::applyToMinMaxInfo(MINMAXINFO *mmi) const
{
    LWindowsGeometryHint::applyToMinMaxInfo(window, screen, margins + customMargins, mmi);
}


void LWindowCreationData::fromWindow(const std::shared_ptr<LWindow> w, const LFlags<LWindowType> flagsIn, unsigned int creationFlags)
{
    flags = flagsIn;

    if (creationFlags & ForceChild) {
        topLevel = false;
    } else if (embedded) {
        // Embedded native windows (for example Active X server windows) are by
        // definition never toplevel, even though they do not have QWindow parents.
        topLevel = false;
    } else {
        topLevel = (creationFlags & ForceTopLevel) ? true : w->isTopLevel();
    }

    if (topLevel){
        fixTopLevelWindowFlags(flags);
    }

    type = static_cast<LWindowType>((flags & LWindowType::WindowTypeMask).value());
    switch (type) {
    case LWindowType::Dialog:
    case LWindowType::Sheet:{
        dialog = true;
        break;
    }
    case LWindowType::Drawer:
    case LWindowType::Tool:{
        tool = true;
        break;
    }
    case LWindowType::Popup:{
        popup = true;
        break;
    }
    default:
        break;
    }
    if ((flags & LWindowType::MSWindowsFixedSizeDialogHint)){
        dialog = true;
    }

    // Parent: Use transient parent for top levels.
    if (popup) {
        flags |= LWindowType::WindowStaysOnTopHint; // a popup stays on top, no parent.
    } else if (!embedded) {
        if (const std::shared_ptr<LWindow> parentWindow = topLevel ? w->transientParent() : w->parent()){
            parentHandle = LWindowsWin::handleOf(parentWindow);
        }
    }

    if (popup || (type == LWindowType::ToolTip) || (type == LWindowType::SplashScreen)) {
        style = WS_POPUP;
    } else if (topLevel) {
        if (flags & LWindowType::FramelessWindowHint){
            style = WS_POPUP;                // no border
        }
        else if (flags & LWindowType::WindowTitleHint){
            style = WS_OVERLAPPED;
        }
        else{
            style = 0;
        }
    } else {
        style = WS_CHILD;
    }

    style |= WS_CLIPSIBLINGS | WS_CLIPCHILDREN ;
    if (topLevel) {
        if ((type == LWindowType::Window || dialog || tool)) {
            if (!(flags & LWindowType::FramelessWindowHint)) {
                style |= WS_POPUP;
                if (flags & LWindowType::MSWindowsFixedSizeDialogHint) {
                    style |= WS_DLGFRAME;
                } else {
                    style |= WS_THICKFRAME;
                }
                if (flags & LWindowType::WindowTitleHint){
                    style |= WS_CAPTION; // Contains WS_DLGFRAME
                }
            }
            if (flags & LWindowType::WindowSystemMenuHint){
                style |= WS_SYSMENU;
            }
            else if (dialog && (flags & LWindowType::WindowCloseButtonHint) && !(flags & LWindowType::FramelessWindowHint)) {
                style |= WS_SYSMENU | WS_BORDER;
                exStyle |= WS_EX_DLGMODALFRAME;
            }
            if (flags & LWindowType::WindowMinimizeButtonHint){
                style |= WS_MINIMIZEBOX;
            }
            if (shouldShowMaximizeButton(w, flags)){
                style |= WS_MAXIMIZEBOX;
            }
            if (tool){
                exStyle |= WS_EX_TOOLWINDOW;
            }
            if (flags & LWindowType::WindowContextHelpButtonHint){
                exStyle |= WS_EX_CONTEXTHELP;
            }
        } else {
            exStyle |= WS_EX_TOOLWINDOW;
        }

        // make mouse events fall through this window
        // NOTE: WS_EX_TRANSPARENT flag can make mouse inputs fall through a layered window
        if (flagsIn & LWindowType::WindowTransparentForInput){
            exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
        }
    }

}

LWindowCreationData::WindowData LWindowCreationData::create(const std::shared_ptr<LWindow> w, const WindowData &data, std::wstring title) const
{
    WindowData result;
    result.flags = flags;

    const auto appinst = reinterpret_cast<HINSTANCE>(GetModuleHandle(nullptr));

    const std::string windowClassName = LWindowsCtx::instance()->registerWindowClass(w);

    LRect<int> rect;
    std::shared_ptr<LScreen> screen = LPlatformWindow::initialGeometry(w, data.geometry,
                                                                      160, 160,
                                                                      rect);

    if (title.empty() && (result.flags & LWindowType::WindowTitleHint)){
        //title = topLevel ? lAppName() : w->title();
        title = w->title();
    }

    // Capture events before CreateWindowEx() returns. The context is cleared in
    // the LWindowsWindow constructor.
    std::shared_ptr<LWindowCreationCtx> context  = std::make_shared<LWindowCreationCtx>(w,
                                                                                       screen,
                                                                                       data.geometry,
                                                                                       rect,
                                                                                       data.customMargins,
                                                                                       style,
                                                                                       exStyle);
    LWindowsCtx::instance()->setWindowCreationContext(context);

    const bool hasFrame = (style & (WS_DLGFRAME | WS_THICKFRAME));
    LMargins<int> invMargins = topLevel && hasFrame && LWindowsGeometryHint::positionIncludesFrame(w)
                                   ? invisibleMargins(LPoint<int>(context->frameX, context->frameY)) : LMargins<int>();


    LPoint<int> pos = calcPosition(w, context, invMargins);

    // Mirror the position when creating on a parent in RTL mode, ditto for the obtained geometry.
    int mirrorParentWidth = 0;
    if (mirrorParentWidth != 0 && pos.m_x != CW_USEDEFAULT && context->frameWidth != CW_USEDEFAULT){
        pos.m_x = (mirrorParentWidth - context->frameWidth - pos.m_x);
    }

    std::string titleS = LStringUtil::wideToUtf8(title);
    result.hwnd = CreateWindowExA(exStyle, windowClassName.data(), titleS.data(),
                                 style,
                                 pos.m_x, pos.m_y,
                                 context->frameWidth, context->frameHeight,
                                 parentHandle, nullptr, appinst, nullptr);

    if (!result.hwnd) {
        return result;
    }


    if (mirrorParentWidth != 0) {
        context->obtainedPos.m_x = (mirrorParentWidth - context->obtainedSize.width()-  context->obtainedPos.m_x);
    }

    LRect<int> obtainedGeometry(context->obtainedPos, context->obtainedSize);

    result.geometry = obtainedGeometry;
    result.fullFrameMargins = context->margins;
    result.embedded = embedded;
    result.hasFrame = hasFrame;
    result.customMargins = context->customMargins;

    return result;
}

void LWindowCreationData::applyWindowFlags(HWND hwnd) const
{
    // Keep enabled and visible from the current style.
    const LONG_PTR oldStyle = GetWindowLongPtr(hwnd, GWL_STYLE);
    const LONG_PTR oldExStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

    const LONG_PTR newStyle = style | (oldStyle & (WS_DISABLED|WS_VISIBLE));
    if (oldStyle != newStyle){
        SetWindowLongPtr(hwnd, GWL_STYLE, newStyle);
    }
    const LONG_PTR newExStyle = exStyle;
    if (newExStyle != oldExStyle){
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, newExStyle);
    }
}

void LWindowCreationData::initialize(const std::shared_ptr<LWindow> w, HWND hwnd, bool frameChange, double opacityLevel) const
{
    if (!hwnd){
        return;
    }

    UINT swpFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER;
    if (frameChange){
        swpFlags |= SWP_FRAMECHANGED;
    }
    if (topLevel) {
        swpFlags |= SWP_NOACTIVATE;
        if ((flags & LWindowType::WindowStaysOnTopHint) || (type == LWindowType::ToolTip)) {
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, swpFlags);
        } else if (flags & LWindowType::WindowStaysOnBottomHint) {
            SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, swpFlags);
        } else if (frameChange) { // Force WM_NCCALCSIZE with wParam=1 in case of custom margins.
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, swpFlags);
        }
        if (flags & (LWindowType::CustomizeWindowHint|LWindowType::WindowTitleHint)) {
            HMENU systemMenu = GetSystemMenu(hwnd, FALSE);
            if (flags & LWindowType::WindowCloseButtonHint)
                EnableMenuItem(systemMenu, SC_CLOSE, MF_BYCOMMAND|MF_ENABLED);
            else
                EnableMenuItem(systemMenu, SC_CLOSE, MF_BYCOMMAND|MF_GRAYED);
        }
    } else { // child.
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, swpFlags);
    }
}

LWindowsWindowData LWindowsWindowData::create(const std::shared_ptr<LWindow> w,
                                              const LWindowsWindowData &parameters,
                                              const std::wstring &title)
{
    LWindowCreationData creationData;
    creationData.fromWindow(w, parameters.flags);
    LWindowsWindowData result = creationData.create(w, parameters, title);
    // Force WM_NCCALCSIZE (with wParam=1) via SWP_FRAMECHANGED for custom margin.
    creationData.initialize(w, result.hwnd, !parameters.customMargins.isNull(), 1);
    return result;
}



LWindowsWin::LWindowsWin(std::shared_ptr<LWindow> window, const LWindowsWindowData &data)
   :LPlatformWindow(window),
    m_data(data)
{
    m_cursor = std::make_shared<LCursorHandle>();
}

LWindowsWin::~LWindowsWin()
{
    setFlag(WithinDestroy);
    if (testFlag(TouchRegistered)){
        UnregisterTouchWindow(m_data.hwnd);
    }
    destroyWindow();
    destroyIcon();
}

void LWindowsWin::initialize()
{
    //构造函数中移过来的
    {
        std::shared_ptr<LWindowsWin> w = getPtrT<LWindowsWin>();
        LWindowsCtx::instance()->addWindow(m_data.hwnd, w);
    }

    {
        std::shared_ptr<LWindowCreationCtx> creationContext = LWindowsCtx::instance()->setWindowCreationContext(nullptr);
        std::shared_ptr<LWindow> w = window();
        setWindowStates(w->windowStates());

        // Trigger geometry change (unless it has a special state in which case setWindowState()
        // will send the message) and screen change signals of QWindow.
        if (w->type() != LWindowType::Desktop) {
            const LWindowState state = w->windowState();
            if(creationContext != nullptr){
                const LRect<int> obtainedGeometry(creationContext->obtainedPos, creationContext->obtainedSize);
                if (state != LWindowState::Maximized && state != LWindowState::FullScreen
                    && creationContext->requestedGeometryIn != obtainedGeometry) {
                    LWindowSystemInterface::handleGeometryChange(w, obtainedGeometry, LDeliverType::Sync);
                }

                std::shared_ptr<LPlatformScreen> obtainedScreen = screenForGeometry(obtainedGeometry);
                if (obtainedScreen && screen() != obtainedScreen){
                    LWindowSystemInterface::handleWindowScreenChanged(w, obtainedScreen->screen(), LDeliverType::Sync);
                }
            }
        }
    }
}

HICON LWindowsWin::pixmapToWinHICON(const std::shared_ptr<LPixmap> p)
{
    if (p == nullptr || p->isNull()){
        return nullptr;
    }

    ICONINFO ii;
    ii.fIcon    = true;

    const int w = p->width();
    const int h = p->height();

    std::shared_ptr<LImage> im = std::make_shared<LImage>(w,h,LImage::Format::Mono);
    im->fill(LColorID::Black);
    ii.hbmMask = CreateBitmap(w, h, 1, 1, im->bits());

    std::shared_ptr<LImage> tm = p->toImage();
    ii.hbmColor = imageToWinHBITMAP(tm, HBitmapFormat::Alpha);
    ii.xHotspot = 0;
    ii.yHotspot = 0;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(ii.hbmColor);
    DeleteObject(ii.hbmMask);

    return hIcon;
}

template <typename Int>
static inline Int pad4(Int v)
{
    return (v + Int(3)) & ~Int(3);
}

static inline void initBitMapInfoHeader(int width, int height, bool topToBottom,
                                        DWORD compression, DWORD bitCount,
                                        BITMAPINFOHEADER *bih)
{
    memset(bih, 0, sizeof(BITMAPINFOHEADER));
    bih->biSize        = sizeof(BITMAPINFOHEADER);
    bih->biWidth       = width;
    bih->biHeight      = topToBottom ? -height : height;
    bih->biPlanes      = 1;
    bih->biBitCount    = WORD(bitCount);
    bih->biCompression = compression;
        // scan lines are word-aligned (unless RLE)
    const DWORD bytesPerLine = pad4(DWORD(width) * bitCount / 8);
    bih->biSizeImage   = bytesPerLine * DWORD(height);
}


enum { Indexed8ColorTableSize = 256 };

struct BITMAPINFO_COLORTABLE256 { // BITMAPINFO with 256 entry color table for Indexed 8 format
    BITMAPINFOHEADER    bmiHeader;
    RGBQUAD             bmiColors[Indexed8ColorTableSize];
};


static inline LImage::Format format32(HBitmapFormat hbitmapFormat)
{
    switch (hbitmapFormat) {
    case HBitmapFormat::NoAlpha:{
        return LImage::Format::RGB32;
    }
    case HBitmapFormat::Alpha:
        return LImage::Format::ARGB32;
    default:
        break;
    }
    return LImage::Format::ARGB32Premultiplied;
}

template <class BITMAPINFO_T> // BITMAPINFO, BITMAPINFO_COLORTABLE256
static inline void initBitMapInfo(int width, int height, bool topToBottom,
                                  DWORD compression, DWORD bitCount,
                                  BITMAPINFO_T *bmi)
{
    initBitMapInfoHeader(width, height, topToBottom, compression, bitCount, &bmi->bmiHeader);
    memset(bmi->bmiColors, 0, sizeof(bmi->bmiColors));
}

static inline RGBQUAD lRgbToRgbQuad(uint32_t argb)
{
    RGBQUAD result = {BYTE(LColor::uintToBlue(argb)),
                      BYTE(LColor::uintToGreen(argb)),
                      BYTE(LColor::uintToRed(argb)),
                      0};
    return result;
}

// Flip RGB triplets from DIB to QImage formats. Scan lines are padded to 32bit
// both in LImage and DIB.
static inline void flipRgb3(uint8_t *p, int width, int height)
{
    const int lineSize = 3 * width;
    const int linePad = pad4(lineSize) - lineSize;
    for (int y = 0; y < height; ++y) {
        uint8_t *end = p + lineSize;
        for ( ; p < end; p += 3){
            std::swap(*p, *(p + 2));
        }
        p += linePad;
    }
}


HBITMAP LWindowsWin::imageToWinHBITMAP(std::shared_ptr<LImage> image, HBitmapFormat hbitmapFormat)
{
    if (image == nullptr || image->isNull()){
        return nullptr;
    }
    // Define the header
    DWORD compression = 0;
    DWORD bitCount = 0;

    switch (image->format()) {
    case LImage::Format::Mono:
        bitCount = 1u;
        break;
    case LImage::Format::RGB32:
    case LImage::Format::ARGB32:
    case LImage::Format::ARGB32Premultiplied: {
        compression = BI_RGB;
        bitCount = 32u;
        const LImage::Format targetFormat = format32(hbitmapFormat);
        if (targetFormat != image->format()){
            image = image->convertToFormat(targetFormat);
        }
    }
    break;
    case LImage::Format::RGB888:
    case LImage::Format::BGR888:
        compression = BI_RGB;
        bitCount = 24u;
        break;
    case LImage::Format::Indexed8:
        bitCount = 8u;
        break;
    case LImage::Format::RGB555:
        bitCount = 16u;
        break;
    default: {
        LImage::Format fallbackFormat = LImage::Format::ARGB32Premultiplied;
        switch (image->format()) { // Convert to a suitable format.
        case LImage::Format::MonoLSB:
            fallbackFormat = LImage::Format::Mono;
            break;
        case LImage::Format::RGB16:
            fallbackFormat = LImage::Format::RGB555;
            break;
        case LImage::Format::Grayscale8:
            fallbackFormat = LImage::Format::Indexed8;
            break;
        default:
            break;
        } // switch conversion format
        return imageToWinHBITMAP(image->convertToFormat(fallbackFormat), hbitmapFormat);
    }
    }

    const int w = image->width();
    const int h = image->height();

    BITMAPINFO_COLORTABLE256 bmiColorTable256;
    initBitMapInfo(w, h, true, compression, bitCount, &bmiColorTable256);
    BITMAPINFO &bmi = reinterpret_cast<BITMAPINFO &>(bmiColorTable256);
    switch (image->format()) {
    case LImage::Format::Mono: // Color table with 2 entries
    case LImage::Format::Indexed8:{
        std::transform(image->colorTable().begin(), image->colorTable().end(),
                       bmiColorTable256.bmiColors, lRgbToRgbQuad);
        break;
    }
    default:
        break;
    }

    // Create the pixmap
    uint8_t *pixels = nullptr;
    const HBITMAP bitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS,
                                            reinterpret_cast<void **>(&pixels), nullptr, 0);
    if (!bitmap) {
        lWarn() << "Failed to create dibsection";
        return nullptr;
    }
    if (!pixels) {
        lWarn() << "Did not allocate pixel data";
        return nullptr;
    }
    memcpy(pixels, image->bits(), bmi.bmiHeader.biSizeImage);
    if (image->format() == LImage::Format::RGB888){
        flipRgb3(pixels, w, h);
    }
    return bitmap;

}

LMargins<int> LWindowsWin::frameMargins() const
{
    LMargins<int> result = fullFrameMargins();
    if (isTopLevel() && m_data.hasFrame){
        result -= invisibleMargins(m_data.geometry.topLeft());
    }
    return result;
}

LMargins<int> LWindowsWin::fullFrameMargins() const
{
    return m_data.fullFrameMargins;
}

void LWindowsWin::setFullFrameMargins(const LMargins<int> &newMargins)
{
    if (m_data.fullFrameMargins != newMargins) {
        m_data.fullFrameMargins = newMargins;
    }
}

void LWindowsWin::updateFullFrameMargins()
{
    //Todo
//    if (GetMenu(m_data.hwnd)){
//        LWindowsCtx::forceNcCalcSize(m_data.hwnd);
//    }
//    else{
//        calculateFullFrameMargins();
//    }
}

void LWindowsWin::setGeometry(const LRect<int> &rectIn)
{
    LRect<int> rect = rectIn;
    std::shared_ptr<LWindow> win = window();
    if(win != nullptr){
        if (win->positionPolicy()  == LWindow::WindowFrameInclusive) {
            const LMargins<int> margins = frameMargins();
            rect.moveTopLeft(rect.topLeft() + LPoint<int>(margins.m_left, margins.m_top));
        }
    }

    if (m_windowState & LWindowState::Minimized){
        m_data.geometry = rect; // Otherwise set by handleGeometryChange() triggered by event.
    }
    if (m_data.hwnd) {
        setFlag(WithinSetGeometry);
        setGeometrySys(rect);
        clearFlag(WithinSetGeometry);
        if (m_data.geometry != rect && isVisible()) {
            lWarn() << "Unable Set geometry";
        }
    } else {
        LPlatformWindow::setGeometry(rect);
    }
}

static LRect<int> normalFrameGeometry(HWND hwnd)
{
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(hwnd, &wp)) {
        const LRect<int> result = lrectFromRECT(wp.rcNormalPosition);
        return result.translated(windowPlacementOffset(hwnd, result.topLeft()));
    }
    return LRect<int>();
}

LRect<int> LWindowsWin::normalGeometry() const
{
    // Check for fake 'fullscreen' mode.
    const bool fakeFullScreen =
        m_savedFrameGeometry.isValid() && (window()->windowStates() & LWindowState::FullScreen);
    const LRect<int> frame = fakeFullScreen ? m_savedFrameGeometry : normalFrameGeometry(m_data.hwnd);
    const LMargins<int> margins = fakeFullScreen
                                 ? LWindowsGeometryHint::frame(handle(), m_savedStyle, 0)
                                 : fullFrameMargins();
    return frame.isValid() ? frame.marginsRemoved(margins) : frame;
}

void LWindowsWin::setWindowFlags(LFlags<LWindowType> flags)
{
    const LRect<int> oldGeometry = geometry();
    if (m_data.flags != flags) {
        m_data.flags = flags;
        if (m_data.hwnd) {
            m_data = setWindowFlagsSys(flags);
            updateDropSite(window()->isTopLevel());
        }
    }
    // When switching to a frameless window, geometry
    // may change without a WM_MOVE. Report change manually.
    // Do not send synchronously as not to clobber the widget
    // geometry in a sequence of setting flags and geometry.
    const LRect<int> newGeometry = geometrySys();
    if (oldGeometry != newGeometry){
        handleGeometryChange();
    }
}

void LWindowsWin::setWindowStates(LFlags<LWindowState> state)
{
    if (m_data.hwnd) {
        setWindowStateSys(state);
        m_windowState = state;
    }
}

HWND LWindowsWin::handle() const
{
    return m_data.hwnd;
}

WId LWindowsWin::winId() const
{
    return WId(reinterpret_cast<uintptr_t>(m_data.hwnd));
}

bool LWindowsWin::isTopLevel() const
{
    const HWND parent = parentHwnd();
    return !parent || parent == GetDesktopWindow();
}

void LWindowsWin::setVisible(bool visible)
{
    const std::shared_ptr<LWindow> win = window();
    if (m_data.hwnd && win != nullptr) {
        if (visible) {
            showSys();
            // When the window is layered, we won't get WM_PAINT, and "we" are in control
            // over the rendering of the window
            // There is nobody waiting for this, so we don't need to flush afterwards.
            if (isLayered()){
                fireFullExpose();
            }
            // This is to resolve the problem where popups are
            // opened from the system tray and not being implicitly activated
            if (win->type() == LWindowType::Popup && !win->parent() && !LGuiApplication::focusWindow()){
                SetForegroundWindow(m_data.hwnd);
            }
        } else {
            if (hasMouseCapture()){
                setMouseGrabEnabled(false);
            }
            if (window()->flags() & LWindowType::Popup){
                ShowWindow(m_data.hwnd, SW_HIDE);
            }
            else{
                hideSys();
            }
            fireExpose(nullptr);
        }
    }
}

bool LWindowsWin::isVisible() const
{
    return m_data.hwnd && IsWindowVisible(m_data.hwnd);
}

HDC LWindowsWin::getDC()
{
    if (!m_hdc) {
        m_hdc = GetDC(handle());
    }
    return m_hdc;
}

void LWindowsWin::releaseDC()
{
    if (m_hdc) {
        ReleaseDC(handle(), m_hdc);
        m_hdc = nullptr;
    }
}

std::shared_ptr<LWindowsWin> LWindowsWin::baseWindowOf(const std::shared_ptr<LWindow> w)
{
    if (w!= nullptr) {
        std::shared_ptr<LPlatformWindow> pw = w->handle();
        return std::dynamic_pointer_cast<LWindowsWin>(pw);
    }
    return nullptr;
}

std::shared_ptr<LWindowsWin> LWindowsWin::windowsWindowOf(const std::shared_ptr<LWindow> w)
{
    if (w!= nullptr) {
        std::shared_ptr<LPlatformWindow> pw = w->handle();
        if(pw != nullptr){
           if (w->type() == LWindowType::Desktop || pw->isForeignWindow()){
                return nullptr;
            }
            return std::dynamic_pointer_cast<LWindowsWin>(pw);
        }
    }
    return nullptr;

}

bool LWindowsWin::isAncestorOf(const std::shared_ptr<LPlatformWindow> child) const
{
    std::shared_ptr<LWindowsWin> childWindow = std::dynamic_pointer_cast<LWindowsWin>(child);
    return IsChild(m_data.hwnd, childWindow->handle());
}

void LWindowsWin::setParent(const std::shared_ptr<LPlatformWindow> parent)
{
    if (m_data.hwnd){
        // Use GetAncestor instead of GetParent, as GetParent can return owner window for toplevels
        HWND oldParentHWND = parentHwnd();
        HWND newParentHWND = nullptr;

        std::shared_ptr<LWindowsWin> parentW = std::dynamic_pointer_cast<LWindowsWin>(parent);
        if (parentW != nullptr) {
            newParentHWND = parentW->handle();
        }

        // NULL handle means desktop window, which also has its proper handle -> disambiguate
        HWND desktopHwnd = GetDesktopWindow();
        if (oldParentHWND == desktopHwnd){
            oldParentHWND = nullptr;
        }
        if (newParentHWND == desktopHwnd){
            newParentHWND = nullptr;
        }

        if (newParentHWND != oldParentHWND) {
            const bool wasTopLevel = oldParentHWND == nullptr;
            const bool isTopLevel = newParentHWND == nullptr;

            setFlag(WithinSetParent);
            SetParent(m_data.hwnd, newParentHWND);
            clearFlag(WithinSetParent);

            if (wasTopLevel != isTopLevel) {
                setDropSiteEnabled(false);
                setWindowFlagsSys(window()->flags(), unsigned(isTopLevel ? LWindowCreationData::ForceTopLevel : LWindowCreationData::ForceChild));
                updateDropSite(isTopLevel);
            }
        }
    }
}

void LWindowsWin::setWindowTitle(const std::wstring &title)
{
    SetWindowTextW(handle(), title.data());
}

void LWindowsWin::setWindowIcon(const std::shared_ptr<LTexture> icon)
{
    if (m_data.hwnd) {
        destroyIcon();

        m_iconSmall = createHIcon(icon, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
        m_iconBig = createHIcon(icon, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));

        if (m_iconBig) {
            SendMessage(m_data.hwnd, WM_SETICON, 0 /* ICON_SMALL */, LPARAM(m_iconSmall));
            SendMessage(m_data.hwnd, WM_SETICON, 1 /* ICON_BIG */, LPARAM(m_iconBig));
        } else {
            SendMessage(m_data.hwnd, WM_SETICON, 0 /* ICON_SMALL */, LPARAM(m_iconSmall));
            SendMessage(m_data.hwnd, WM_SETICON, 1 /* ICON_BIG */, LPARAM(m_iconSmall));
        }
    }
}

HWND LWindowsWin::handleOf(const std::shared_ptr<LWindow> w)
{
    std::shared_ptr<LWindowsWin> ww = std::dynamic_pointer_cast<LWindowsWin>(w->handle());
    if(ww!= nullptr){
        return ww->handle();
    }
    return HWND(nullptr);
}

std::shared_ptr<LWindow> LWindowsWin::topLevelOf(std::shared_ptr<LWindow> w)
{
    while (std::shared_ptr<LWindow> parent = w->parent()){
        w = parent;
    }

    if (const std::shared_ptr<LPlatformWindow> handle = w->handle()) {
        std::shared_ptr<LWindowsWin> ww = std::dynamic_pointer_cast<LWindowsWin>(handle);
        if (ww->isEmbedded()) {
            HWND parentHWND = GetAncestor(ww->handle(), GA_PARENT);
            const HWND desktopHwnd = GetDesktopWindow();
            const LWindowsCtx *ctx = LWindowsCtx::instance();
            while (parentHWND && parentHWND != desktopHwnd) {
                if (std::shared_ptr<LWindowsWin> ancestor = ctx->findPlatformWindow(parentHWND)){
                    return topLevelOf(ancestor->window());
                }
                parentHWND = GetAncestor(parentHWND, GA_PARENT);
            }
        }
    }
    return w;
}

bool LWindowsWin::isRtlLayout(HWND hwnd)
{
    return (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
}

std::shared_ptr<LScreen> LWindowsWin::forcedScreenForGLWindow(const std::shared_ptr<LWindow> w)
{
    static std::string forceToScreen;
    if (!m_screenForGLInitialized) {
        std::shared_ptr<LGpuDescription> desc = LGpuDescription::detect();
        if(desc != nullptr){
            forceToScreen = desc->m_gpuSuitableScreen;
            m_screenForGLInitialized = true;
        }
    }
    return forceToScreen.empty() ? nullptr : screenForName(w, forceToScreen);
}

bool LWindowsWin::setWindowLayered(HWND hwnd, LFlags<LWindowType> flags, bool hasAlpha, double opacity)
{
    const LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    const bool needsLayered = (flags & LWindowType::WindowTransparentForInput)
                              || (hasAlpha && (flags & LWindowType::FramelessWindowHint)) || opacity < 1.0;
    const bool isLayered = (exStyle & WS_EX_LAYERED);
    if (needsLayered != isLayered) {
        if (needsLayered) {
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        } else {
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
        }
    }
    return needsLayered;
}

void LWindowsWin::handleMoved()
{
    if (!IsIconic(m_data.hwnd) && !testFlag(WithinSetParent)){
        handleGeometryChange();
    }
}

void LWindowsWin::handleResized(int wParam)
{
    switch (wParam) {
    case SIZE_MAXHIDE: // Some other window affected.
    case SIZE_MAXSHOW:{
        return;
    }
    case SIZE_MINIMIZED:{ // prevent state change events during programmatic state change
        if (!testFlag(WithinSetStyle) && !testFlag(WithinSetGeometry)){
            handleWindowStateChange(m_windowState | LWindowState::Minimized);
        }
        return;
    }
    case SIZE_MAXIMIZED:{
        handleGeometryChange();
        if (!testFlag(WithinSetStyle) && !testFlag(WithinSetGeometry)){
            handleWindowStateChange(LWindowState::Maximized | (isFullScreenSys() ? LWindowState::FullScreen
                                                                              : LWindowState::NoState));
        }
        break;
    }
    case SIZE_RESTORED:
        handleGeometryChange();
        if (!testFlag(WithinSetStyle) && !testFlag(WithinSetGeometry)) {
            if (isFullScreenSys()){
                handleWindowStateChange(
                    LWindowState::FullScreen
                    | (testFlag(MaximizeToFullScreen) ? LWindowState::Maximized : LWindowState::NoState));
            }
            else if (m_windowState != LWindowState::NoState && !testFlag(MaximizeToFullScreen)){
                handleWindowStateChange(LWindowState::NoState);
            }
        }
        break;
    }
}

void LWindowsWin::handleHidden()
{

}

static int getBorderWidth(const std::shared_ptr<LPlatformScreen> screen)
{
    NONCLIENTMETRICS ncm;
    LWindowsCtx::nonClientMetricsForScreen(&ncm, screen);
    return ncm.iBorderWidth + ncm.iPaddedBorderWidth + 2;
}

void LWindowsWin::getSizeHints(MINMAXINFO *mmi) const
{
    std::shared_ptr<LWindow> win = window();
    if(win == nullptr){
        return;
    }

    // We don't apply the min/max size hint as we change the dpi, because we did not adjust the
    // LScreen of the window yet so we don't have the min/max with the right ratio
    if (!testFlag(LWindowsWin::WithinDpiChanged)){
        LWindowsGeometryHint::applyToMinMaxInfo(win, fullFrameMargins(), mmi);
    }

    // Frameless/title-less windows shouldn't cover the
    // taskbar when maximized
    if ((testFlag(WithinMaximize) || win->windowStates().testFlag(LWindowState::Minimized))
        && (m_data.flags.testFlag(LWindowType::FramelessWindowHint)
            || (m_data.flags.testFlag(LWindowType::CustomizeWindowHint) && !m_data.flags.testFlag(LWindowType::WindowTitleHint)))) {
        const std::shared_ptr<LScreen> screen = win->screen();

        // Documentation of MINMAXINFO states that it will only work for the primary screen
        if(screen != nullptr){
            if (screen == LGuiApplication::primaryScreen()) {
                const LRect<int> availableGeometry = LHighDpi::toNativePixels(screen->m_availableGeometry, screen.get());
                mmi->ptMaxSize.y = availableGeometry.height();

                // Width, because you can have the taskbar on the sides too.
                mmi->ptMaxSize.x = availableGeometry.width();

                // If you have the taskbar on top, or on the left you don't want it at (0,0):
                mmi->ptMaxPosition.x = availableGeometry.m_x;
                mmi->ptMaxPosition.y = availableGeometry.m_y;
                if (!m_data.flags.testFlag(LWindowType::FramelessWindowHint)) {
                    const int borderWidth = getBorderWidth(screen->handle());
                    mmi->ptMaxSize.x += borderWidth * 2;
                    mmi->ptMaxSize.y += borderWidth * 2;
                    mmi->ptMaxTrackSize = mmi->ptMaxSize;
                    mmi->ptMaxPosition.x -= borderWidth;
                    mmi->ptMaxPosition.y -= borderWidth;
                }
            }
        }
    }
}

void LWindowsWin::setCursor(const std::shared_ptr<LCursorHandle> c)
{
    if (c->handle() != m_cursor->handle()) {
        const bool apply = canApplyNewCursor(window());
        m_cursor = c;
        if (apply){
            applyCursor();
        }
    }
}

void LWindowsWin::applyCursor()
{
    if (LWindowsCursor::hasOverrideCursor()) {
        if (isTopLevel()){
            LWindowsCursor::enforceOverrideCursor();
        }
        return;
    }

    if (m_cursor->isNull()) { // Recurse up to parent with non-null cursor. Set default for toplevel.
        if (const std::shared_ptr<LWindow> p = window()->parent()) {
            std::shared_ptr<LWindowsWin> platformWindow = LWindowsWin::windowsWindowOf(p);
            if (platformWindow != nullptr){
                platformWindow->applyCursor();
            }
        } else {
            SetCursor(defaultCursor(window())->handle());
        }
    } else {
        SetCursor(m_cursor->handle());
    }
}

void LWindowsWin::setCustomMargins(const LMargins<int> &newCustomMargins)
{
    if (newCustomMargins != m_data.customMargins) {
        const LMargins<int> oldCustomMargins = m_data.customMargins;
        m_data.customMargins = newCustomMargins;
        // Re-trigger WM_NCALCSIZE with wParam=1 by passing SWP_FRAMECHANGED
        const LRect<int> currentFrameGeometry = frameGeometrySys();
        const LPoint<int> topLeft = currentFrameGeometry.topLeft();
        LRect<int> newFrame = currentFrameGeometry.marginsRemoved(oldCustomMargins) + m_data.customMargins;
        newFrame.moveTo(topLeft);
        SetWindowPos(m_data.hwnd, nullptr, newFrame.m_x, newFrame.m_y, newFrame.width(), newFrame.height(), SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    }
}

void LWindowsWin::checkForScreenChanged(ScreenChangeMode mode)
{
    if (parent() || LWindowsScreenManager::isSingleScreen()){
        return;
    }

    std::shared_ptr<LPlatformScreen> currentScreen = screen();
    const std::shared_ptr<LWindowsScreen> newScreen =
        LWindowsCtx::instance()->screenManager().screenForHwnd(m_data.hwnd);
    if (newScreen == nullptr || newScreen == currentScreen){
        return;
    }
    // For screens with different DPI: postpone until WM_DPICHANGE
    // Check on currentScreen as it can be 0 when resuming a session (QTBUG-80436).
    if (mode == FromGeometryChange && currentScreen != nullptr
        && !equalDpi(currentScreen->logicalDpi(), newScreen->logicalDpi())) {
        return;
    }
    updateFullFrameMargins();
    LWindowSystemInterface::handleWindowScreenChanged(window(),
                                                      newScreen->screen(),
                                                      LDeliverType::Sync);

}

bool LWindowsWin::handleWmPaint(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_ERASEBKGND){ // Backing store - ignored.
        return true;
    }

    std::shared_ptr<LWindow> win = window();
    if(win == nullptr){
        return false;
    }
    // Suppress WM_PAINT sent to invisible windows when setting WS_EX_LAYERED
    if (!win->visible() && (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) != 0){
        return false;
    }
    // Ignore invalid update bounding rectangles
    RECT updateRect;
    if (!GetUpdateRect(m_data.hwnd, &updateRect, FALSE)){
        return false;
    }
    PAINTSTRUCT ps;

    BeginPaint(hwnd, &ps);

    // If the a window is obscured by another window (such as a child window)
    // we still need to send isExposed=true, for compatibility.
    // Our tests depend on it.
    std::shared_ptr<LRegion> region = std::make_shared<LRegion>(lrectFromRECT(ps.rcPaint));
    fireExpose(region, true);
    if (lSizeOfRect(updateRect) == m_data.geometry.size() && !LWindowsCtx::instance()->asyncExpose()){
        LWindowSystemInterface::flushWinSysEvents(LProcessEvents::ExcludeUserInput);
    }

    EndPaint(hwnd, &ps);
    return true;

}

void LWindowsWin::raise()
{
    const LWindowType type = window()->type();
    if (type == LWindowType::Popup
        || type == LWindowType::SubWindow
        || !(window()->flags() & LWindowType::WindowStaysOnBottomHint)) {
        SetWindowPos(handle(), HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
    }
}

void LWindowsWin::lower()
{
    if (!(window()->flags() & LWindowType::WindowStaysOnTopHint)){
        SetWindowPos(handle(), HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
    }
}

void LWindowsWin::requestActivateWindow()
{
    // 'Active' state handling is based in focus since it needs to work for
    // child windows as well.
    if (m_data.hwnd) {
        const DWORD currentThread = GetCurrentThreadId();
        bool attached = false;
        DWORD foregroundThread = 0;

        if (LGuiApplication::applicationState() != LApplicationState::Active) {
            if (const HWND foregroundWindow = GetForegroundWindow()) {
                foregroundThread = GetWindowThreadProcessId(foregroundWindow, nullptr);
                if (foregroundThread && foregroundThread != currentThread){
                    attached = AttachThreadInput(foregroundThread, currentThread, TRUE) == TRUE;
                }
                if (attached) {
                    if (!window()->flags().testFlag(LWindowType::WindowStaysOnBottomHint)
                        && !window()->flags().testFlag(LWindowType::WindowStaysOnTopHint)
                        && window()->type() != LWindowType::ToolTip) {
                        const UINT swpFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER;
                        SetWindowPos(m_data.hwnd, HWND_TOPMOST, 0, 0, 0, 0, swpFlags);
                        SetWindowPos(m_data.hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, swpFlags);
                    }
                }
            }
        }
        SetForegroundWindow(m_data.hwnd);
        SetFocus(m_data.hwnd);
        if (attached){
            AttachThreadInput(foregroundThread, currentThread, FALSE);
        }
    }

}

static inline HRGN createRectRegion(const LRect<int> &r)
{
    return CreateRectRgn(r.m_x, r.m_y, r.x2(), r.y2());
}

static inline void addRectToWinRegion(const LRect<int> &rect, HRGN *winRegion)
{
    if (const HRGN rectRegion = createRectRegion(rect)) {
        HRGN result = CreateRectRgn(0, 0, 0, 0);
        if (CombineRgn(result, *winRegion, rectRegion, RGN_OR)) {
            DeleteObject(*winRegion);
            *winRegion = result;
        }
        DeleteObject(rectRegion);
    }
}

static HRGN lRegionToWinRegion(const std::shared_ptr<LRegion> region)
{
    auto it = region->begin();
    const auto end = region->end();
    if (it == end){
        return nullptr;
    }
    HRGN hRegion = createRectRegion(*it);
    while (++it != end){
        addRectToWinRegion(*it, &hRegion);
    }
    return hRegion;
}

void LWindowsWin::setMask(const std::shared_ptr<LRegion> region)
{
    if (region == nullptr || region->isEmpty()) {
        SetWindowRgn(m_data.hwnd, nullptr, true);
        return;
    }
    const HRGN winRegion = lRegionToWinRegion(region);

    // Mask is in client area coordinates, so offset it in case we have a frame
    if (window()->isTopLevel()) {
        const LMargins<int> margins = fullFrameMargins();
        OffsetRgn(winRegion, margins.m_left, margins.m_top);
    }

    // SetWindowRgn takes ownership.
    if (!SetWindowRgn(m_data.hwnd, winRegion, true)){
        DeleteObject(winRegion);
    }
}

void LWindowsWin::setStyle(unsigned int s) const
{
    setFlag(WithinSetStyle);
    SetWindowLongPtr(m_data.hwnd, GWL_STYLE, s);
    clearFlag(WithinSetStyle);
}

void LWindowsWin::setExStyle(unsigned int s) const
{
    SetWindowLongPtr(m_data.hwnd, GWL_EXSTYLE, s);
}

void LWindowsWin::setOpacity(double value)
{
    if (!lFuzzyCompare(m_opacity, value)) {
        m_opacity = value;
        if (m_data.hwnd){
            std::shared_ptr<LWindow> w =window();
            if(w != nullptr){
                setWindowOpacity(m_data.hwnd, m_data.flags,
                                 w->format().hasAlpha(),
                                 testFlag(OpenGLSurface) || testFlag(VulkanSurface),
                                 value);
            }
        }
    }
}

bool LWindowsWin::isActive() const
{
    if (const HWND activeHwnd = GetForegroundWindow()){
        if (m_data.hwnd == activeHwnd || IsChild(activeHwnd, m_data.hwnd)){
            return true;
        }
    }
    return false;
}

void LWindowsWin::setAlertState(bool enabled)
{
    if (isAlertState() == enabled)
        return;
    if (enabled) {
        alertWindow(0);
        setFlag(AlertState);
    } else {
        stopAlertWindow();
        clearFlag(AlertState);
    }
}

void LWindowsWin::alertWindow(int durationMs)
{
    UINT timeOutMs = GetCaretBlinkTime();
    if (!timeOutMs || timeOutMs == INFINITE){
        timeOutMs = 250;
    }

    FLASHWINFO info;
    info.cbSize = sizeof(info);
    info.hwnd = m_data.hwnd;
    info.dwTimeout = timeOutMs;
    info.dwFlags = FLASHW_TRAY;
    info.uCount = durationMs == 0 ? 10 : UINT(durationMs) / timeOutMs;
    FlashWindowEx(&info);
}

void LWindowsWin::stopAlertWindow()
{
    FLASHWINFO info;
    info.cbSize = sizeof(info);
    info.hwnd = m_data.hwnd;
    info.dwFlags = FLASHW_STOP;
    info.dwTimeout = 0;
    info.uCount = 0;
    FlashWindowEx(&info);
}

bool LWindowsWin::isLayered() const
{
    return GetWindowLongPtr(m_data.hwnd, GWL_EXSTYLE) & WS_EX_LAYERED;
}

bool LWindowsWin::setMouseGrabEnabled(bool grab)
{
    if (!m_data.hwnd) {
        return false;
    }
    if (!isVisible() && grab) {
        return false;
    }
    // release grab or an explicit grab overriding autocapture: Clear flag.
    clearFlag(LWindowsWin::AutoMouseCapture);
    if (hasMouseCapture() != grab) {
        if (grab) {
            SetCapture(m_data.hwnd);
        } else {
            ReleaseCapture();
        }
    }
    return grab;
}


bool LWindowsWin::setKeyboardGrabEnabled(bool grab)
{
    if (!m_data.hwnd) {
        return false;
    }

    LWindowsCtx *context = LWindowsCtx::instance();
    if (grab) {
        context->setKeyGrabber(window());
    } else {
        if (context->keyGrabber() == window())
            context->setKeyGrabber(nullptr);
    }
    return true;

}


bool LWindowsWin::setGeometrySys(const LRect<int> &rect) const
{
    const LMargins<int> margins = fullFrameMargins();
    const LRect<int> frameGeometry = rect.marginsAdded(margins);

    bool result = false;
    const HWND hwnd = handle();
    WINDOWPLACEMENT windowPlacement;
    windowPlacement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(hwnd, &windowPlacement);
    // If the window is hidden and in maximized state or minimized, instead of moving the
    // window, set the normal position of the window.
    if ((windowPlacement.showCmd == SW_MAXIMIZE && !IsWindowVisible(hwnd))
        || windowPlacement.showCmd == SW_SHOWMINIMIZED) {
        windowPlacement.rcNormalPosition =
            RECTfromLRect(frameGeometry.translated(-windowPlacementOffset(hwnd, frameGeometry.topLeft())));
        windowPlacement.showCmd = windowPlacement.showCmd == SW_SHOWMINIMIZED ? SW_SHOWMINIMIZED : SW_HIDE;
        result = SetWindowPlacement(hwnd, &windowPlacement);
    } else {
        result = MoveWindow(hwnd, frameGeometry.m_x, frameGeometry.m_y,
                            frameGeometry.width(), frameGeometry.height(), true);
    }
    return result;
}

LMargins<int> LWindowsWin::frameMarginsSys() const
{
    return LWindowsGeometryHint::frame(handle(), style(), exStyle());
}

bool LWindowsWin::isFullScreenSys() const
{
    const std::shared_ptr<LWindow> w = window();
    if(w!= nullptr){
        if (!w->isTopLevel()){
            return false;
        }
        LRect<int> geometry = geometrySys();
        if (testFlag(HasBorderInFullScreen)){
            geometry = geometry.marginsAdded(LMargins<int>(1, 1, 1, 1));
        }
        std::shared_ptr<LPlatformScreen> screen = screenForGeometry(geometry);
        return screen && geometry == screen->geometry();
    }
    return false;
}

void LWindowsWin::setWindowStateSys(LFlags<LWindowState> newState)
{
    const LFlags<LWindowState> oldState = m_windowState;
    if (oldState == newState){
        return;
    }

    const bool visible = isVisible();
    auto stateChange = oldState ^ newState;

    if (stateChange & LWindowState::FullScreen) {
        if (newState & LWindowState::FullScreen) {
            UINT newStyle = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_POPUP;
            // Save geometry and style to be restored when fullscreen \
            // is turned off again, since on Windows, it is not a real \
            // Window state but emulated by changing geometry and style.
            if (!m_savedStyle) {
                m_savedStyle = style();
                if ((oldState & LWindowState::Minimized) || (oldState & LWindowState::Maximized)) {
                    const LRect<int> nf = normalFrameGeometry(m_data.hwnd);
                    if (nf.isValid()){
                        m_savedFrameGeometry = nf;
                    }
                } else {
                    m_savedFrameGeometry = frameGeometrySys();
                }
            }
            if (newState & LWindowState::Maximized){
                setFlag(MaximizeToFullScreen);
            }
            if (m_savedStyle & WS_SYSMENU){
                newStyle |= WS_SYSMENU;
            }
            if (visible){
                newStyle |= WS_VISIBLE;
            }
            if (testFlag(HasBorderInFullScreen)){
                newStyle |= WS_BORDER;
            }
            setStyle(newStyle);
            // Use geometry of LWindow::screen() within creation or the virtual screen the
            // window is in.
            std::shared_ptr<LWindow> win = window();
            std::shared_ptr<LScreen> screen = win->screen();
            if (screen == nullptr){
                screen = LGuiApplication::primaryScreen();
            }
            const LRect<int> r = screen ? LHighDpi::toNativePixels(screen->m_geometry, win.get()) : m_savedFrameGeometry;

            if (newState & LWindowState::Minimized) {
                setMinimizedGeometry(m_data.hwnd, r);
                if (stateChange & LWindowState::Maximized){
                    setRestoreMaximizedFlag(m_data.hwnd, newState & LWindowState::Maximized);
                }
            } else {
                const UINT swpf = SWP_FRAMECHANGED | SWP_NOACTIVATE;
                const bool wasSync = testFlag(SynchronousGeometryChangeEvent);
                setFlag(SynchronousGeometryChangeEvent);
                SetWindowPos(m_data.hwnd, HWND_TOP, r.m_x, r.m_y, r.width(), r.height(), swpf);
                if (!wasSync)
                    clearFlag(SynchronousGeometryChangeEvent);
                clearFlag(MaximizeToFullScreen);
                LWindowSystemInterface::handleGeometryChange(win, r);
                LWindowSystemInterface::flushWinSysEvents(LProcessEvents::ExcludeUserInput);
            }
        } else {
            // Restore saved state.
            unsigned newStyle = m_savedStyle ? m_savedStyle : style();
            if (visible){
                newStyle |= WS_VISIBLE;
            }
            setStyle(newStyle);

            std::shared_ptr<LWindow> win = window();
            std::shared_ptr<LScreen> screen = win->screen();
            if (screen == nullptr){
                screen = LGuiApplication::primaryScreen();
            }
            // That area of the virtual desktop might not be covered by a screen anymore.
            if (const auto platformScreen = screen->handle()) {
                if (!platformScreen->geometry().isIntersects(m_savedFrameGeometry)){
                    m_savedFrameGeometry.moveTo(platformScreen->geometry().topLeft());
                }
            }

            if (newState & LWindowState::Minimized) {
                setMinimizedGeometry(m_data.hwnd, m_savedFrameGeometry);
                if (stateChange & LWindowState::Maximized){
                    setRestoreMaximizedFlag(m_data.hwnd, newState & LWindowState::Maximized);
                }
            } else {
                UINT swpf = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE;
                if (!m_savedFrameGeometry.isValid()){
                    swpf |= SWP_NOSIZE | SWP_NOMOVE;
                }
                const bool wasSync = testFlag(SynchronousGeometryChangeEvent);
                setFlag(SynchronousGeometryChangeEvent);
                // After maximized/fullscreen; the window can be in a maximized state. Clear
                // it before applying the normal geometry.
                if (windowVisibilitySys(m_data.hwnd) == LWindow::Maximized){
                    ShowWindow(m_data.hwnd, SW_SHOWNOACTIVATE);
                }
                SetWindowPos(m_data.hwnd, nullptr, m_savedFrameGeometry.m_x, m_savedFrameGeometry.m_y,
                             m_savedFrameGeometry.width(), m_savedFrameGeometry.height(), swpf);
                if (!wasSync)
                    clearFlag(SynchronousGeometryChangeEvent);
                // preserve maximized state
                if (visible) {
                    setFlag(WithinMaximize);
                    ShowWindow(m_data.hwnd,
                               (newState & LWindowState::Maximized) ? SW_MAXIMIZE : SW_SHOWNA);
                    clearFlag(WithinMaximize);
                }
            }
            m_savedStyle = 0;
            m_savedFrameGeometry = LRect<int>();
        }
    } else if ((oldState & LWindowState::Maximized) != (newState & LWindowState::Maximized)) {
        if (visible && !(newState & LWindowState::Minimized)) {
            setFlag(WithinMaximize);
            if (newState & LWindowState::FullScreen){
                setFlag(MaximizeToFullScreen);
            }
            ShowWindow(m_data.hwnd,
                       (newState & LWindowState::Maximized) ? SW_MAXIMIZE : SW_SHOWNOACTIVATE);
            clearFlag(WithinMaximize);
            clearFlag(MaximizeToFullScreen);
        } else if (visible && (oldState & newState & LWindowState::Minimized)) {
            // change of the maximized state while keeping minimized
            setRestoreMaximizedFlag(m_data.hwnd, newState & LWindowState::Maximized);
        }
    }

    if (stateChange & LWindowState::Minimized) {
        if (visible) {
            ShowWindow(m_data.hwnd,
                       (newState & LWindowState::Minimized) ? SW_MINIMIZE :
                           (newState & LWindowState::Maximized) ? SW_MAXIMIZE : SW_SHOWNORMAL);
            if ((newState & LWindowState::Minimized) && (stateChange & LWindowState::Maximized)){
                setRestoreMaximizedFlag(m_data.hwnd, newState & LWindowState::Maximized);
            }
        }
    }
}

LRect<int> LWindowsWin::frameGeometrySys() const
{
    return frameGeometry(handle(), isTopLevel());
}

LRect<int> LWindowsWin::geometrySys() const
{
    return frameGeometrySys().marginsRemoved(fullFrameMargins());
}

void LWindowsWin::destroyWindow()
{
    if (m_data.hwnd) { // Stop event dispatching before Window is destroyed.
        setFlag(WithinDestroy);

        std::shared_ptr<LWindow> win = window();
        // Clear any transient child relationships as Windows will otherwise destroy them
        const auto tlw = LGuiApplication::topLevelWindows();
        for (std::shared_ptr<LWindow> w : tlw) {
            if (w->transientParent() == win) {
                if (std::shared_ptr<LWindowsWin> tw = LWindowsWin::windowsWindowOf(w)){
                    tw->updateTransientParent();
                }
            }
        }
        LWindowsCtx *context = LWindowsCtx::instance();
        if (context->windowUnderMouse() == win){
            context->clearWindowUnderMouse();
        }
        if (hasMouseCapture()){
            setMouseGrabEnabled(false);
        }
        setDropSiteEnabled(false);
//#if LK_CONFIG(vulkan)
//        if (m_vkSurface) {
//            LVulkanInstance *inst = window()->vulkanInstance();
//            if (inst)
//                static_cast<LWindowsVulkanInstance *>(inst->handle())->destroySurface(m_vkSurface);
//            m_vkSurface = VK_NULL_HANDLE;
//        }
//#endif
        DestroyWindow(m_data.hwnd);
        context->removeWindow(m_data.hwnd);
        m_data.hwnd = nullptr;
    }
}

void LWindowsWin::destroyIcon()
{
    if (m_iconBig) {
        DestroyIcon(m_iconBig);
        m_iconBig = nullptr;
    }
    if (m_iconSmall) {
        DestroyIcon(m_iconSmall);
        m_iconSmall = nullptr;
    }
}

void LWindowsWin::updateTransientParent() const
{
    std::shared_ptr<LWindow>  win = window();
    if(win != nullptr){
        if (window()->type() == LWindowType::Popup){
            return;
        }
        const HWND oldTransientParent = GetWindow(m_data.hwnd, GW_OWNER);
        HWND newTransientParent = nullptr;
        if (const std::shared_ptr<LWindow> tp = window()->transientParent()){
            if (const std::shared_ptr<LWindowsWin> tw = LWindowsWin::windowsWindowOf(tp)){
                if(tw != nullptr){
                    if (!tw->testFlag(WithinDestroy)){
                        newTransientParent = tw->handle();
                    }
                }
            }
        }

        while (newTransientParent && (GetWindowLongPtr(newTransientParent, GWL_STYLE) & WS_CHILD) != 0){
            newTransientParent = GetParent(newTransientParent);
        }

        if (newTransientParent != oldTransientParent){
#ifdef _WIN64
            SetWindowLongPtr(m_data.hwnd, GWLP_HWNDPARENT, LONG_PTR(newTransientParent));
#else
            SetWindowLongPtr(m_data.hwnd, GWL_HWNDPARENT, LONG_PTR(newTransientParent));
#endif
        }
    }

}

void LWindowsWin::setDropSiteEnabled(bool dropEnabled)
{
    //Todo ole support
}

void LWindowsWin::updateDropSite(bool topLevel)
{
    bool enabled = false;
    bool parentIsEmbedded = false;

    if (!topLevel) {
        // if the parent window is a foreign window wrapped via QWindow::fromWinId, we need to enable the drop site
        // on the first child window
        const std::shared_ptr<LWindow> parent = window()->parent();
        if (parent){
            std::shared_ptr<LPlatformWindow> pWin = parent->handle();
            if(pWin){
                parentIsEmbedded = pWin->isForeignWindow();
            }
        }
    }

    if (topLevel || parentIsEmbedded) {
        switch (window()->type()) {
        case LWindowType::Window:
        case LWindowType::Dialog:
        case LWindowType::Sheet:
        case LWindowType::Drawer:
        case LWindowType::Popup:
        case LWindowType::Tool:
            enabled = true;
            break;
        default:
            break;
        }
    }
    setDropSiteEnabled(enabled);
}

LWindowsWindowData LWindowsWin::setWindowFlagsSys(LFlags<LWindowType> wt, unsigned int flags) const
{
    LWindowCreationData creationData;
    std::shared_ptr<LWindow> win = window();
    creationData.fromWindow(win, wt, flags);
    creationData.applyWindowFlags(m_data.hwnd);
    creationData.initialize(win, m_data.hwnd, true, m_opacity);

    LWindowsWindowData result = m_data;
    result.flags = creationData.flags;
    result.embedded = creationData.embedded;
    return result;
}

void LWindowsWin::fireExpose(const std::shared_ptr<LRegion> region, bool force)
{
    if(force){
        setFlag(Exposed);
    }else{
        if(region != nullptr){
            if(region->isEmpty()){
                clearFlag(Exposed);
            }else{
                setFlag(Exposed);
            }
        }else{
            clearFlag(Exposed);
        }
    }

    std::shared_ptr<LWindow> win = window();
    LWindowSystemInterface::handleExposeEvent(win, region);
}

void LWindowsWin::fireFullExpose(bool force)
{
    std::shared_ptr<LRegion> r= std::make_shared<LRegion>(LRect<int>(0, 0, m_data.geometry.size()));
    fireExpose(r, force);
}

void LWindowsWin::showSys()
{
    int sm = SW_SHOWNORMAL;
    bool fakedMaximize = false;
    bool restoreMaximize = false;
    const std::shared_ptr<LWindow> w = window();
    const LFlags<LWindowType> flags = w->flags();
    const LWindowType type = w->type();
    if (w->isTopLevel()) {
        const LFlags<LWindowState> state = w->windowStates();
        if (state & LWindowState::Minimized) {
            sm = SW_SHOWMINIMIZED;
            if (!isVisible()){
                sm = SW_SHOWMINNOACTIVE;
            }
            if (state & LWindowState::Maximized){
                restoreMaximize = true;
            }
        } else {
            updateTransientParent();
            if (state & LWindowState::Maximized) {
                sm = SW_SHOWMAXIMIZED;
                // Windows will not behave correctly when we try to maximize a window which does not
                // have minimize nor maximize buttons in the window frame. Windows would then ignore
                // non-available geometry, and rather maximize the widget to the full screen, minus the
                // window frame (caption). So, we do a trick here, by adding a maximize button before
                // maximizing the widget, and then remove the maximize button afterwards.
                if ((flags & LWindowType::WindowTitleHint) &&
                    !(flags & (LWindowType::WindowMinMaxButtonsHint | LWindowType::FramelessWindowHint))) {
                    fakedMaximize = TRUE;
                    setStyle(style() | WS_MAXIMIZEBOX);
                }
            }
        }
    }
    if (type == LWindowType::Popup
        || type == LWindowType::ToolTip
        || type == LWindowType::Tool){
        sm = SW_SHOWNOACTIVATE;
    }

    if (w->windowStates() & LWindowState::Maximized){
        setFlag(WithinMaximize);
    }
    ShowWindow(m_data.hwnd, sm);
    clearFlag(WithinMaximize);

    if (fakedMaximize) {
        setStyle(style() & ~WS_MAXIMIZEBOX);
        SetWindowPos(m_data.hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER
                         | SWP_FRAMECHANGED);
    }
    if (restoreMaximize){
        setRestoreMaximizedFlag(m_data.hwnd);
    }
}

void LWindowsWin::hideSys()
{
    SetWindowPos(m_data.hwnd, nullptr , 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

}

void LWindowsWin::handleGeometryChange()
{
    const LRect<int> previousGeometry = m_data.geometry;
    m_data.geometry = geometrySys();

    //lWarn() << "geo change: " << m_data.geometry.m_width << ", " <<  m_data.geometry.m_height;
    if (testFlag(WithinDpiChanged)
        && LWindowsCtx::instance()->screenManager().screenForHwnd(m_data.hwnd) != screen()) {
        return; // LGuiApplication will send resize when screen actually changes
    }
    LWindowSystemInterface::handleGeometryChange(window(), m_data.geometry);
    // OpenGL/normal windows (with exception of ANGLE) do not receive
    // expose events when shrinking, synthesize.
    if (!testFlag(OpenGL_ES2) && isExposed()
        && m_data.geometry.size() != previousGeometry.size() // Exclude plain move
        // One dimension grew -> Windows will send expose, no need to synthesize.
        && !(m_data.geometry.width() > previousGeometry.width() || m_data.geometry.height() > previousGeometry.height())) {
        fireFullExpose(true);
    }

    const bool wasSync = testFlag(SynchronousGeometryChangeEvent);
    checkForScreenChanged();

    if (testFlag(SynchronousGeometryChangeEvent)){
        LWindowSystemInterface::flushWinSysEvents(LProcessEvents::ExcludeUserInput);
    }

    if (!wasSync){
        clearFlag(SynchronousGeometryChangeEvent);
    }
}

void LWindowsWin::handleWindowStateChange(LFlags<LWindowState> state)
{
    m_windowState = state;
    std::shared_ptr<LWindow> w = window();
    LWindowSystemInterface::handleWindowStateChanged(w, state);
    if (state & LWindowState::Minimized) {
        handleHidden();
        LWindowSystemInterface::flushWinSysEvents(LProcessEvents::ExcludeUserInput);
    } else {
        // layered windows and transient children, we won't receive any WM_Paint.
        bool exposeEventsSent = false;
        if (isLayered()) {
            fireFullExpose();
            exposeEventsSent = true;
        }
        const std::vector<std::shared_ptr<LWindow> > allWindows = LGuiApplication::allWindows();
        for (std::shared_ptr<LWindow> child : allWindows) {
            if (child != w && child->visible() && child->transientParent() == w) {
                std::shared_ptr<LWindowsWin> platformWindow = LWindowsWin::windowsWindowOf(child);
                if (platformWindow && platformWindow->isLayered()) {
                    platformWindow->fireFullExpose();
                    exposeEventsSent = true;
                }
            }
        }
        if (exposeEventsSent && !LWindowsCtx::instance()->asyncExpose()){
            LWindowSystemInterface::flushWinSysEvents(LProcessEvents::ExcludeUserInput);
        }
    }
}

bool LWindowsWin::handleGeometryChangingMessage(MSG *message, const std::shared_ptr<LWindow> w, const LMargins<int> &margins)
{
    if(w==nullptr){
        return false;
    }

    auto *windowPos = reinterpret_cast<WINDOWPOS *>(message->lParam);
    if ((windowPos->flags & SWP_NOZORDER) == 0) {
        std::shared_ptr<LWindowsWin> pw = LWindowsWin::windowsWindowOf(w);
        if (pw != nullptr) {
            std::shared_ptr<LWindow>  parentWindow = w->parent();
            HWND parentHWND = GetAncestor(windowPos->hwnd, GA_PARENT);
            HWND desktopHWND = GetDesktopWindow();

            pw->m_data.embedded = (parentWindow!=nullptr) && parentHWND && (parentHWND != desktopHWND);
        }
        if (w->flags().testFlag(LWindowType::WindowStaysOnBottomHint)){
            windowPos->hwndInsertAfter = HWND_BOTTOM;
        }
    }
    if (!w->isTopLevel()){ // Implement hasHeightForWidth().
        return false;
    }
    if ((windowPos->flags & (SWP_NOCOPYBITS | SWP_NOSIZE))){
        return false;
    }
    const LRect<int> suggestedFrameGeometry(windowPos->x, windowPos->y,
                                            LSize<int>(windowPos->cx, windowPos->cy));
    const LRect<double> suggestedGeometry = (suggestedFrameGeometry-margins).toRectF();
    const LRect<double> correctedGeometryF = LPlatformWindow::closestAcceptableGeometry(w, suggestedGeometry);
    if (!correctedGeometryF.isValid()){
        return false;
    }
    const LRect<int> correctedFrameGeometry = correctedGeometryF.toRect() + margins;
    if (correctedFrameGeometry == suggestedFrameGeometry){
        return false;
    }
    windowPos->x = correctedFrameGeometry.m_x;
    windowPos->y = correctedFrameGeometry.m_y;
    windowPos->cx = correctedFrameGeometry.m_width;
    windowPos->cy = correctedFrameGeometry.m_height;
    return true;

}

bool LWindowsWin::handleGeometryChanging(MSG *message) const
{
    const LMargins<int> margins = window()->isTopLevel() ? fullFrameMargins() : LMargins<int>();
    return LWindowsWin::handleGeometryChangingMessage(message, window(), margins);
}

bool LWindowsWin::handleNonClientHitTest(const LPoint<int> &globalPos, LRESULT *result) const
{
    const std::shared_ptr<LWindow> w = window();
    if (!w->isTopLevel() // Minimized windows need to respond to user input.
        || (m_windowState != LWindowState::NoState)
        || !isActive()
        || (m_data.flags & LWindowType::FramelessWindowHint)) {
        return false;
    }
    const LSize<int> minimumSize = w->minimumSize();
    if (minimumSize.isEmpty()){
        return false;
    }
    const LSize<int> maximumSize = w->maximumSize();
    const bool fixedWidth = minimumSize.width() == maximumSize.width();
    const bool fixedHeight = minimumSize.height() == maximumSize.height();
    if (!fixedWidth && !fixedHeight){
        return false;
    }
    const LPoint<int> localPos = w->mapFromGlobal(LHighDpi::fromNativePixels(globalPos, w.get()));
    const LSize<int> size = w->size();
    if (fixedHeight) {
        if (localPos.m_y >= size.height()) {
            *result = HTBORDER; // Unspecified border, no resize cursor.
            return true;
        }
        if (localPos.m_y < 0) {
            // We want to return HTCAPTION/true only over the outer sizing frame, not the entire title bar,
            // otherwise the title bar buttons (close, etc.) become unresponsive on Windows 7.
            // However, neither frameMargins() nor GetSystemMetrics(SM_CYSIZEFRAME), etc., give the correct
            // sizing frame height in all Windows versions/scales. This empirical constant seems to work, though.
            const int sizingHeight = 9;
            const int topResizeBarPos = sizingHeight - frameMargins().m_top;
            if (localPos.m_y < topResizeBarPos) {
                *result = HTCAPTION; // Extend caption over top resize bar, let's user move the window.
                return true;
            }
        }
    }
    if (fixedWidth && (localPos.m_x < 0 || localPos.m_x >= size.width())) {
        *result = HTBORDER; // Unspecified border, no resize cursor.
        return true;
    }
    return false;
}

LK_NAMESPACE_END



