#ifndef LWINDOWSINTEGRATION_H
#define LWINDOWSINTEGRATION_H
#include "lkgui/lplatformintegration.h"
#include "lwindowsservices.h"
#include "lwindowsctx.h"

LK_NAMESPACE_BEGIN
class LWindowsIntegration : public LPlatformIntegration
{
public:
    LWindowsIntegration();

    virtual void initialize() override;

    static LWindowsIntegration *instance() { return m_instance; }
    virtual std::shared_ptr<LEventDispatcher> createEventDispatcher() const override;
    virtual std::shared_ptr<LPlatformWindow> createPlatformWindow(std::shared_ptr<LWindow> window) const override;
    virtual std::shared_ptr<LPlatformFontDatabase> fontDatabase() const override;
    virtual std::shared_ptr<LPlatformClipboard> clipboard() const override;
    virtual std::shared_ptr<LPlatformDialogHelper> createPlatformDialogHelper(LPlatformDialogHelper::Type type) const;

    virtual LSize<int> systemCursorSize() const override;
    virtual std::list<uint32_t> possibleKeys(const std::shared_ptr<LKeyEvent> e) const override;
    virtual void applicationStateChanged(LApplicationState state) override;
protected:
    static LWindowsIntegration *m_instance;
    LWindowsServices m_services;
    LWindowsCtx m_context;
    std::shared_ptr<LPlatformFontDatabase> m_fontDatabase = nullptr;
    std::shared_ptr<LPlatformClipboard> m_clipboard = nullptr;

    virtual std::shared_ptr<LWindowsWin> createPlatformWindowHelper(std::shared_ptr<LWindow> window, const LWindowsWindowData &data) const;
};


LK_NAMESPACE_END
#endif // LWINDOWSINTEGRATION_H
