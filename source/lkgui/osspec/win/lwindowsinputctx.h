#ifndef LWINDOWSINPUTCTX_H
#define LWINDOWSINPUTCTX_H

#include "lkgui/lplatforminputctx.h"
#include "lwindows.h"
#include <string>
#include "lkcore/lobject.h"

LK_NAMESPACE_BEGIN

class LWindowsWin;
class LWindowsInputCtx : public LPlatformInputCtx
{
    struct CompositionCtx
    {
        HWND hwnd = nullptr;
        std::wstring composition;
        int position = 0;
        bool isComposing = false;
        std::weak_ptr<LObject> focusObject;
    };
public:
    LWindowsInputCtx();
    static void setWindowsImeEnabled(std::shared_ptr<LWindowsWin> platformWindow, bool enabled);

    bool startComposition(HWND hwnd);
    bool composition(HWND hwnd, LPARAM lParam);
    bool endComposition(HWND hwnd);
    inline bool isComposing() const { return m_compositionContext.isComposing; }

    virtual void update(LFlags<LInputMethodQuery>) override;
    virtual void cursorRectChanged() override;
    int reconvertString(RECONVERTSTRING *reconv);
    bool handleIMERequest(WPARAM wparam, LPARAM lparam, LRESULT *result);
    void handleInputLanguageChanged(WPARAM wparam, LPARAM lparam);

    virtual void setFocusObject(std::shared_ptr<LObject> object) override;
    virtual void reset() override;
    virtual void showInputPanel() override;
    virtual void hideInputPanel() override;

    virtual std::string localeLanguage() const override;
    virtual void invokeAction(LInputMethod::Action, int cursorPosition) override;

    static std::wstring lcidToLocaleName(LCID id);
    static LCID localeNameToLcid(const std::wstring& name);
private:
    void initContext(HWND hwnd, std::shared_ptr<LObject> focusObject);
    void doneContext();
    void startContextComposition();
    void endContextComposition();
    void updateEnabled();


    const DWORD m_WM_MSIME_MOUSE;
    bool m_caretCreated = false;
    HBITMAP m_transparentBitmap;
    CompositionCtx m_compositionContext;
    bool m_endCompositionRecursionGuard = false;
    LCID m_languageId;

};

LK_NAMESPACE_END

#endif // LWINDOWSINPUTCTX_H
