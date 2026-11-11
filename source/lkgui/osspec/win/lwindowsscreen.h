#ifndef LWINDOWSSCREEN_H
#define LWINDOWSSCREEN_H
#include "lkgui/lplatformscreen.h"
#include "lkgui/lrect.h"
#include <string>
#include "lwindows.h"
#include "lkgui/lscreen.h"
#include "lkgui/osspec/win/lwindowscursor.h"
#include "lkgui/image/limage.h"

LK_NAMESPACE_BEGIN

struct LWindowsScreenData
{
    enum Flags
    {
        PrimaryScreen = 0x1,
        VirtualDesktop = 0x2,
        LockScreen = 0x4 // Temporary screen existing during user change, etc.
    };

    LRect<int> geometry;
    LRect<int> availableGeometry;
    std::pair<double, double> dpi{96, 96};
    LSize<double> physicalSizeMM;
    int depth = 32;
    LImage::Format format = LImage::Format::ARGB32Premultiplied;
    unsigned flags = VirtualDesktop;
    std::wstring name;
    LScreenOrientation orientation = LScreenOrientation::Landscape;
    double refreshRateHz = 60;
    HMONITOR hMonitor = nullptr;
};

class LWindowsScreen : public LPlatformScreen
{
public:
    explicit LWindowsScreen(const LWindowsScreenData &data);

    //using CursorPtr = QScopedPointer<QPlatformCursor>;

    virtual LRect<int> geometry() const override { return m_data.geometry; }
    virtual LRect<int> availableGeometry() const override { return m_data.availableGeometry; }
    virtual int depth() const override { return m_data.depth; }
    //QImage::Format format() const override { return m_data.format; }
    virtual LSize<double> physicalSize() const override { return m_data.physicalSizeMM; }
    virtual std::pair<double, double> logicalDpi() const override { return m_data.dpi; }
    virtual std::pair<double, double> logicalBaseDpi() const override { return std::pair<double, double>(96, 96); }
    virtual double devicePixelRatio() const override { return 1.0; }
    virtual double refreshRate() const override { return m_data.refreshRateHz; }
    virtual std::wstring name() const override { return m_data.name; }
    virtual LScreenOrientation orientation() const override { return m_data.orientation; }

    static LScreenOrientation orientationPreference();
    static bool setOrientationPreference(LScreenOrientation o);

    static std::shared_ptr<LWindow> windowAt(const LPoint<int> &point, unsigned flags);

    inline void handleChanges(const LWindowsScreenData &newData);

    HMONITOR handle() const;

    virtual std::shared_ptr<LPlatformCursor> cursor() const override;
    virtual std::shared_ptr<LWindow> topLevelAt(const LPoint<int> & pos) const override;

    const LWindowsScreenData &data() const  { return m_data; }

    static LRect<int> virtualGeometry(const LPlatformScreen *screen);

private:
    LWindowsScreenData m_data;
    std::shared_ptr<LWindowsCursor> m_cursor;
};


class LWindowsScreenManager
{
public:
    using WindowsScreens = std::deque<std::shared_ptr<LWindowsScreen> >;

    LWindowsScreenManager() = default;

    void clearScreens();

    bool handleScreenChanges();
    bool handleDisplayChange(WPARAM wParam, LPARAM lParam);
    const WindowsScreens &screens() const { return m_screens; }

    const std::shared_ptr<LWindowsScreen> screenAtDp(const LPoint<int> &p) const;
    const std::shared_ptr<LWindowsScreen> screenForHwnd(HWND hwnd) const;

    static bool isSingleScreen();

private:
    void removeScreen(int index);

    WindowsScreens m_screens;
    int m_lastDepth = -1;
    WORD m_lastHorizontalResolution = 0;
    WORD m_lastVerticalResolution = 0;
};

LK_NAMESPACE_END
#endif // LWINDOWSSCREEN_H
