#ifndef LGUIAPPLICATION_H
#define LGUIAPPLICATION_H

#include "lkcore/lcoreapplication.h"
#include "lkgui/lplatformintegration.h"
#include "lkgui/lpoint.h"
#include "lkgui/lguievent.h"
#include "lkgui/lshortcutman.h"
#include "lkgui/lthemehints.h"
#include "lkgui/lpalette.h"
#include "lkgui/lwindowsysteminterface.h"
#include "lkgui/lclipboard.h"

LK_NAMESPACE_BEGIN

//#if defined(lApp)
//#undef lApp
//#endif
//#define lApp LGuiApplication::guiInst()

#define lGuiApp LGuiApplication::guiInst()
#define lGuiShare LGuiApplication::guiShare()


class LWindow;
class LScreen;
class LInputMethod;
class LWindowSystemInterface;
class LColorTrcLut;
class LKGUIDEF LGuiApplication : public LCoreApplication
{
public:
    LGuiApplication(int &aargc, char **aargv);

    static std::shared_ptr<LScreen> primaryScreen();
    static std::list<std::shared_ptr<LScreen> > screens();
    static std::shared_ptr<LScreen> screenAt(int index, int defaultIndex = 0);
    static std::shared_ptr<LScreen> screenAt(const LPoint<int> &point);

    static std::vector<std::shared_ptr<LWindow> > allWindows();
    static std::vector<std::shared_ptr<LWindow> > topLevelWindows();
    static std::shared_ptr<LWindow> topLevelAt(const LPoint<int> &pos);
    static std::shared_ptr<LTexture> windowIcon();

    static LGuiApplication *guiInst() { return (LGuiApplication *)m_self; }
    static std::shared_ptr<LGuiApplication> guiShare() {
        return m_self==nullptr ? nullptr : m_self->getPtrT<LGuiApplication>();
    }

    static const LCursor& overrideCursor();
    static void setOverrideCursor(const LCursor &);
    static void restoreOverrideCursor(const LCursor &);
    static void applyOverrideCursor(const LCursor &);

    static std::shared_ptr<LWindow> modalWindow();
    static void showModalWindow(std::shared_ptr<LWindow> window);
    static void hideModalWindow(std::shared_ptr<LWindow> window);
    static void updateBlockedStatus(std::shared_ptr<LWindow> window);
    virtual std::shared_ptr<LWindow> windowBlocked(std::shared_ptr<LWindow> window) const;
    virtual bool notify(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event) override;
    virtual void init() override;
    virtual void notifyActiveWindowChange(std::shared_ptr<LWindow> previous);
    void updateFocusObject(std::shared_ptr<LObject> object);

    std::shared_ptr<LColorTrcLut> colorProfileForA8Text();
    std::shared_ptr<LColorTrcLut> colorProfileForA32Text();

    bool modalState(){
        return !m_modelWindows.empty();
    }

    static LPlatformIntegration *platformIntegration();

    static std::shared_ptr<LInputMethod> inputMethod();
    static LApplicationState applicationState();

    static inline std::shared_ptr<LWindow> focusWindow(){
        return m_focusWindow.lock();
    }
    static inline std::shared_ptr<LWindow> currentMouseWindow(){
        return m_currentMouseWindow.lock();
    }
    static inline std::shared_ptr<LWindow> currentMousePressWindow(){
        return m_currentMousePressWindow.lock();
    }


    static std::shared_ptr<LObject> focusObject();

    static LMouseEventSource mouseEventSource(const LMouseEvent *event);
    static void setMouseEventSource(LMouseEvent *event, LMouseEventSource source);

    static LFlags<LMouseEventFlag> mouseEventFlags(const LMouseEvent *event);
    static void setMouseEventFlags(LMouseEvent *event, LFlags<LMouseEventFlag> flags);
    static std::shared_ptr<LPalette> palette();

    static bool processNativeEvent(std::shared_ptr<LWindow> window,
                                   const std::string &eventType,
                                   void *message,
                                   lintptr *result);

    static void processWindowSystemEvent(std::shared_ptr<LWinSysEvent> e);
    static void processMouseEvent(std::shared_ptr<wse::LMouseEvent> e);
    static void processWheelEvent(std::shared_ptr<wse::LWheelEvent> e);
    static void processKeyEvent(std::shared_ptr<wse::LKeyEvent> e);
    static void processGeometryChangeEvent(std::shared_ptr<wse::LGeometryChangeEvent> e);
    static void processEnterEvent(std::shared_ptr<wse::LEnterEvent> e);
    static void processLeaveEvent(std::shared_ptr<wse::LWinEvent> e);

    static void processActivatedEvent(std::shared_ptr<wse::LActivatedWindowEvent> e);
    static void processWindowStateChangedEvent(std::shared_ptr<wse::LWindowStateChangedEvent> e);
    static void processWindowScreenChangedEvent(std::shared_ptr<wse::LWindowScreenChangedEvent> e);
    static void processSafeAreaMarginsChangedEvent(std::shared_ptr<wse::LWinEvent> e);

    static void processApplicationTermination(std::shared_ptr<LWinSysEvent> e);
    static void processCloseEvent(std::shared_ptr<wse::LWinEvent> e);
    static void processScreenOrientationChange(std::shared_ptr<wse::LScreenOrientationEvent> e);
    static void processScreenGeometryChange(std::shared_ptr<wse::LScreenGeometryEvent> e);
    static void processScreenLogicalDotsPerInchChange(std::shared_ptr<wse::LScreenLogicalDotsPerInchEvent> e);

    static void processScreenRefreshRateChange(std::shared_ptr<wse::LScreenRefreshRateEvent> e);
    static void processThemeChanged(std::shared_ptr<wse::LWinEvent> e);
    static void processExposeEvent(std::shared_ptr<wse::LExposeEvent> e);
    static void processTabletEvent(std::shared_ptr<wse::LTabletEvent> e);
    static void processTabletEnterProximityEvent(std::shared_ptr<wse::LTabletProximityEvent> e);
    static void processTabletLeaveProximityEvent(std::shared_ptr<wse::LTabletProximityEvent> e);

    static void processPlatformPanelEvent(std::shared_ptr<wse::LWinEvent> e);
    static void processContextMenuEvent(std::shared_ptr<wse::LContextMenuEvent> e);

    static void updateFilteredScreenOrientation(std::shared_ptr<LScreen> screen);
    static void resetCachedDevicePixelRatio();

    static const std::shared_ptr<LFont> font();
    static void setFont(const std::shared_ptr<LFont> f);

    static void setQuitOnLastWindowClosed(bool quit);
    static bool quitOnLastWindowClosed();
    static void setLayoutDirection(LLayoutDirection direction);
    static LLayoutDirection layoutDirection();

    static double devicePixelRatio();
    static const LFlags<LKeyModifier>& keyboardModifiers();
    static const LFlags<LMouseButton>& mouseButtons();
    static std::list<uint32_t> possibleKeys(std::shared_ptr<LKeyEvent> e);

    static inline LFlags<LAlignment> visualAlignment(LLayoutDirection direction, LFlags<LAlignment> alignment)
    {
        if (!(alignment & LAlignment::HorizontalMask)){
            alignment |= LAlignment::Left;
        }
        if (!(alignment & LAlignment::Absolute) && (alignment & (LAlignment::Left | LAlignment::Right))) {
            if (direction == LLayoutDirection::RightToLeft){
                alignment ^= (LAlignment::Left | LAlignment::Right);
            }
            alignment |= LAlignment::Absolute;
        }
        return alignment;
    }

    inline static LThemeHints* themeHints(){
        return &m_themeHints;
    }

    inline static bool tabAllWidgets(){
        return m_themeHints.tabFocusCtrls() == LTabFocusCtrls::All;
    }

    static std::shared_ptr<LClipboard> clipboard();

    static uint64_t m_mousePressTime;
    static LMouseButton m_mousePressButton;
    static int m_mousePressX;
    static int m_mousePressY;
    static LPoint<double> m_lastCursorPosition;
    static bool m_antialiasingDisabled;

    LShortcutMan m_shortcutMan;

    LSignal<std::shared_ptr<LWindow> > sigFocusWindowChanged;
protected:
    static bool sendWindowEventToPlatformWindow(std::shared_ptr<LWindow> window, std::shared_ptr<LEvent> event);
    static void setApplicationState(LApplicationState state, bool forcePropagate = false);

    std::shared_ptr<LEventDispatcher> createEventDispatcher() override;
    void eventDispatcherReady() override;
    virtual bool popupActive() { return false; }


    static void reportScreenOrientationChange(std::shared_ptr<LScreen> screen);

    static void captureGlobalModifierState(std::shared_ptr<LEvent> e);

    static void insertWin(std::shared_ptr<LWindow> w);
    static void removeWin(std::shared_ptr<LWindow> w);
//    static void removeScreen(std::shared_ptr<LScreen> w);


    static std::shared_ptr<LClipboard> m_clipboard;
    static LFlags<LKeyModifier> m_modifierButtons;
    static LFlags<LMouseButton> m_mouseButtons;

    static std::list<std::shared_ptr<LScreen> > m_screens;
    static std::list<std::weak_ptr<LWindow> > m_windows;
    static double m_maxDevicePixelRatio;
    static bool m_highDpiScalingUpdated;
    static std::shared_ptr<LTexture> m_appIcon;
    static LApplicationState m_applicationState;
    static LThemeHints m_themeHints;
    static std::shared_ptr<LPalette> m_palette;
    static LCursor m_cursor;
    static LCursor m_lastCursor;

    static std::weak_ptr<LWindow> m_focusWindow;
    static std::weak_ptr<LWindow> m_currentMouseWindow;
    static std::weak_ptr<LWindow> m_currentMousePressWindow;
    static std::shared_ptr<LFont> m_font;

    static LLayoutDirection m_layoutDirection;


    std::list<std::weak_ptr<LWindow> > m_modelWindows;
    std::shared_ptr<LInputMethod> m_inputMethod = nullptr;
    std::shared_ptr<LColorTrcLut> m_a8ColorProfile = nullptr;
    std::shared_ptr<LColorTrcLut> m_a32ColorProfile = nullptr;


    friend class LWindowSystemInterface;
    friend class LPlatformIntegration;
    friend class LHighDpiScaling;
    friend class LWindow;
};

LK_NAMESPACE_END
#endif // LGUIAPPLICATION_H
