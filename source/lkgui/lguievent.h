#ifndef LGUIEVENT_H
#define LGUIEVENT_H
#include "lkcore/levent.h"
#include "lkcore/lflags.h"
#include "lkgui/lpoint.h"
#include <utility>
#include <list>
#include "lkcore/lvariant.h"
#include "lkgui/lguienums.h"
#include "lkgui/paint/lregion.h"
#include "lkgui/lkeysequence.h"
#include "lkgui/text/ltextformat.h"
LK_NAMESPACE_BEGIN

class LGuiApplication;
class LKGUIDEF LInputEvent : public LEvent
{
public:
    explicit LInputEvent(Type type, LFlags<LKeyModifier> modifiers = LKeyModifier::No);

    inline LFlags<LKeyModifier> modifiers() const { return m_modState; }
    inline void setModifiers(LFlags<LKeyModifier> amodifiers) { m_modState = amodifiers; }
    inline uint64_t timestamp() const { return m_ts; }
    inline void setTimestamp(uint64_t atimestamp) { m_ts = atimestamp; }
protected:
    LFlags<LKeyModifier>  m_modState;
    uint64_t m_ts=0;
};


class LKGUIDEF LEnterEvent : public LEvent
{
public:
    LEnterEvent(const LPoint<double> &localPos, const LPoint<double> &windowPos, const LPoint<double> &screenPos);

    inline LPoint<int> pos() const { return m_l.toPoint(); }
    inline LPoint<int> globalPos() const { return m_s.toPoint(); }
    inline int x() const { return std::lround(m_l.m_x); }
    inline int y() const { return std::lround(m_l.m_y); }
    inline int globalX() const { return std::lround(m_s.m_x); }
    inline int globalY() const { return std::lround(m_s.m_y); }

    const LPoint<double> &localPos() const { return m_l; }
    const LPoint<double> &windowPos() const { return m_w; }
    const LPoint<double> &screenPos() const { return m_s; }

protected:
    LPoint<double> m_l, m_w, m_s;
};

class LKGUIDEF LMouseEvent : public LInputEvent
{
public:
    LMouseEvent(Type type,
                const LPoint<double> &localPos,
                LMouseButton button,
                LFlags<LMouseButton> buttons,
                LFlags<LKeyModifier> modifiers);
    LMouseEvent(Type type,
                const LPoint<double> &localPos,
                const LPoint<double> &screenPos,
                LMouseButton button,
                LFlags<LMouseButton> buttons,
                LFlags<LKeyModifier> modifiers);
    LMouseEvent(Type type,
                const LPoint<double> &localPos,
                const LPoint<double> &windowPos,
                const LPoint<double> &screenPos,
                LMouseButton button,
                LFlags<LMouseButton> buttons,
                LFlags<LKeyModifier> modifiers);
    LMouseEvent(Type type,
                const LPoint<double> &localPos,
                const LPoint<double> &windowPos,
                const LPoint<double> &screenPos,
                LMouseButton button,
                LFlags<LMouseButton> buttons,
                LFlags<LKeyModifier> modifiers,
                LMouseEventSource source);

    inline LPoint<int> pos() const { return m_l.toPoint(); }
    inline LPoint<int> globalPos() const { return m_s.toPoint(); }
    inline int x() const { return std::lround(m_l.m_x); }
    inline int y() const { return std::lround(m_l.m_y); }
    inline int globalX() const { return std::lround(m_s.m_x); }
    inline int globalY() const { return std::lround(m_s.m_y); }

    const LPoint<double>  &localPos() const { return m_l; }
    const LPoint<double>  &windowPos() const { return m_w; }
    const LPoint<double>  &screenPos() const { return m_s; }

    inline LMouseButton button() const { return m_b; }
    inline LFlags<LMouseButton> buttons() const { return m_mouseState; }

    inline void setLocalPos(const LPoint<double> &localPosition) { m_l = localPosition; }

    LMouseEventSource source() const;
    LFlags<LMouseEventFlag> flags() const;

protected:
    LPoint<double> m_l, m_w, m_s;
    LMouseButton m_b;
    LFlags<LMouseButton> m_mouseState;
    uint32_t m_caps;
    friend class LGuiApplication;
};

class LKGUIDEF LInputMethodEvent : public LEvent
{
public:
    LInputMethodEvent();
    LInputMethodEvent(const std::wstring &preeditText);
    LInputMethodEvent(const LInputMethodEvent &other);

    void setCommitString(const std::wstring &commitString, int replaceFrom = 0, int replaceLength = 0);
    inline const std::wstring &preeditString() const { return m_preedit; }

    inline const std::wstring &commitString() const { return m_commit; }
    inline int replacementStart() const { return m_replaceFrom; }
    inline int replacementLength() const { return m_replaceLength; }
private:
    std::wstring m_preedit;
    std::wstring m_commit;
    int m_replaceFrom;
    int m_replaceLength;
};


class LKGUIDEF LKeyEvent : public LInputEvent
{
public:
    LKeyEvent(Type type, uint32_t key, LFlags<LKeyModifier> modifiers, const std::wstring& text = std::wstring(),
              bool autorep = false, uint16_t count = 1);
    LKeyEvent(Type type, uint32_t key, LFlags<LKeyModifier> modifiers,
              uint32_t nativeScanCode, uint32_t nativeVirtualKey, uint32_t nativeModifiers,
              const std::wstring &text = std::wstring(), bool autorep = false, uint16_t count = 1);

    uint32_t key() const { return k; }
    LFlags<LKeyModifier> modifiers() const;
    inline const std::wstring& text() const { return txt; }
    inline bool isAutoRepeat() const { return autor; }
    inline int count() const { return int(c); }

    inline uint32_t nativeScanCode() const { return nScanCode; }
    inline uint32_t nativeVirtualKey() const { return nVirtualKey; }
    inline uint32_t nativeModifiers() const { return nModifiers; }

    bool matches(LKeySequence::StandardKey key) const;
protected:
    std::wstring txt;
    uint32_t k;
    uint32_t nScanCode;
    uint32_t nVirtualKey;
    uint32_t nModifiers;
    uint16_t c;
    uint16_t autor:1;
    // ushort reserved:15;
};

class LKGUIDEF LFocusEvent : public LEvent
{
public:
    LFocusEvent(Type type, LFocusReason reason=LFocusReason::Other);

    inline bool gotFocus() const { return type() == FocusIn; }
    inline bool lostFocus() const { return type() == FocusOut; }

    inline LFocusReason reason() const{
        return m_reason;
    }
private:
    LFocusReason m_reason;
};

class LKGUIDEF LWindowStateChangeEvent: public LEvent
{
public:
    LWindowStateChangeEvent(LFlags<LWindowState> aOldState, bool isOverride = false);

    inline LFlags<LWindowState> oldState() const { return m_oldState; }
    bool isOverride() const;

private:
    LFlags<LWindowState> m_oldState;
    bool m_override;
};


class LKGUIDEF LUpdateLaterEvent : public LEvent
{
public:
    LUpdateLaterEvent(std::shared_ptr<LRegion> paintRegion)
        : LEvent(UpdateLater), m_region(paintRegion)
    {
    }

    explicit LUpdateLaterEvent(LRect<int> paintRect)
        : LEvent(UpdateLater)
    {
        m_region = std::make_shared<LRegion>(paintRect);
    }

    inline const std::shared_ptr<LRegion> region() const { return m_region; }
protected:
    std::shared_ptr<LRegion> m_region;
};


class LKGUIDEF LPaintEvent : public LEvent
{
public:
    LPaintEvent(const std::shared_ptr<LRegion> paintRegion);
    LPaintEvent(const LRect<int> &paintRect);

    inline const LRect<int> rect() const { return m_region->boundingRect(); }
    inline const std::shared_ptr<LRegion> region() const { return m_region; }

protected:
    std::shared_ptr<LRegion>  m_region;
    bool m_erased;
};

class LKGUIDEF LMoveEvent : public LEvent
{
public:
    LMoveEvent(const LPoint<int> &pos, const LPoint<int> &oldPos);

    inline const LPoint<int> &pos() const { return m_p; }
    inline const LPoint<int> &oldPos() const { return m_oldp;}
protected:
    LPoint<int> m_p, m_oldp;
};

class LKGUIDEF LResizeEvent : public LEvent
{
public:
    LResizeEvent(const LSize<int> &size, const LSize<int> &oldSize);
    inline const LSize<int> &size() const { return m_s; }
    inline const LSize<int> &oldSize()const { return m_olds;}
protected:
    LSize<int> m_s, m_olds;
};

class LKGUIDEF LApplicationStateChangeEvent : public LEvent
{
public:
    LApplicationStateChangeEvent(LApplicationState state);
    LApplicationState applicationState() const{
        return m_applicationState;
    }

private:
    LApplicationState m_applicationState;
};


class LKGUIDEF LWheelEvent : public LInputEvent
{
public:
    enum { DefaultDeltasPerStep = 120 };

    LWheelEvent(LPoint<double> pos, LPoint<double> globalPos,
                LPoint<int> pixelDelta, LPoint<int> angleDelta,
                LFlags<LMouseButton> buttons, LFlags<LKeyModifier> modifiers, LScrollPhase phase,
                bool inverted, LMouseEventSource source = LMouseEventSource::NotSynthesized);

    inline const LPoint<int>& pixelDelta() const { return m_pixelD; }
    inline const LPoint<int>& angleDelta() const { return m_angleD; }


    inline const LPoint<double>&  position() const { return m_p; }
    inline const LPoint<double>&  globalPosition() const { return m_g; }

    inline LFlags<LMouseButton> buttons() const { return m_mouseState; }

    inline LScrollPhase phase() const { return LScrollPhase(m_ph); }
    inline bool inverted() const { return m_invertedScrolling; }

    LMouseEventSource source() const { return LMouseEventSource(m_src); }

protected:
    LPoint<double> m_p;
    LPoint<double> m_g;
    LPoint<int> m_pixelD;
    LPoint<int> m_angleD;
    int m_distance = 0;
    LOrientation m_orientation = LOrientation::Vertical;
    LFlags<LMouseButton> m_mouseState = LMouseButton::No;
    uint32_t m_unused : 2; // Kept for binary compatibility
    uint32_t m_src: 2;
    bool m_invertedScrolling : 1;
    uint32_t m_ph : 3;
    int m_reserved : 24;
};

class LScreen;
class LKGUIDEF LScreenOrientationChangeEvent : public LEvent
{
public:
    LScreenOrientationChangeEvent(std::shared_ptr<LScreen> screen,
                                  LScreenOrientation orientation);

    std::shared_ptr<LScreen>  screen() const;
    LScreenOrientation orientation() const;

private:
    std::shared_ptr<LScreen> m_screen;
    LScreenOrientation m_orientation;
};

class LKGUIDEF LExposeEvent : public LEvent
{
public:
    LExposeEvent(const std::shared_ptr<LRegion> rgn);
    inline const std::shared_ptr<LRegion>  region() const { return m_rgn; }
protected:
    std::shared_ptr<LRegion> m_rgn;
};


class LKGUIDEF LContextMenuEvent : public LInputEvent
{
public:
    enum Reason { Mouse, Keyboard, Other };

    LContextMenuEvent(Reason reason, const LPoint<int> &pos, const LPoint<int> &globalPos,
                      LFlags<LKeyModifier> modifiers);
    LContextMenuEvent(Reason reason, const LPoint<int> &pos, const LPoint<int> &globalPos);
//    LContextMenuEvent(Reason reason, const LPoint<int> &pos);

    inline int x() const { return m_p.m_x; }
    inline int y() const { return m_p.m_y; }
    inline int globalX() const { return m_gp.m_x; }
    inline int globalY() const { return m_gp.m_y; }

    inline const LPoint<int>& pos() const { return m_p; }
    inline const LPoint<int>& globalPos() const { return m_gp; }

    inline Reason reason() const { return Reason(m_reason); }
protected:
    LPoint<int> m_p;
    LPoint<int> m_gp;
    uint8_t m_reason;
};

class LKGUIDEF LPlatformSurfaceEvent : public LEvent
{
public:
    enum SurfaceEventType {
        SurfaceCreated,
        SurfaceAboutToBeDestroyed
    };

    LPlatformSurfaceEvent(SurfaceEventType surfaceEventType);

    inline SurfaceEventType surfaceEventType() const { return m_surfaceEventType; }
protected:
    SurfaceEventType m_surfaceEventType;
};


class LKGUIDEF LShortcutEvent : public LEvent
{
public:
    LShortcutEvent(const LKeySequence &key, int id, bool ambiguous = false);

    inline const LKeySequence &key() const { return sequence; }
    inline int shortcutId() const { return sid; }
    inline bool isAmbiguous() const { return ambig; }
protected:
    LKeySequence sequence;
    bool ambig;
    int  sid;
};


class LKGUIDEF LScrollPrepareEvent : public LEvent
{
public:
    LScrollPrepareEvent(const LPoint<double> &startPos);

    const LPoint<double>& startPos() const { return m_startPos; }

    const LSize<double>& viewportSize() const { return m_viewportSize; }
    const LRect<double>& contentPosRange() const { return m_contentPosRange; }
    const LPoint<double>& contentPos() const { return m_contentPos; }

    void setViewportSize(const LSize<double> &size);
    void setContentPosRange(const LRect<double> &rect);
    void setContentPos(const LPoint<double> &pos);

private:
    LRect<double> m_contentPosRange;
    LSize<double> m_viewportSize;
    LPoint<double> m_startPos;
    LPoint<double> m_contentPos;
};


class LKGUIDEF LScrollEvent : public LEvent
{
public:
    enum ScrollState
    {
        ScrollStarted,
        ScrollUpdated,
        ScrollFinished
    };

    LScrollEvent(const LPoint<double> &contentPos, const LPoint<double> &overshoot, ScrollState scrollState);

    const LPoint<double>& contentPos() const { return m_contentPos; }
    const LPoint<double>& overshootDistance() const { return m_overshoot; }
    ScrollState scrollState() const { return m_state; }

private:
    LPoint<double> m_contentPos;
    LPoint<double> m_overshoot;
    LScrollEvent::ScrollState m_state;
};
LK_NAMESPACE_END
#endif // LGUIEVENT_H
