#ifndef LAPPLICATION_H
#define LAPPLICATION_H

#include "lkgui/lguiapplication.h"
#include "lkwidgets/draw/lstyle.h"
#include "lkwidgets/core/ldesktopwidget.h"
#include "lkcore/time/ltimer.h"

LK_NAMESPACE_BEGIN


#define lWApp LApplication::wInst()
#define lWShare LApplication::wShare()

class LKWIDGETSDEF LApplication : public LGuiApplication
{
public:
    LApplication(int &aargc,  char **aargv);
    virtual ~LApplication();

    virtual bool notify(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event) override;
    virtual void init() override;
    virtual void notifyActiveWindowChange(std::shared_ptr<LWindow> previous) override;
    virtual std::shared_ptr<LWindow> windowBlocked(std::shared_ptr<LWindow> window) const override;

    void focusBlink();
    void sendSyntheticEnterLeave(std::shared_ptr<LWidget> widget);

    static LApplication *wInst() { return (LApplication *)m_self; }
    static std::shared_ptr<LApplication> wShare() {
        return m_self==nullptr ? nullptr : m_self->getPtrT<LApplication>();
    }

    static void closeAllPopups();
    static std::shared_ptr<LWidget> activePopupWidget();
    static std::shared_ptr<LWidget> activeModalWidget();

    static inline std::shared_ptr<LWidget> focusWidget(){
        return m_focusWidget.lock();
    }

    static void setFocusWidget(std::shared_ptr<LWidget> focus, LFocusReason reason);
    static std::shared_ptr<LDesktopWidget> desktop();

    void closePopup(std::shared_ptr<LWidget> popup);
    void openPopup(std::shared_ptr<LWidget> popup);

    static std::shared_ptr<LWidget> activeWindow();
    static void setActiveWindow(std::shared_ptr<LWidget> act);

    static std::shared_ptr<LWidget> topLevelWidgetForWindow(std::shared_ptr<LWindow> wnd);
    static std::list<std::shared_ptr<LWidget> > topLevelWidgets();
    static std::shared_ptr<LWidget> topLevelWidgdetAt(const LPoint<int> &p);

    static std::shared_ptr<LStyle> style();

    static bool inPopupMode()
    {
        return !m_popupWidgets.empty();
    }

    static std::shared_ptr<LWidget> widgetAt(const LPoint<int> &p);
    static std::shared_ptr<LTimer> blinkTimer();

    LSignal<std::shared_ptr<LWidget>,/*old*/
            std::shared_ptr<LWidget> /*now*/> sigFocusChanged;


protected:
    bool notifyWidgetHelper(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> e);

    static void giveFocusAccordingToFocusPolicy(std::shared_ptr<LWidget> w,
                                                std::shared_ptr<LEvent> event,
                                                LPoint<int> localPos);

    static std::shared_ptr<LWidget> focusNextPrevChildHelper(std::shared_ptr<LWidget> toplevel,
                                                              bool next,
                                                              bool *wrappingOccurred = nullptr);


    static void insertWidget(std::shared_ptr<LWidget> w);
    static void removeWidget(std::shared_ptr<LWidget> w);


    static void dispatchEnterLeave(std::shared_ptr<LWidget> enter,
                                   std::shared_ptr<LWidget> leave,
                                   const LPoint<double> &globalPosF);

    static bool sendMouseEvent(std::shared_ptr<LWidget> receiver, std::shared_ptr<LMouseEvent> event,
                               std::shared_ptr<LWidget> alienWidget, std::shared_ptr<LWidget> native,
                               std::weak_ptr<LWidget>& buttonDown, std::weak_ptr<LWidget>& lastMouseReceiver,
                               bool spontaneous = true, bool onlyDispatchEnterLeave = false);

    static bool isBlockedByModal(std::shared_ptr<LWidget> widget);
    static bool tryModal(std::shared_ptr<LWidget> widget, LEvent::Type type);
    static std::shared_ptr<LWidget> tryModalHelper(std::shared_ptr<LWidget> w);

    static std::shared_ptr<LWidget> pickMouseReceiver(std::shared_ptr<LWidget> candidate,
                                                      const LPoint<int> &windowPos,
                                                      LPoint<int> *pos,
                                                      LEvent::Type type, LFlags<LMouseButton> buttons,
                                                      std::shared_ptr<LWidget> buttonDown,
                                                      std::shared_ptr<LWidget> alienWidget);


    static void ungrabKeyboardForPopup(std::shared_ptr<LWidget> popup);
    static void ungrabMouseForPopup(std::shared_ptr<LWidget> popup);
    static void grabForPopup(std::shared_ptr<LWidget>  popup);

    static bool shouldSetFocus(std::shared_ptr<LWidget> w, LFocusPolicy policy);


    static std::unordered_map<WId, std::weak_ptr<LWidget> > m_mapper;
    static std::list<std::weak_ptr<LWidget> > m_allWidgets;
    static std::list<std::shared_ptr<LWidget> > m_popupWidgets;


    std::shared_ptr<LWidget> m_toolTipWidget;
    LPoint<int> m_toolTipPos, m_toolTipGlobalPos, m_hoverGlobalPos;

    static std::shared_ptr<LTimer> m_blinkTimer;

    static std::weak_ptr<LWidget> m_focusWidget;
    static std::weak_ptr<LWidget> m_hiddenFocusWidget;
    static std::shared_ptr<LDesktopWidget> m_desktop;
    static std::shared_ptr<LWidget> m_activeWindow;
    static std::shared_ptr<LStyle> m_style;
    static std::weak_ptr<LWidget> m_leaveAfterRelease;

    static bool m_inTabKeyEvent;
    static bool m_popupGrabOk;
    static bool m_autoSipEnabled;

    friend class LWidget;
    friend class LWidgetWindow;
    friend bool correctWidgetContext(LShortcutCtxType context,
                                     std::shared_ptr<LWidget> w,
                                     std::shared_ptr<LWidget> activeWindow);
};


LK_NAMESPACE_END
#endif // LAPPLICATION_H
