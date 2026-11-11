#include "leventdispatcherwin.h"
#include "lkcore/ldebug.h"
#include "lkcore/lcoreapplication.h"
#include <string>

LK_NAMESPACE_BEGIN
//enum {
//    SendPostedEventsTimerId = ~1u
//};

LRESULT L_WIN_CALLBACK LEventDispatcherWin::loukeWinInternalProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    if (message == WM_NCCREATE){
        return true;
    }


    std::shared_ptr<LEventDispatcher> dispatcher = LEventDispatcher::instance();
    if (dispatcher == nullptr) {
        return 0;
    }

    MSG msg;
    msg.hwnd = hwnd;
    msg.message = message;
    msg.wParam = wp;
    msg.lParam = lp;

    lintptr result;
    if (dispatcher->filterNativeEvent("windows_dispatcher_MSG", &msg, &result)){
        return result;
    }

    LEventDispatcherWin * winDispatcher =
        reinterpret_cast<LEventDispatcherWin *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if(winDispatcher == nullptr){
        return 0;
    }
    switch (message) {
    case WM_LK_SENDPOSTEDEVENTS:{
        winDispatcher->sendPostedEvents();
        return 0;
    }
    default:{
        return DefWindowProc(hwnd, message, wp, lp);
    }
    }
}


class LWinMsgContext
{
    using pointer_sized_uint = _W64 unsigned int;
public:
    LWinMsgContext();
    ~LWinMsgContext();
    ATOM atom;
    std::string className;
    HINSTANCE  hInstance;
    void release();
};

HINSTANCE windowsModuleHandle()
{
    MEMORY_BASIC_INFORMATION mbi;
    static int dummy;
    VirtualQuery(&dummy, &mbi, sizeof(mbi));
    return reinterpret_cast<HINSTANCE>(mbi.AllocationBase);
}
static void* currentModuleHandle = nullptr;
void* getCurrentModuleInstanceHandle() noexcept
{
    if (currentModuleHandle == nullptr)
    {
        auto status = GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCTSTR)&currentModuleHandle,
            (HMODULE*)&currentModuleHandle);

        if (status == 0 || currentModuleHandle == nullptr) {
            currentModuleHandle = GetModuleHandleA(nullptr);
        }
    }
    return currentModuleHandle;
}

LWinMsgContext::LWinMsgContext()
    : atom(0)
{
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = LEventDispatcherWin::loukeWinInternalProc;
    wc.cbWndExtra = 4;

    hInstance = (HINSTANCE)getCurrentModuleInstanceHandle();
    wc.hInstance = hInstance;

    className = "LuokeInternal";
    className.append(std::to_string((long long)LEventDispatcherWin::loukeWinInternalProc));
    wc.lpszClassName = className.data();
    atom = RegisterClassExA(&wc);

    if (!atom) {
        lWarn() << "RegisterClassExA " << className << " failed!";
    }
}

void LWinMsgContext::release(){
    if(!className.empty()){
        UnregisterClassA(className.data(), hInstance);
        className.clear();
        hInstance = nullptr;
    }
}

LWinMsgContext::~LWinMsgContext()
{
    release();
}

LWinMsgContext *lWinMsgContext(){
    static LWinMsgContext ctx;
    return &ctx;
}

static HWND lWinCreateInternal(const LEventDispatcherWin *eventDispatcher)
{
    LWinMsgContext *ctx = lWinMsgContext();
    if (!ctx->atom){
        return 0;
    }

    HWND wnd = CreateWindowA(ctx->className.data(),    // classname
                            ctx->className.data(),    // window name
                            0,                 // style
                            0, 0, 0, 0,        // geometry
                            nullptr,            // parent
                            nullptr,                 // menu handle
                            ctx->hInstance,     // application
                            0);                // windows creation data.


    if (!wnd) {
        lFatal() << "CreateWindowA "<< ctx->className << " for LWinEventDispatcher failed";
        return 0;
    }
    SetWindowLongPtr(wnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(eventDispatcher));
    return wnd;
}



LEventDispatcherWin::LEventDispatcherWin()
{

}

LEventDispatcherWin::~LEventDispatcherWin() {

    if (m_internalHwnd != nullptr) {
        DestroyWindow(m_internalHwnd);
        m_internalHwnd = nullptr;
    }
}

bool LEventDispatcherWin::processEvents(LFlags<LProcessEvents> flags)
{
    sendPostedEvents(flags);

    MSG msg;
    while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
    {
        if (!filterNativeEvent(nativeEventType(), &msg, 0)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        if (msg.message == WM_QUIT){
            if (LCoreApplication::instance() != nullptr){
                LCoreApplication::instance()->quit();
            }
            return false;
        }
    }
    return true;
}

void LEventDispatcherWin::wakeUp()
{
    bool f = false;
    if (m_internalHwnd && m_wakeUps.compare_exchange_strong(f, true)) {
        // post a WM_LK_SENDPOSTEDEVENTS to this thread if there isn't one already pending
        if (!PostMessage(m_internalHwnd, WM_LK_SENDPOSTEDEVENTS, 0, 0)){
            lWarn()<< "LEventDispatcherWin::wakeUp: Failed to post a message";
        }
    }
    LEventDispatcher::wakeUp();
}

void LEventDispatcherWin::sendPostedEvents(LFlags<LProcessEvents> flags)
{
    m_wakeUps.store(false, std::memory_order::relaxed);
    LEventDispatcher::processEvents(flags);
}

void LEventDispatcherWin::init()
{
    if (m_internalHwnd){
        return;
    }
    m_internalHwnd = lWinCreateInternal(this);
}

LK_NAMESPACE_END
