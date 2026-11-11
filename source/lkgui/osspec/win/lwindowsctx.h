#ifndef LWINDOWSCTX_H
#define LWINDOWSCTX_H

#define STRICT_TYPED_ITEMIDS
#include <shlobj.h>
#include <shlwapi.h>
#include "lkgui/lwindow.h"
#include "lwindows.h"
#include <set>
#include <string>
#include "lkcore/lglobal.h"
#include <unordered_map>
#include "lkgui/lrect.h"
#include "lwindowsscreen.h"
#include "lwindowswin.h"
#include "lwindowskeymapper.h"
#include "lwindowsmousehandler.h"
#include "lwindowspointerhandler.h"

LK_NAMESPACE_BEGIN

class LWindowsWin;
struct LWindowsUser32DLL
{
    inline void init();
    inline bool supportsPointerApi();

    typedef BOOL (WINAPI *EnableMouseInPointer)(BOOL);
    typedef BOOL (WINAPI *GetPointerType)(UINT32, PVOID);
    typedef BOOL (WINAPI *GetPointerInfo)(UINT32, PVOID);
    typedef BOOL (WINAPI *GetPointerDeviceRects)(HANDLE, RECT *, RECT *);
    typedef BOOL (WINAPI *GetPointerTouchInfo)(UINT32, PVOID);
    typedef BOOL (WINAPI *GetPointerFrameTouchInfo)(UINT32, UINT32 *, PVOID);
    typedef BOOL (WINAPI *GetPointerFrameTouchInfoHistory)(UINT32, UINT32 *, UINT32 *, PVOID);
    typedef BOOL (WINAPI *GetPointerPenInfo)(UINT32, PVOID);
    typedef BOOL (WINAPI *GetPointerPenInfoHistory)(UINT32, UINT32 *, PVOID);
    typedef BOOL (WINAPI *SkipPointerFrameMessages)(UINT32);
    typedef BOOL (WINAPI *SetProcessDPIAware)();
    typedef BOOL (WINAPI *AddClipboardFormatListener)(HWND);
    typedef BOOL (WINAPI *RemoveClipboardFormatListener)(HWND);
    typedef BOOL (WINAPI *GetDisplayAutoRotationPreferences)(DWORD *);
    typedef BOOL (WINAPI *SetDisplayAutoRotationPreferences)(DWORD);
    typedef BOOL (WINAPI *AdjustWindowRectExForDpi)(LPRECT,DWORD,BOOL,DWORD,UINT);
    typedef BOOL (WINAPI *EnableNonClientDpiScaling)(HWND);
    typedef int  (WINAPI *GetWindowDpiAwarenessContext)(HWND);
    typedef int  (WINAPI *GetAwarenessFromDpiAwarenessContext)(int);
    typedef BOOL (WINAPI *SystemParametersInfoForDpi)(UINT, UINT, PVOID, UINT, UINT);

    // Windows pointer functions (Windows 8 or later).
    EnableMouseInPointer enableMouseInPointer = nullptr;
    GetPointerType getPointerType = nullptr;
    GetPointerInfo getPointerInfo = nullptr;
    GetPointerDeviceRects getPointerDeviceRects = nullptr;
    GetPointerTouchInfo getPointerTouchInfo = nullptr;
    GetPointerFrameTouchInfo getPointerFrameTouchInfo = nullptr;
    GetPointerFrameTouchInfoHistory getPointerFrameTouchInfoHistory = nullptr;
    GetPointerPenInfo getPointerPenInfo = nullptr;
    GetPointerPenInfoHistory getPointerPenInfoHistory = nullptr;
    SkipPointerFrameMessages skipPointerFrameMessages = nullptr;

    // Windows Vista onwards
    SetProcessDPIAware setProcessDPIAware = nullptr;

    // Clipboard listeners are present on Windows Vista onwards
    // but missing in MinGW 4.9 stub libs. Can be removed in MinGW 5.
    AddClipboardFormatListener addClipboardFormatListener = nullptr;
    RemoveClipboardFormatListener removeClipboardFormatListener = nullptr;

    // Rotation API
    GetDisplayAutoRotationPreferences getDisplayAutoRotationPreferences = nullptr;
    SetDisplayAutoRotationPreferences setDisplayAutoRotationPreferences = nullptr;

    AdjustWindowRectExForDpi adjustWindowRectExForDpi = nullptr;
    EnableNonClientDpiScaling enableNonClientDpiScaling = nullptr;
    GetWindowDpiAwarenessContext getWindowDpiAwarenessContext = nullptr;
    GetAwarenessFromDpiAwarenessContext getAwarenessFromDpiAwarenessContext = nullptr;
    SystemParametersInfoForDpi systemParametersInfoForDpi = nullptr;
};

// Shell scaling library (Windows 8.1 onwards)
struct LWindowsShcoreDLL {
    void init();
    bool isValid() const;

    typedef HRESULT (WINAPI *GetProcessDpiAwareness)(HANDLE,int *);
    typedef HRESULT (WINAPI *SetProcessDpiAwareness)(int);
    typedef HRESULT (WINAPI *GetDpiForMonitor)(HMONITOR,int,UINT *,UINT *);

    GetProcessDpiAwareness getProcessDpiAwareness = nullptr;
    SetProcessDpiAwareness setProcessDpiAwareness = nullptr;
    GetDpiForMonitor getDpiForMonitor = nullptr;
};


class LWindowsCtx
{
public:
    LWindowsCtx();
    ~LWindowsCtx();

    enum SystemInfoFlags
    {
        SI_RTL_Extensions = 0x1,
        SI_SupportsTouch = 0x2,
        SI_SupportsPointer = 0x4,
    };


    static LWindowsUser32DLL user32dll;
    static LWindowsShcoreDLL shcoredll;

    std::string registerWindowClass(const std::shared_ptr<LWindow> w);
    std::string registerWindowClass(std::string cname, WNDPROC proc,
                                unsigned style = 0, HBRUSH brush = nullptr,
                                bool icon = false);

    void addWindow(HWND, std::shared_ptr<LWindowsWin> w);
    void removeWindow(HWND);

    static LWindowsCtx *instance();
    void unregisterWindowClasses();

    static std::string comErrorString(HRESULT hr);

    inline bool windowsProc(HWND hwnd, UINT message,
                            LWindows::WindowsEventType et,
                            WPARAM wParam, LPARAM lParam, LRESULT *result,
                            std::shared_ptr<LWindowsWin> platformWindow);

    std::shared_ptr<LWindowsWin> findClosestPlatformWindow(HWND) const;
    std::shared_ptr<LWindowsWin> findPlatformWindow(HWND) const;

    static bool filterNativeEvent(MSG *msg, LRESULT *result);
    static bool filterNativeEvent(std::shared_ptr<LWindow>  window, MSG *msg, LRESULT *result);

    inline std::shared_ptr<LWindowCreationCtx> windowCreationContext(){
        return m_creationContext;
    }

    std::shared_ptr<LWindowCreationCtx> setWindowCreationContext(const std::shared_ptr<LWindowCreationCtx> &ctx);

    inline LWindowsScreenManager& screenManager(){
        return m_screenManager;
    }

    static bool shouldHaveNonClientDpiScaling(const std::shared_ptr<LWindow> window);

    std::shared_ptr<LWindow> windowUnderMouse() const;
    void clearWindowUnderMouse();

    inline bool asyncExpose(){
        return m_asyncExpose;
    }

    static bool systemParametersInfo(uint32_t action, uint32_t param, void *out, uint32_t dpi = 0);

    static bool nonClientMetrics(NONCLIENTMETRICS *ncm, uint32_t dpi = 0);
    static bool nonClientMetricsForScreen(NONCLIENTMETRICS *ncm,
                                          const std::shared_ptr<LPlatformScreen> screen = nullptr);
    static DWORD readAdvancedExplorerSettings(const std::wstring& subKey, DWORD defaultValue);

    std::shared_ptr<LWindowsWin> findPlatformWindowAt(HWND parent, const
                                                      LPoint<int> &screenPoint,
                                                      unsigned cwexFlags) const;

    inline std::shared_ptr<LWindow> keyGrabber() const{
        return m_keyMapper.keyGrabber();
    }
    inline void setKeyGrabber(std::shared_ptr<LWindow> w){
        m_keyMapper.setKeyGrabber(w);
    }

    inline std::list<uint32_t> possibleKeys(const std::shared_ptr<LKeyEvent> e) const{
        return m_keyMapper.possibleKeys(e);
    }
protected:
    static LWindowsCtx *m_instance;

    uint8_t m_systemInfo = 0;
    std::set<std::string> m_registeredWindowClassNames;

    std::unordered_map<HWND, std::weak_ptr<LWindowsWin> >m_windows;
    HDC m_displayContext = nullptr;
    int m_defaultDPI = 96;

    LWindowsKeyMapper m_keyMapper;
    LWindowsMouseHandler m_mouseHandler;
    LWindowsPointerHandler m_pointerHandler;

    const HRESULT m_oleInitializeResult;
    std::weak_ptr<LWindow> m_lastActiveWindow;
    bool m_asyncExpose = false;
    HPOWERNOTIFY m_powerNotification = nullptr;
    HWND m_powerDummyWindow = nullptr;
    std::shared_ptr<LWindowCreationCtx> m_creationContext;
    LWindowsScreenManager m_screenManager;

    void handleFocusEvent(LWindows::WindowsEventType et, std::shared_ptr<LWindowsWin> w);
    void handleExitSizeMove(std::shared_ptr<LWindow> window);
};

extern "C" LRESULT L_WIN_CALLBACK lWindowsWndProc(HWND, UINT, WPARAM, LPARAM);


LK_NAMESPACE_END

#endif // LWINDOWSCTX_H
