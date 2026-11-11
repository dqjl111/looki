#include "lthemehints.h"
#include "lplatformtheme.h"
#include "lplatformintegration.h"
#include "lguiapplication.h"

LK_NAMESPACE_BEGIN
static inline LVariant themeableHint(LPlatformTheme::Hint th)
{
    return LGuiApplication::platformIntegration()->styleHint(th);
}



LThemeHints::LThemeHints()
{

}

void LThemeHints::setMouseDoubleClickInterval(int mouseDoubleClickInterval)
{
    m_mouseDoubleClickInterval = mouseDoubleClickInterval;
}

int LThemeHints::mouseDoubleClickInterval() const
{
    return m_mouseDoubleClickInterval >= 0 ?
           m_mouseDoubleClickInterval :
           themeableHint(LPlatformTheme::Hint::MouseDoubleClickInterval).getInt32();
}

int LThemeHints::mouseDoubleClickDistance() const
{
    return m_mouseDoubleClickDistance >= 0 ?
               m_mouseDoubleClickDistance :
               themeableHint(LPlatformTheme::Hint::MouseDoubleClickDistance).getInt32();
}

int LThemeHints::touchDoubleTapDistance() const
{
    return m_touchDoubleTapDistance >= 0 ?
               m_touchDoubleTapDistance :
               themeableHint(LPlatformTheme::Hint::TouchDoubleTapDistance).getInt32();
}

void LThemeHints::setMousePressAndHoldInterval(int mousePressAndHoldInterval)
{
    m_mousePressAndHoldInterval = mousePressAndHoldInterval;
}

int LThemeHints::mousePressAndHoldInterval() const
{
    return m_mousePressAndHoldInterval >= 0 ?
               m_mousePressAndHoldInterval :
               themeableHint(LPlatformTheme::Hint::MousePressAndHoldInterval).getInt32();
}

void LThemeHints::setStartDragDistance(int startDragDistance)
{
    m_startDragDistance = startDragDistance;
}

int LThemeHints::startDragDistance() const
{
    return m_startDragDistance >= 0 ?
               m_startDragDistance :
               themeableHint(LPlatformTheme::Hint::StartDragDistance).getInt32();
}

void LThemeHints::setStartDragTime(int startDragTime)
{
    m_startDragTime = startDragTime;
}

int LThemeHints::startDragTime() const
{
    return m_startDragTime >= 0 ?
               m_startDragTime :
               themeableHint(LPlatformTheme::Hint::StartDragTime).getInt32();
}

int LThemeHints::startDragVelocity() const
{
    return themeableHint(LPlatformTheme::Hint::StartDragVelocity).getInt32();
}

void LThemeHints::setKeyboardInputInterval(int keyboardInputInterval)
{
    m_keyboardInputInterval = keyboardInputInterval;
}

int LThemeHints::keyboardInputInterval() const
{
    return m_keyboardInputInterval >= 0 ?
               m_keyboardInputInterval :
               themeableHint(LPlatformTheme::Hint::KeyboardInputInterval).getInt32();

}

int LThemeHints::keyboardAutoRepeatRate() const
{
    return themeableHint(LPlatformTheme::Hint::KeyboardAutoRepeatRate).getInt32();
}

void LThemeHints::setCursorFlashTime(int cursorFlashTime)
{
    m_cursorFlashTime = cursorFlashTime;
}

int LThemeHints::cursorFlashTime() const
{
    return m_cursorFlashTime >= 0 ?
               m_cursorFlashTime :
               themeableHint(LPlatformTheme::Hint::CursorFlashTime).getInt32();
}


bool LThemeHints::showShortcutsInContextMenus() const
{
    return m_showShortcutsInContextMenus >= 0
               ? m_showShortcutsInContextMenus != 0
               : themeableHint(LPlatformTheme::Hint::ShowShortcutsInContextMenus).getBool();
}

void LThemeHints::setShowShortcutsInContextMenus(bool showShortcutsInContextMenus)
{
    m_showShortcutsInContextMenus = showShortcutsInContextMenus;
}

LTabFocusCtrls LThemeHints::tabFocusCtrls() const
{
    if(m_tabFocusCtrls >=0){
        return LTabFocusCtrls(m_tabFocusCtrls);
    }else{
        return LTabFocusCtrls::All;
    }
}

void LThemeHints::setTabFocusCtrls(LTabFocusCtrls v)
{
    m_tabFocusCtrls = int(v);
    //emit tabFocusCtrlsChanged(v);
}

bool LThemeHints::useHoverEffects() const
{
    return (m_uiEffects >= 0 ?
                m_uiEffects :
                themeableHint(LPlatformTheme::Hint::UiEffects).getInt32()) & int(LPlatformTheme::UiEffect::Hover);
}

void LThemeHints::setUseHoverEffects(bool useHoverEffects)
{
    if (m_uiEffects >= 0 && useHoverEffects == bool(m_uiEffects & int(LPlatformTheme::UiEffect::Hover))){
        return;
    }
    if (m_uiEffects == -1){
        m_uiEffects = 0;
    }
    if (useHoverEffects){
        m_uiEffects |= int(LPlatformTheme::UiEffect::Hover);
    }
    else{
        m_uiEffects &= ~int(LPlatformTheme::UiEffect::Hover);
    }
}

int LThemeHints::wheelScrollLines() const
{
    if (m_wheelScrollLines > 0){
        return m_wheelScrollLines;
    }
    return themeableHint(LPlatformTheme::Hint::WheelScrollLines).getInt32();

}

void LThemeHints::setWheelScrollLines(int scrollLines)
{
    m_wheelScrollLines = scrollLines;
}

void LThemeHints::setMouseQuickSelectionThreshold(int threshold)
{
    m_mouseQuickSelectionThreshold = threshold;
}

int LThemeHints::mouseQuickSelectionThreshold() const
{
    if (m_mouseQuickSelectionThreshold >= 0){
        return m_mouseQuickSelectionThreshold;
    }
    return themeableHint(LPlatformTheme::Hint::MouseQuickSelectionThreshold).getInt32();
}


LK_NAMESPACE_END
