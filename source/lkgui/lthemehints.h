#ifndef LTHEMEHINTS_H
#define LTHEMEHINTS_H
#include "lguienums.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LThemeHints
{
public:
    LThemeHints();

    void setMouseDoubleClickInterval(int mouseDoubleClickInterval);
    int mouseDoubleClickInterval() const;
    int mouseDoubleClickDistance() const;
    int touchDoubleTapDistance() const;
    void setMousePressAndHoldInterval(int mousePressAndHoldInterval);
    int mousePressAndHoldInterval() const;
    void setStartDragDistance(int startDragDistance);
    int startDragDistance() const;
    void setStartDragTime(int startDragTime);
    int startDragTime() const;
    int startDragVelocity() const;
    void setKeyboardInputInterval(int keyboardInputInterval);
    int keyboardInputInterval() const;
    int keyboardAutoRepeatRate() const;
    void setCursorFlashTime(int cursorFlashTime);
    int cursorFlashTime() const;

    bool showShortcutsInContextMenus() const;
    void setShowShortcutsInContextMenus(bool showShortcutsInContextMenus);

    LTabFocusCtrls tabFocusCtrls() const;
    void setTabFocusCtrls(LTabFocusCtrls v);

    bool useHoverEffects() const;
    void setUseHoverEffects(bool useHoverEffects);
    int wheelScrollLines() const;
    void setWheelScrollLines(int scrollLines);
    void setMouseQuickSelectionThreshold(int threshold);
    int mouseQuickSelectionThreshold() const;
protected:
    int m_mouseDoubleClickInterval = -1;
    int m_mousePressAndHoldInterval = -1;
    int m_startDragDistance = -1;
    int m_startDragTime = -1;
    int m_keyboardInputInterval = -1;
    int m_cursorFlashTime = -1;
    int m_tabFocusCtrls = -1;
    int m_uiEffects = -1;
    int m_showShortcutsInContextMenus = -1;
    int m_wheelScrollLines = -1;
    int m_mouseQuickSelectionThreshold = -1;
    int m_mouseDoubleClickDistance = -1;
    int m_touchDoubleTapDistance = -1;
};

LK_NAMESPACE_END

#endif // LTHEMEHINTS_H
