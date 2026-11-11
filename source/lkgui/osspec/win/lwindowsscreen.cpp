#include "lwindowsscreen.h"
#include "lwindowsctx.h"
#include "lkgui/lguiapplication.h"
#include "lkgui/lwindowsysteminterface.h"
#include "lwindowswin.h"
#include "lwindowsfontdatabase.h"
#include "lkcore/ldebug.h"
LK_NAMESPACE_BEGIN


// Move a window to a new virtual screen, accounting for varying sizes.
static void moveToVirtualScreen(std::shared_ptr<LWindow> w,
                                const std::shared_ptr<LScreen> newScreen)
{
    LRect<int> geometry = w->geometry();
    const LRect<int> oldScreenGeometry = w->screen()->m_geometry;
    const LRect<int> newScreenGeometry = newScreen->m_geometry;
    LPoint<int> relativePosition = geometry.topLeft() - oldScreenGeometry.topLeft();
    if (oldScreenGeometry.size() != newScreenGeometry.size()) {
        const double factor =
            double(LPoint<int>(newScreenGeometry.width(), newScreenGeometry.height()).manhattanLength()) /
            double(LPoint<int>(oldScreenGeometry.width(), oldScreenGeometry.height()).manhattanLength());
        relativePosition = (relativePosition.toPointF()* factor).toPoint();
    }
    geometry.moveTopLeft(relativePosition);
    w->setGeometry(geometry);
}

static inline std::pair<double, double> deviceDPI(HDC hdc)
{
    return std::pair<double, double>(GetDeviceCaps(hdc, LOGPIXELSX),
                                     GetDeviceCaps(hdc, LOGPIXELSY));
}

static inline std::pair<double, double> monitorDPI(HMONITOR hMonitor)
{
    if (LWindowsCtx::shcoredll.isValid()) {
        UINT dpiX;
        UINT dpiY;
        if (SUCCEEDED(LWindowsCtx::shcoredll.getDpiForMonitor(hMonitor, 0, &dpiX, &dpiY)))
            return std::pair<double, double>(dpiX, dpiY);
    }
    return {0, 0};
}



using WindowsScreenDataList = std::deque<LWindowsScreenData>;

static bool monitorData(HMONITOR hMonitor, LWindowsScreenData *data)
{
    MONITORINFOEXW info;
    memset(&info, 0, sizeof(MONITORINFOEXW));
    info.cbSize = sizeof(MONITORINFOEXW);
    if (GetMonitorInfoW(hMonitor, &info) == FALSE){
        return false;
    }

    data->hMonitor = hMonitor;
    data->geometry = LRect<int>(LPoint<int>(info.rcMonitor.left, info.rcMonitor.top),
                                LPoint<int>(info.rcMonitor.right, info.rcMonitor.bottom));
    data->availableGeometry = LRect<int>(LPoint<int>(info.rcWork.left, info.rcWork.top),
                                         LPoint<int>(info.rcWork.right, info.rcWork.bottom));
    data->name = info.szDevice;
    if (data->name == L"WinDisc") {
        data->flags |= LWindowsScreenData::LockScreen;
    } else {
        if (const HDC hdc = CreateDCW(info.szDevice, nullptr, nullptr, nullptr)) {
            const std::pair<double, double> dpi = monitorDPI(hMonitor);
            data->dpi = dpi.first > 0 ? dpi : deviceDPI(hdc);
            data->depth = GetDeviceCaps(hdc, BITSPIXEL);
            data->format = data->depth == 16 ? LImage::Format::RGB16 : LImage::Format::RGB32;
            data->physicalSizeMM = LSize<double>(GetDeviceCaps(hdc, HORZSIZE),
                                                GetDeviceCaps(hdc, VERTSIZE));
            const int refreshRate = GetDeviceCaps(hdc, VREFRESH);
            if (refreshRate > 1) // 0,1 means hardware default.
                data->refreshRateHz = refreshRate;
            DeleteDC(hdc);
        } else {
            lWarn() << "Unable to obtain handle for monitor " << info.szDevice;
        } // CreateDC() failed
    } // not lock screen
    data->orientation = data->geometry.height() > data->geometry.width() ?
                            LScreenOrientation::Portrait : LScreenOrientation::Landscape;
    // EnumDisplayMonitors (as opposed to EnumDisplayDevices) enumerates only
    // virtual desktop screens.
    data->flags |= LWindowsScreenData::VirtualDesktop;
    if (info.dwFlags & MONITORINFOF_PRIMARY) {
        data->flags |= LWindowsScreenData::PrimaryScreen;
        if ((data->flags & LWindowsScreenData::LockScreen) == 0){
            LWindowsFontDatabase::setDefaultVerticalDPI(data->dpi.second);
        }
    }
    return true;
}

BOOL __stdcall monitorEnumCallback(HMONITOR hMonitor, HDC, LPRECT, LPARAM p)
{
    LWindowsScreenData data;
    if (monitorData(hMonitor, &data)) {
        auto *result = reinterpret_cast<WindowsScreenDataList *>(p);
        if (data.flags & LWindowsScreenData::PrimaryScreen){
            result->push_front(data);
        }
        else{
            result->push_back(data);
        }
    }
    return TRUE;
}

static inline WindowsScreenDataList monitorData()
{
    WindowsScreenDataList result;
    EnumDisplayMonitors(nullptr, nullptr, monitorEnumCallback, reinterpret_cast<LPARAM>(&result));
    return result;
}

LWindowsScreen::LWindowsScreen(const LWindowsScreenData &data) : m_data(data)
{

}

std::shared_ptr<LWindow> LWindowsScreen::windowAt(const LPoint<int> &screenPoint, unsigned int flags)
{
    std::shared_ptr<LPlatformWindow> bw =
        LWindowsCtx::instance()->findPlatformWindowAt(GetDesktopWindow(), screenPoint, flags);
    if (bw != nullptr){
        return bw->window();
    }
    return nullptr;
}

void LWindowsScreen::handleChanges(const LWindowsScreenData &newData)
{
    m_data.physicalSizeMM = newData.physicalSizeMM;

    if (m_data.hMonitor != newData.hMonitor) {
        m_data.hMonitor = newData.hMonitor;
    }

    // QGuiApplicationPrivate::processScreenGeometryChange() checks and emits
    // DPI and orientation as well, so, assign new values and emit DPI first.
    const bool geometryChanged = m_data.geometry != newData.geometry
                                 || m_data.availableGeometry != newData.availableGeometry;
    const bool dpiChanged = !lFuzzyCompare(m_data.dpi.first, newData.dpi.first)
                            || !lFuzzyCompare(m_data.dpi.second, newData.dpi.second);
    const bool orientationChanged = m_data.orientation != newData.orientation;
    m_data.dpi = newData.dpi;
    m_data.orientation = newData.orientation;
    m_data.geometry = newData.geometry;
    m_data.availableGeometry = newData.availableGeometry;

    if (dpiChanged) {
        LWindowSystemInterface::handleScreenLogicalDotsPerInchChange(screen(),
                                                                     newData.dpi.first,
                                                                     newData.dpi.second);
    }
    if (orientationChanged){
        LWindowSystemInterface::handleScreenOrientationChange(screen(), newData.orientation);
    }
    if (geometryChanged) {
        LWindowSystemInterface::handleScreenGeometryChange(screen(),
                                                           newData.geometry, newData.availableGeometry);
    }

}

std::shared_ptr<LPlatformCursor> LWindowsScreen::cursor() const
{
    if(m_cursor == nullptr){
        const_cast<LWindowsScreen*>(this)->m_cursor = std::make_shared<LWindowsCursor>(getPtrT<LWindowsScreen>());
    }
    return m_cursor;
}

std::shared_ptr<LWindow> LWindowsScreen::topLevelAt(const LPoint<int> &pos) const
{
    std::shared_ptr<LWindow> result = nullptr;
    if (std::shared_ptr<LWindow> child = LWindowsScreen::windowAt(pos, CWP_SKIPINVISIBLE)){
        result = LWindowsWin::topLevelOf(child);
    }
    return result;
}


void LWindowsScreenManager::clearScreens()
{
    while (!m_screens.empty()){
        std::shared_ptr<LWindowsScreen>  t = m_screens.back();
        LWindowSystemInterface::handleScreenRemoved(t);
        m_screens.pop_back();
    }
}

static inline int indexOfMonitor(const LWindowsScreenManager::WindowsScreens &screens,
                                 const std::wstring &monitorName)
{
    int i=0;
    for (const std::shared_ptr<LWindowsScreen> data : screens) {
        if (data->name() == monitorName){
            return i;
        }
        ++i;
    }
    return -1;
}

static inline int indexOfMonitor(const WindowsScreenDataList &screenData,
                                 const std::wstring &monitorName)
{
    int i=0;
    for (const LWindowsScreenData &data : screenData) {
        if (data.name == monitorName){
            return i;
        }
        ++i;
    }
    return -1;
}

bool LWindowsScreenManager::handleScreenChanges()
{
    // Look for changed monitors, add new ones
    const WindowsScreenDataList newDataList = monitorData();
    const bool lockScreen = newDataList.size() == 1 && (newDataList.front().flags & LWindowsScreenData::LockScreen);
    bool primaryScreenChanged = false;
    for (const LWindowsScreenData &newData : newDataList) {
        const int existingIndex = indexOfMonitor(m_screens, newData.name);
        if (existingIndex != -1) {
            m_screens.at(existingIndex)->handleChanges(newData);
            if (existingIndex == 0)
                primaryScreenChanged = true;
        } else {
            std::shared_ptr<LWindowsScreen> newScreen = std::make_shared<LWindowsScreen>(newData);
            m_screens.push_back(newScreen);
            LWindowSystemInterface::handleScreenAdded(newScreen,
                                                      newData.flags & LWindowsScreenData::PrimaryScreen);
        }    // exists
    }        // for new screens.
    // Remove deleted ones but keep main monitors if we get only the
    // temporary lock screen to avoid window recreation.
    if (!lockScreen) {
        for (int i = m_screens.size() - 1; i >= 0; --i) {
            if (indexOfMonitor(newDataList, m_screens.at(i)->data().name) == -1){
                removeScreen(i);
            }
        }     // for existing screens
    }     // not lock screen
    if (primaryScreenChanged) {
//        if (auto theme = LWindowsTheme::instance()){
//            theme->refreshFonts();
//        }
    }
    return true;
}

bool LWindowsScreenManager::handleDisplayChange(WPARAM wParam, LPARAM lParam)
{
    return true;
}

const std::shared_ptr<LWindowsScreen> LWindowsScreenManager::screenAtDp(const LPoint<int> &p) const
{
    for (std::shared_ptr<LWindowsScreen> scr : m_screens) {
        if (scr->geometry().contains(p)){
            return scr;
        }
    }
    return nullptr;
}

const std::shared_ptr<LWindowsScreen> LWindowsScreenManager::screenForHwnd(HWND hwnd) const
{
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (hMonitor == nullptr){
        return nullptr;
    }
    for(const std::shared_ptr<LWindowsScreen> s: m_screens){
        if(s->data().hMonitor == hMonitor
                   && (s->data().flags & LWindowsScreenData::VirtualDesktop)){
            return s;
        }
    }
    return nullptr;
}

bool LWindowsScreenManager::isSingleScreen()
{
    return LWindowsCtx::instance()->screenManager().screens().size() < 2;
}

void LWindowsScreenManager::removeScreen(int index)
{
    std::shared_ptr<LWindowsScreen> winScreen = m_screens.at(index);
    std::shared_ptr<LScreen>  screen = winScreen->screen();
    std::shared_ptr<LScreen>  primaryScreen = LGuiApplication::primaryScreen();

    if (screen != primaryScreen) {
        unsigned movedWindowCount = 0;
        const std::vector<std::shared_ptr<LWindow> > tlws = LGuiApplication::topLevelWindows();
        for (std::shared_ptr<LWindow> w : tlws) {
            if (w->screen() == screen && w->handle() && w->type() != LWindowType::Desktop) {
                std::shared_ptr<LWindowsWin> windowWin = LWindowsWin::baseWindowOf(w);

                if (w->visible() && w->windowState() != LWindowState::Minimized
                    && (windowWin->exStyle() & WS_EX_TOOLWINDOW)) {
                    moveToVirtualScreen(w, primaryScreen);
                } else {
                    LWindowSystemInterface::handleWindowScreenChanged(w, primaryScreen);
                }
                ++movedWindowCount;
            }
        }
        if (movedWindowCount){
            LWindowSystemInterface::flushWinSysEvents();
        }
    }

    m_screens.erase(m_screens.begin()+index);
    LWindowSystemInterface::handleScreenRemoved(winScreen);
}

LK_NAMESPACE_END




