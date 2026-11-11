#include "lguievent.h"
#include "lguiapplication.h"
#include "lscreen.h"
LK_NAMESPACE_BEGIN


LInputEvent::LInputEvent(Type type, LFlags<LKeyModifier> modifiers)
: LEvent(type), m_modState(modifiers), m_ts(0)
{

}

LEnterEvent::LEnterEvent(const LPoint<double> &localPos,
                         const LPoint<double> &windowPos,
                         const LPoint<double> &screenPos)
    : LEvent(LEvent::Enter)
    , m_l(localPos)
    , m_w(windowPos)
    , m_s(screenPos)
{

}

LMouseEvent::LMouseEvent(Type type,
                         const LPoint<double> &localPos,
                         LMouseButton button,
                         LFlags<LMouseButton> buttons,
                         LFlags<LKeyModifier> modifiers)
    : LInputEvent(type, modifiers), m_l(localPos), m_w(localPos), m_b(button), m_mouseState(buttons|button), m_caps(0)

{
}

LMouseEvent::LMouseEvent(Type type,
            const LPoint<double> &localPos,
            const LPoint<double> &screenPos,
            LMouseButton button,
            LFlags<LMouseButton> buttons,
            LFlags<LKeyModifier> modifiers)
: LMouseEvent(type, localPos, localPos, screenPos, button, buttons, modifiers)
{
}

LMouseEvent::LMouseEvent(Type type,
            const LPoint<double> &localPos,
            const LPoint<double> &windowPos,
            const LPoint<double> &screenPos,
            LMouseButton button,
            LFlags<LMouseButton> buttons,
            LFlags<LKeyModifier> modifiers)
: LInputEvent(type, modifiers), m_l(localPos), m_w(windowPos), m_s(screenPos), m_b(button), m_mouseState(buttons|button), m_caps(0)
{
}

LMouseEvent::LMouseEvent(Type type,
            const LPoint<double> &localPos,
            const LPoint<double> &windowPos,
            const LPoint<double> &screenPos,
            LMouseButton button,
            LFlags<LMouseButton> buttons,
            LFlags<LKeyModifier> modifiers,
            LMouseEventSource source)
: LMouseEvent(type, localPos, windowPos, screenPos, button, buttons, modifiers)
{
    LGuiApplication::setMouseEventSource(this, source);
}


LMouseEventSource LMouseEvent::source() const
{
    return LGuiApplication::mouseEventSource(this);
}

LFlags<LMouseEventFlag> LMouseEvent::flags() const
{
    return LGuiApplication::mouseEventFlags(this);
}


LInputMethodEvent::LInputMethodEvent()
: LEvent(LEvent::InputMethod), m_replaceFrom(0), m_replaceLength(0)
{

}

LInputMethodEvent::LInputMethodEvent(const std::wstring &preeditText)
    : LEvent(LEvent::InputMethod), m_preedit(preeditText), m_replaceFrom(0), m_replaceLength(0)
{

}

LInputMethodEvent::LInputMethodEvent(const LInputMethodEvent &other)
    : LEvent(LEvent::InputMethod), m_preedit(other.m_preedit),
    m_commit(other.m_commit), m_replaceFrom(other.m_replaceFrom), m_replaceLength(other.m_replaceLength)
{

}

void LInputMethodEvent::setCommitString(const std::wstring &commitString, int replaceFrom, int replaceLength)
{
    m_commit = commitString;
    m_replaceFrom = replaceFrom;
    m_replaceLength = replaceLength;
}


LKeyEvent::LKeyEvent(Type type, uint32_t key, LFlags<LKeyModifier> modifiers, const std::wstring &text, bool autorep, uint16_t count)
    : LInputEvent(type, modifiers), txt(text), k(key),
    nScanCode(0), nVirtualKey(0), nModifiers(0),
    c(count), autor(autorep)
{
    if (type == LEvent::ShortcutOverride){
        ignore();
    }
}

LKeyEvent::LKeyEvent(Type type, uint32_t key, LFlags<LKeyModifier> modifiers, uint32_t nativeScanCode, uint32_t nativeVirtualKey, uint32_t nativeModifiers, const std::wstring &text, bool autorep, uint16_t count)
    : LInputEvent(type, modifiers), txt(text), k(key),
    nScanCode(nativeScanCode), nVirtualKey(nativeVirtualKey), nModifiers(nativeModifiers),
    c(count), autor(autorep)
{
    if (type == LEvent::ShortcutOverride){
        ignore();
    }
}



LFlags<LKeyModifier> LKeyEvent::modifiers() const
{
    if (key() == int(LKey::Shift)){
        return LInputEvent::modifiers()^uint32_t(LKeyModifier::Shift);
    }
    if (key() == int(LKey::Control)){
        return LInputEvent::modifiers()^uint32_t(LKeyModifier::Ctrl);
    }
    if (key() == int(LKey::Alt)){
        return LInputEvent::modifiers()^uint32_t(LKeyModifier::Alt);
    }
    if (key() == int(LKey::Meta)){
        return LInputEvent::modifiers()^uint32_t(LKeyModifier::Meta);
    }
    if (key() == int(LKey::AltGr)){
        return LInputEvent::modifiers()^uint32_t(LKeyModifier::GroupSwitch);
    }
    return LInputEvent::modifiers();
}

bool LKeyEvent::matches(LKeySequence::StandardKey matchKey) const
{
    //The keypad and group switch modifier should not make a difference
    uint32_t searchkey = (modifiers() | key()) & ~((uint32_t)LKeyModifier::Keypad | (uint32_t)LKeyModifier::GroupSwitch);
    const std::list<LKeySequence>& bindings = LKeySequence::keyBindings(matchKey);
    return find(bindings.cbegin(), bindings.cend(), LKeySequence(searchkey)) != bindings.end();
}


LFocusEvent::LFocusEvent(Type type, LFocusReason reason) :LEvent(type), m_reason(reason)
{

}

LWindowStateChangeEvent::LWindowStateChangeEvent(LFlags<LWindowState> s, bool isOverride)
 :LEvent(WindowStateChange), m_oldState(s), m_override(isOverride)
{

}

bool LWindowStateChangeEvent::isOverride() const
{
    return m_override;
}

/////
LPaintEvent::LPaintEvent(const std::shared_ptr<LRegion> paintRegion)
    : LEvent(LEvent::Paint), m_region(paintRegion), m_erased(false)
{

}

LPaintEvent::LPaintEvent(const LRect<int> &paintRect)
: LEvent(LEvent::Paint), m_erased(false)
{
    m_region = std::make_shared<LRegion>(paintRect);
}

/////
LMoveEvent::LMoveEvent(const LPoint<int> &pos, const LPoint<int> &oldPos)
    : LEvent(LEvent::Move), m_p(pos), m_oldp(oldPos)
{

}

/////
LResizeEvent::LResizeEvent(const LSize<int> &size, const LSize<int> &oldSize)
 : LEvent(LEvent::Resize), m_s(size), m_olds(oldSize)
{

}


LApplicationStateChangeEvent::LApplicationStateChangeEvent(LApplicationState state)
     : LEvent(LEvent::ApplicationStateChange), m_applicationState(state)
{

}

LWheelEvent::LWheelEvent(LPoint<double> pos, LPoint<double> globalPos,
                         LPoint<int> pixelDelta, LPoint<int> angleDelta,
                         LFlags<LMouseButton> buttons, LFlags<LKeyModifier> modifiers,
                         LScrollPhase phase, bool inverted, LMouseEventSource source)
    : LInputEvent(Wheel, modifiers), m_p(pos), m_g(globalPos),
    m_pixelD(pixelDelta), m_angleD(angleDelta),
    m_mouseState(buttons), m_src((uint32_t)source), m_invertedScrolling(inverted), m_ph((uint32_t)phase)
{
    if(std::abs(angleDelta.m_x) > std::abs(angleDelta.m_y)){
        m_orientation =  LOrientation::Horizontal;
        m_distance = angleDelta.m_x;
    }else{
        m_orientation =  LOrientation::Vertical;
        m_distance = angleDelta.m_y;
    }
}

LScreenOrientationChangeEvent::LScreenOrientationChangeEvent(std::shared_ptr<LScreen> screen, LScreenOrientation orientation)
 : LEvent(LEvent::OrientationChange), m_screen(screen), m_orientation(orientation)
{

}

std::shared_ptr<LScreen> LScreenOrientationChangeEvent::screen() const
{
    return m_screen;
}

LScreenOrientation LScreenOrientationChangeEvent::orientation() const
{
    return m_orientation;
}

LExposeEvent::LExposeEvent(const std::shared_ptr<LRegion> rgn)
    : LEvent(Expose)
    , m_rgn(rgn)
{

}

LContextMenuEvent::LContextMenuEvent(Reason reason,
                                     const LPoint<int> &pos,
                                     const LPoint<int> &globalPos,
                                     LFlags<LKeyModifier> modifiers)
: LInputEvent(ContextMenu, modifiers), m_p(pos), m_gp(globalPos), m_reason(reason)
{

}

LContextMenuEvent::LContextMenuEvent(Reason reason, const LPoint<int> &pos, const LPoint<int> &globalPos)
    : LContextMenuEvent(reason, pos, globalPos, LKeyModifier::No)
{

}

LPlatformSurfaceEvent::LPlatformSurfaceEvent(SurfaceEventType surfaceEventType)
    : LEvent(PlatformSurface)
    , m_surfaceEventType(surfaceEventType)
{

}

LShortcutEvent::LShortcutEvent(const LKeySequence &key, int id, bool ambiguous)
: LEvent(Shortcut), sequence(key), ambig(ambiguous), sid(id)
{

}

LScrollPrepareEvent::LScrollPrepareEvent(const LPoint<double> &startPos): LEvent(LEvent::ScrollPrepare), m_startPos(startPos)
{

}

void LScrollPrepareEvent::setViewportSize(const LSize<double> &size)
{
    m_viewportSize = size;
}

void LScrollPrepareEvent::setContentPosRange(const LRect<double> &rect)
{
    m_contentPosRange = rect;
}

void LScrollPrepareEvent::setContentPos(const LPoint<double> &pos)
{
    m_contentPos = pos;
}

LScrollEvent::LScrollEvent(const LPoint<double> &contentPos, const LPoint<double> &overshoot, ScrollState scrollState)
: LEvent(LEvent::Scroll), m_contentPos(contentPos), m_overshoot(overshoot), m_state(scrollState)
{

}


LK_NAMESPACE_END




