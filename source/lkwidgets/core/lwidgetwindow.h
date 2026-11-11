#ifndef LWIDGETWINDOW_H
#define LWIDGETWINDOW_H

#include "lkwidgets/core/lwidget.h"

LK_NAMESPACE_BEGIN

class LWidgetWindow : public LWindow
{
public:
    LWidgetWindow(std::shared_ptr<LWidget> w);

    std::shared_ptr<LWidget>  widget();
    virtual bool isWidgetWindow() override;
    virtual std::shared_ptr<LObject> focusObject() override;
protected:
    virtual bool handleEvent(std::shared_ptr<LEvent> ev) override;
    void handleCloseEvent(std::shared_ptr<LEvent> ev);
    void handleEnterLeaveEvent(std::shared_ptr<LEvent> ev);
    void handleFocusInEvent(std::shared_ptr<LFocusEvent> ev);
    void handleKeyEvent(std::shared_ptr<LKeyEvent> ev);
    void handleMouseEvent(std::shared_ptr<LMouseEvent> ev);
    void handleNonClientAreaMouseEvent(std::shared_ptr<LMouseEvent> ev);

    void handleMoveEvent(std::shared_ptr<LMoveEvent> ev);
    void handleResizeEvent(std::shared_ptr<LResizeEvent> ev);
    void handleWheelEvent(std::shared_ptr<LWheelEvent> ev);
    void handleExposeEvent(std::shared_ptr<LExposeEvent> ev);
    void handleWindowStateChangedEvent(std::shared_ptr<LWindowStateChangeEvent> ev);

    bool updateSize();
    void updateMargins();

    enum FocusWidgets {
        FirstFocusWidget,
        LastFocusWidget
    };
    std::shared_ptr<LWidget> getFocusWidget(FocusWidgets fw);

    std::weak_ptr<LWidget> m_widget;

    static std::weak_ptr<LWidget> m_buttonDown; // widget got last button-down
    static std::weak_ptr<LWidget> m_popupDown; // popup that contains the pressed widget
    static std::weak_ptr<LWidget> m_lastMouseReceiver;
    static int m_openPopupCount;
    static bool m_popupDownClosed; //popup_down has been closed
    static bool m_replayPopupMouseEvent;

    friend class LWidget;
    friend class LApplication;
};


LK_NAMESPACE_END
#endif // LWIDGETWINDOW_H
