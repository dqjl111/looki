#ifndef LWINDOWSWIN_H
#define LWINDOWSWIN_H

#include "lwindows.h"
#include "lkgui/lwindow.h"
#include "lkgui/lplatformwindow.h"
#include "lkgui/lrect.h"
#include "lkgui/osspec/win/lwindowscursor.h"

LK_NAMESPACE_BEGIN
struct LWindowsGeometryHint
{
    static LMargins<int> frameOnPrimaryScreen(DWORD style, DWORD exStyle);
    static LMargins<int> frameOnPrimaryScreen(HWND hwnd);
    static LMargins<int> frame(DWORD style, DWORD exStyle, double dpi);
    static LMargins<int> frame(HWND hwnd, DWORD style, DWORD exStyle);
    static LMargins<int> frame(const std::shared_ptr<LWindow> w,
                               const LRect<int> &geometry,
                               DWORD style,
                               DWORD exStyle);
    static bool handleCalculateSize(const LMargins<int> &customMargins, const MSG &msg, LRESULT *result);
    static void applyToMinMaxInfo(const std::shared_ptr<LWindow> w,
                                  const std::shared_ptr<LScreen> screen,
                                  const LMargins<int> &margins, MINMAXINFO *mmi);
    static void applyToMinMaxInfo(const std::shared_ptr<LWindow> w,
                                  const LMargins<int> &margins,
                                  MINMAXINFO *mmi);
    static void frameSizeConstraints(const std::shared_ptr<LWindow> w,
                                     const std::shared_ptr<LScreen> screen,
                                     const LMargins<int> &margins,
                                     LSize<int> *minimumSize, LSize<int> *maximumSize);
    static LPoint<int> mapToGlobal(HWND hwnd, const LPoint<int> &);
    static LPoint<int> mapToGlobal(const std::shared_ptr<LWindow> w, const LPoint<int> &);
    static LPoint<int> mapFromGlobal(const HWND hwnd, const LPoint<int> &);
    static LPoint<int> mapFromGlobal(const std::shared_ptr<LWindow> w, const LPoint<int> &);

    static bool positionIncludesFrame(const std::shared_ptr<LWindow> w);
};


struct LWindowCreationCtx
{
    explicit LWindowCreationCtx(const std::shared_ptr<LWindow> w,
                                const std::shared_ptr<LScreen> s,
                                const LRect<int> &geometryIn,
                                const LRect<int> &geometry,
                                const LMargins<int> &customMargins,
                                DWORD style,
                                DWORD exStyle);
    void applyToMinMaxInfo(MINMAXINFO *mmi) const;

    std::shared_ptr<LWindow> window;
    std::shared_ptr<LScreen> screen;
    LRect<int> requestedGeometryIn; // LWindow scaled
    LRect<int> requestedGeometry; // after LPlatformWindow::initialGeometry()
    LPoint<int> obtainedPos;
    LSize<int> obtainedSize;
    LMargins<int> margins;
    LMargins<int> customMargins;  // User-defined, additional frame for WM_NCCALCSIZE
    int frameX = CW_USEDEFAULT; // Passed on to CreateWindowEx(), including frame.
    int frameY = CW_USEDEFAULT;
    int frameWidth = CW_USEDEFAULT;
    int frameHeight = CW_USEDEFAULT;
    int menuHeight = 0;
};


struct LWindowsWindowData
{
    LFlags<LWindowType> flags;
    LRect<int> geometry;
    LMargins<int> fullFrameMargins; // Do not use directly for windows, see FrameDirty.
    LMargins<int> customMargins;    // User-defined, additional frame for NCCALCSIZE
    HWND hwnd = nullptr;
    bool embedded = false;
    bool hasFrame = false;

    static LWindowsWindowData create(const std::shared_ptr<LWindow> w,
                                     const LWindowsWindowData &parameters,
                                     const std::wstring &title);
};

enum class HBitmapFormat
{
    NoAlpha,
    PremultipliedAlpha,
    Alpha
};

class LWindowsWin : public LPlatformWindow
{
public:
    LWindowsWin(std::shared_ptr<LWindow> window, const LWindowsWindowData &data);
    virtual ~LWindowsWin();
    virtual void initialize() override;

    static HICON pixmapToWinHICON(const std::shared_ptr<LPixmap> p);
    static HBITMAP imageToWinHBITMAP(std::shared_ptr<LImage> imageIn,
                                     HBitmapFormat hbitmapFormat = HBitmapFormat::NoAlpha);

    static void clientToScreen(HWND hwnd, POINT *wP);
    static void screenToClient(HWND hwnd, POINT *wP);


    enum Flags
    {
        AutoMouseCapture = 0x1, //! Automatic mouse capture on button press.
        WithinSetParent = 0x2,
        WithinSetGeometry = 0x8,
        OpenGLSurface = 0x10,
        OpenGL_ES2 = 0x20,
        OpenGLDoubleBuffered = 0x40,
        OpenGlPixelFormatInitialized = 0x80,
        BlockedByModal = 0x100,
        SizeGripOperation = 0x200,
        FrameStrutEventsEnabled = 0x400,
        SynchronousGeometryChangeEvent = 0x800,
        WithinSetStyle = 0x1000,
        WithinDestroy = 0x2000,
        TouchRegistered = 0x4000,
        AlertState = 0x8000,
        Exposed = 0x10000,
        WithinCreate = 0x20000,
        WithinMaximize = 0x40000,
        MaximizeToFullScreen = 0x80000,
        Compositing = 0x100000,
        HasBorderInFullScreen = 0x200000,
        WithinDpiChanged = 0x400000,
        VulkanSurface = 0x800000,
        ResizeMoveActive = 0x1000000,
        DisableNonClientScaling = 0x2000000
    };

    LMargins<int> frameMargins() const;
    LMargins<int> fullFrameMargins() const;
    void setFullFrameMargins(const LMargins<int> &newMargins);
    void updateFullFrameMargins();

    virtual void setGeometry(const LRect<int> &rect) override;
    virtual const LRect<int>& geometry() const override { return m_data.geometry; }
    virtual LRect<int> normalGeometry() const override;

    virtual void setWindowFlags(LFlags<LWindowType> flags) override;
    virtual void setWindowStates(LFlags<LWindowState> state) override;

    HWND handle() const;
    bool isTopLevel() const;

    inline bool testFlag(unsigned f) const  { return (m_flags & f) != 0; }
    inline void setFlag(unsigned f) const   { m_flags |= f; }
    inline void clearFlag(unsigned f) const { m_flags &= ~f; }

    virtual void setVisible(bool visible) override;
    bool isVisible() const;

    HDC getDC();
    void releaseDC();

    static std::shared_ptr<LWindowsWin> baseWindowOf(const std::shared_ptr<LWindow> w);
    static std::shared_ptr<LWindowsWin> windowsWindowOf(const std::shared_ptr<LWindow> w);

    unsigned style() const   { return GetWindowLongPtr(handle(), GWL_STYLE); }
    unsigned exStyle() const { return GetWindowLongPtr(handle(), GWL_EXSTYLE); }

    void setStyle(unsigned s) const;
    void setExStyle(unsigned s) const;

    virtual bool isEmbedded() const override{return m_data.embedded;}

    virtual bool isAncestorOf(const std::shared_ptr<LPlatformWindow> child) const override;
    virtual void setParent(const std::shared_ptr<LPlatformWindow> window) override;
    virtual void setWindowTitle(const std::wstring &title) override;
    virtual void setWindowIcon(const std::shared_ptr<LTexture> icon) override;

    virtual void setOpacity(double value) override;
    inline double opacity() const { return m_opacity; }

    virtual bool isActive() const override;
    virtual bool isExposed() const override { return testFlag(Exposed); }

    virtual void setAlertState(bool enabled) override;
    virtual bool isAlertState() const override { return testFlag(AlertState); }
    void alertWindow(int durationMs = 0);
    void stopAlertWindow();

    bool isLayered() const;
    virtual bool setMouseGrabEnabled(bool grab) override;
    virtual bool setKeyboardGrabEnabled(bool grab) override;
    inline bool hasMouseCapture() const { return GetCapture() == m_data.hwnd; }


    static HWND handleOf(const std::shared_ptr<LWindow> w);
    static std::shared_ptr<LWindow> topLevelOf(std::shared_ptr<LWindow> w);


    static bool isRtlLayout(HWND hwnd);
    static std::shared_ptr<LScreen> forcedScreenForGLWindow(const std::shared_ptr<LWindow> w);
    static bool setWindowLayered(HWND hwnd, LFlags<LWindowType> flags, bool hasAlpha, double opacity);
    static bool handleGeometryChangingMessage(MSG *message,
                                              const std::shared_ptr<LWindow> w ,
                                              const LMargins<int> &marginsDp);
    bool handleGeometryChanging(MSG *message) const;
    bool handleNonClientHitTest(const LPoint<int> &globalPos, LRESULT *result) const;

    void handleMoved();
    void handleResized(int wParam);
    void handleHidden();

    void getSizeHints(MINMAXINFO *mmi) const;

    void setCursor(const std::shared_ptr<LCursorHandle> c);
    inline std::shared_ptr<LCursorHandle> cursor() const { return m_cursor; }
    void applyCursor();

    LMargins<int> customMargins() const { return m_data.customMargins; }
    void setCustomMargins(const LMargins<int> &m);

    enum ScreenChangeMode { FromGeometryChange, FromDpiChange };
    void checkForScreenChanged(ScreenChangeMode mode = FromGeometryChange);
    bool handleWmPaint(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    virtual void raise() override;
    virtual void lower() override;
    virtual void requestActivateWindow() override;

    void setMask(const std::shared_ptr<LRegion> region) override;
protected:
    HWND parentHwnd() const { return GetAncestor(handle(), GA_PARENT); }
    LWindowsWindowData m_data;
    mutable unsigned int m_flags = 0;
    LFlags<LWindowState> m_windowState = LWindowState::NoState;
    HDC m_hdc = nullptr;
    double m_opacity = 1;

    HICON m_iconSmall = nullptr;
    HICON m_iconBig = nullptr;

    unsigned m_savedStyle = 0;
    LRect<int> m_savedFrameGeometry;
    std::shared_ptr<LCursorHandle> m_cursor;

    static bool m_screenForGLInitialized;

    bool isTopLevelSys() const;
    LRect<int> frameGeometrySys() const;
    LRect<int> geometrySys() const;
    bool setGeometrySys(const LRect<int> &rect) const;
    LMargins<int> frameMarginsSys() const;
    bool isFullScreenSys() const;
    void setWindowStateSys(LFlags<LWindowState> newState);

    void destroyWindow();
    void destroyIcon();
    void updateTransientParent() const;

    void setDropSiteEnabled(bool dropEnabled);
    void updateDropSite(bool topLevel);
    LWindowsWindowData setWindowFlagsSys(LFlags<LWindowType> wt, unsigned flags = 0) const;

    void fireExpose(const std::shared_ptr<LRegion> region, bool force=false);
    void fireFullExpose(bool force=false);

    void showSys();
    void hideSys();
    void handleGeometryChange();
    void handleWindowStateChange(LFlags<LWindowState> state);
};

LK_NAMESPACE_END

#endif // LWINDOWSWIN_H
