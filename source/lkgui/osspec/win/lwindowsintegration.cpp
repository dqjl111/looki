#include "lkgui/osspec/win/lwindowsguieventdispatcher.h"
#include "lkgui/osspec/win/lwindowsintegration.h"
#include "lkgui/osspec/win/lwindowsinputctx.h"
#include "lkgui/osspec/win/lwindowswin.h"
#include "lkgui/osspec/win/lwindowsfontdatabase.h"
#include "lkgui/osspec/win/lwindowsclipboard.h"
#include "lkgui/osspec/win/lwindowsfiledialoghelper.h"
#include "lkgui/lguiapplication.h"

LK_NAMESPACE_BEGIN

LWindowsIntegration* LWindowsIntegration::m_instance = nullptr;

LWindowsIntegration::LWindowsIntegration()
{
    m_instance = this;
    m_inputCtx = std::make_shared<LWindowsInputCtx>();
    m_context.screenManager().handleScreenChanges();
}

void LWindowsIntegration::initialize()
{
    LThemeHints* themeHints = LGuiApplication::themeHints();
    const uint32_t timeMS = GetCaretBlinkTime();
    if(timeMS == 0 || timeMS == INFINITE ){
        themeHints->setCursorFlashTime(-1);
    }else{
        themeHints->setCursorFlashTime(timeMS);
    }
}

std::shared_ptr<LEventDispatcher> LWindowsIntegration::createEventDispatcher() const
{
    return makeInit<LWindowsGuiEventDispatcher>();
}

std::shared_ptr<LPlatformWindow> LWindowsIntegration::createPlatformWindow(std::shared_ptr<LWindow> window) const
{
    if (window->type() == LWindowType::Desktop) {
        return nullptr;
    }

    LWindowsWindowData requested;
    requested.flags = window->flags();
    requested.geometry = window->isTopLevel()
                             ? LHighDpi::toNativePixels(window->geometry(), window.get())
                             : LHighDpi::toNativeLocalPosition(window->geometry(), window.get());

    LWindowsWindowData obtained = LWindowsWindowData::create(window, requested, window->title());

    if (L_UNLIKELY(!obtained.hwnd)){
        return nullptr;
    }

    std::shared_ptr<LWindowsWin> result = createPlatformWindowHelper(window, obtained);

    if (window->isTopLevel() && !LWindowsCtx::shouldHaveNonClientDpiScaling(window)){
        result->setFlag(LWindowsWin::DisableNonClientScaling);
    }
    return result;
}

std::shared_ptr<LPlatformFontDatabase> LWindowsIntegration::fontDatabase() const
{
    if(m_fontDatabase == nullptr){
        const_cast<LWindowsIntegration*>(this)->m_fontDatabase = std::make_shared<LWindowsFontDatabase>();
    }
    return m_fontDatabase;
}

std::shared_ptr<LPlatformClipboard> LWindowsIntegration::clipboard() const
{
    if(m_clipboard == nullptr){
        const_cast<LWindowsIntegration*>(this)->m_clipboard = std::make_shared<LWindowsClipboard>();
    }
    return  m_clipboard;
}

std::shared_ptr<LPlatformDialogHelper> LWindowsIntegration::createPlatformDialogHelper(LPlatformDialogHelper::Type type) const
{
    std::shared_ptr<LPlatformDialogHelper> ret;
    switch (type) {
    case LPlatformDialogHelper::File:{
        ret = std::make_shared<LWindowsFileDialogHelper>();
        break;
    }
    default:
        break;
    }
    return ret;
}

LSize<int> LWindowsIntegration::systemCursorSize() const
{
    return LSize<int>(GetSystemMetrics(SM_CXCURSOR), GetSystemMetrics(SM_CYCURSOR));
}

std::list<uint32_t> LWindowsIntegration::possibleKeys(const std::shared_ptr<LKeyEvent> e) const
{
    return m_context.possibleKeys(e);
}

void LWindowsIntegration::applicationStateChanged(LApplicationState state)
{
    LWindowsKeyMapper::clearKeyRecorderOnApplicationInActive(state);
}

std::shared_ptr<LWindowsWin> LWindowsIntegration::createPlatformWindowHelper(std::shared_ptr<LWindow> window, const LWindowsWindowData &data) const
{
    return std::make_shared<LWindowsWin>(window, data);
}

LK_NAMESPACE_END
