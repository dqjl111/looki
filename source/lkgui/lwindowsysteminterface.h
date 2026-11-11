#ifndef LWINDOWSYSTEMINTERFACE_H
#define LWINDOWSYSTEMINTERFACE_H

#include "lwindow.h"
#include "lplatformscreen.h"
#include"lkcore/leventloop.h"
#include "lkgui/lguievent.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LWinSysEvent {
public:
    enum Type {
        UserInputEvent = 0x100,
        Close = UserInputEvent | 0x01,
        GeometryChange = 0x02,
        Enter = UserInputEvent | 0x03,
        Leave = UserInputEvent | 0x04,
        ActivatedWindow = 0x05,
        WindowStateChanged = 0x06,
        Mouse = UserInputEvent | 0x07,
        FrameStrutMouse = UserInputEvent | 0x08,  // ### Qt6 remove
        Wheel = UserInputEvent | 0x09,
        Key = UserInputEvent | 0x0a,
        Touch = UserInputEvent | 0x0b,
        ScreenOrientation = 0x0c,
        ScreenGeometry = 0x0d,
        ScreenAvailableGeometry = 0x0e,
        ScreenLogicalDotsPerInch = 0x0f,
        ScreenRefreshRate = 0x10,
        ThemeChange = 0x11,
        Expose = 0x12,
        FileOpen = UserInputEvent | 0x13,
        Tablet = UserInputEvent | 0x14,
        TabletEnterProximity = UserInputEvent | 0x15,
        TabletLeaveProximity = UserInputEvent | 0x16,
        PlatformPanel = UserInputEvent | 0x17,
        ContextMenu = UserInputEvent | 0x18,
        EnterWhatsThisMode = UserInputEvent | 0x19,
        Gesture = UserInputEvent | 0x1a,
        ApplicationStateChanged = 0x19,
        FlushEvents = 0x20,
        WindowScreenChanged = 0x21,
        SafeAreaMarginsChanged = 0x22,
        ApplicationTermination = 0x23
    };

    enum {
        Synthetic = 0x1,
        NullWindow = 0x2
    };

    explicit LWinSysEvent(Type t)
        : m_type(t), m_flags(0), m_eventAccepted(true) { }
    virtual ~LWinSysEvent() { }

    bool synthetic() const  { return m_flags & Synthetic; }
    bool nullWindow() const { return m_flags & NullWindow; }

    Type m_type;
    int m_flags;
    bool m_eventAccepted;
};


namespace wse{
class LKGUIDEF LWinEvent : public LWinSysEvent {
public:
    LWinEvent(Type t, std::shared_ptr<LWindow> w): LWinSysEvent(t), m_window(w){ }
    std::shared_ptr<LWindow> m_window;
};

class LKGUIDEF LExposeEvent : public LWinEvent {
public:
    LExposeEvent(std::shared_ptr<LWindow> w, const std::shared_ptr<LRegion> region);
    bool m_isExposed;
    std::shared_ptr<LRegion> m_region;
};


class LKGUIDEF LGeometryChangeEvent : public LWinEvent {
public:
    LGeometryChangeEvent(std::shared_ptr<LWindow> w, const LRect<int> &newGeometry);
    LRect<int> m_requestedGeometry;
    LRect<int> m_newGeometry;
};

class LEnterEvent : public LWinEvent {
public:
    LEnterEvent(std::shared_ptr<LWindow> w,
                const LPoint<double> &local,
                const LPoint<double> &global)
        : LWinEvent(Enter, w), m_localPos(local), m_globalPos(global)
    { }
    const LPoint<double> m_localPos;
    const LPoint<double> m_globalPos;
};


class LKGUIDEF LActivatedWindowEvent : public LWinEvent {
public:
    LActivatedWindowEvent(std::shared_ptr<LWindow> w, LFocusReason r)
        : LWinEvent(ActivatedWindow, w), m_reason(r)
    { }
    LFocusReason m_reason;
};

class LWindowStateChangedEvent : public LWinEvent {
public:
    LWindowStateChangedEvent(std::shared_ptr<LWindow> w,
                            LFlags<LWindowState> _newState,
                            LFlags<LWindowState> _oldState)
        : LWinEvent(WindowStateChanged, w), m_newState(_newState), m_oldState(_oldState)
    { }

    LFlags<LWindowState> m_newState;
    LFlags<LWindowState> m_oldState;
};

class LKGUIDEF LWindowScreenChangedEvent : public LWinEvent {
public:
    LWindowScreenChangedEvent(std::shared_ptr<LWindow> w, std::shared_ptr<LScreen> s)
        : LWinEvent(WindowScreenChanged, w), screen(s)
    { }

    std::shared_ptr<LScreen> screen;
};

class LKGUIDEF LApplicationStateChangedEvent : public LWinSysEvent {
public:
    LApplicationStateChangedEvent(LApplicationState newState, bool forcePropagate = false)
        : LWinSysEvent(ApplicationStateChanged), m_newState(newState), m_forcePropagate(forcePropagate)
    { }

    LApplicationState m_newState;
    bool m_forcePropagate;
};

class LKGUIDEF LUserEvent : public LWinEvent {
public:
    LUserEvent(std::shared_ptr<LWindow> w, uint64_t time, Type t)
        : LWinEvent(t, w), m_timestamp(time)
    {
        if (w == nullptr){
            m_flags |= NullWindow;
        }
    }
    uint64_t m_timestamp;
};

class LKGUIDEF LInputEvent: public LUserEvent {
public:
    LInputEvent(std::shared_ptr<LWindow> w, uint64_t time, Type t, LFlags<LKeyModifier> mods)
        : LUserEvent(w, time, t), m_modifiers(mods) {}
    LFlags<LKeyModifier> m_modifiers;
};

class LKGUIDEF LMouseEvent : public LInputEvent {
public:
    LMouseEvent(std::shared_ptr<LWindow> w, uint32_t time,
                const LPoint<double> &local, const LPoint<double> &global,
                LFlags<LMouseButton> state, LFlags<LKeyModifier> mods,
                LMouseButton b, LEvent::Type type,
                LMouseEventSource src = LMouseEventSource::NotSynthesized,
                bool frame = false)
        : LInputEvent(w, time, Mouse, mods),
        m_localPos(local), m_globalPos(global), m_buttons(state),
        m_source(src), m_nonClientArea(frame), m_button(b), m_buttonType(type) { }

    LPoint<double> m_localPos;
    LPoint<double> m_globalPos;
    LFlags<LMouseButton> m_buttons;
    LMouseEventSource m_source;
    bool m_nonClientArea;
    LMouseButton m_button;
    LEvent::Type m_buttonType;
};

class LKGUIDEF LWheelEvent : public LInputEvent {
public:
    LWheelEvent(std::shared_ptr<LWindow> w, uint32_t time,
                const LPoint<double> &local, const LPoint<double> &global,
                LPoint<int> pixelD, LPoint<int> angleD,
                LFlags<LKeyModifier> mods,
                LScrollPhase phase = LScrollPhase::No,
                LMouseEventSource src = LMouseEventSource::NotSynthesized,
                bool inverted = false);

    LPoint<int> m_pixelDelta;
    LPoint<int> m_angleDelta;
    LPoint<double> m_localPos;
    LPoint<double> m_globalPos;
    LScrollPhase m_phase;
    LMouseEventSource m_source;
    bool m_inverted;
};

class LKGUIDEF LKeyEvent : public LInputEvent {
public:
    LKeyEvent(std::shared_ptr<LWindow> w, uint64_t time, LEvent::Type t, int k,
              LFlags<LKeyModifier> mods, const std::wstring& text = std::wstring(),
              bool autorep = false, uint16_t count = 1)
        :LInputEvent(w, time, Key, mods), m_key(k), m_text(text), m_repeat(autorep),
        m_repeatCount(count), m_keyType(t),
        m_nativeScanCode(0), m_nativeVirtualKey(0), m_nativeModifiers(0) { }

    LKeyEvent(std::shared_ptr<LWindow> w, uint64_t time, LEvent::Type t, int k,
             LFlags<LKeyModifier> mods, uint32_t nativeSC, uint32_t nativeVK, uint32_t nativeMods,
             const std::wstring& text = std::wstring(), bool autorep = false, uint16_t count = 1)
        :LInputEvent(w, time, Key, mods), m_key(k), m_text(text), m_repeat(autorep),
        m_repeatCount(count), m_keyType(t),
        m_nativeScanCode(nativeSC), m_nativeVirtualKey(nativeVK), m_nativeModifiers(nativeMods) { }

    int m_key;
    std::wstring m_text;
    bool m_repeat;
    uint16_t m_repeatCount;
    LEvent::Type m_keyType;
    uint32_t m_nativeScanCode;
    uint32_t m_nativeVirtualKey;
    uint32_t m_nativeModifiers;
};

//class LTouchEvent : public LInputEvent {
//public:
//    TouchEvent(QWindow *w, ulong time, QEvent::Type t, QTouchDevice *dev,
//               const QList<QTouchEvent::TouchPoint> &p, Qt::KeyboardModifiers mods)
//        :InputEvent(w, time, Touch, mods), device(dev), points(p), touchType(t) { }
//    LTouchDevice *device;
//    std::list<QTouchEvent::TouchPoint> points;
//    QEvent::Type touchType;
//};

class LKGUIDEF LScreenOrientationEvent : public LWinSysEvent {
public:
    LScreenOrientationEvent(std::shared_ptr<LScreen> s, LScreenOrientation o)
        : LWinSysEvent(ScreenOrientation), m_screen(s), m_orientation(o) { }
    std::shared_ptr<LScreen> m_screen;
    LScreenOrientation m_orientation;
};

class LKGUIDEF LScreenGeometryEvent : public LWinSysEvent {
public:
    LScreenGeometryEvent(std::shared_ptr<LScreen> s, const LRect<int> &g, const LRect<int> &ag)
        : LWinSysEvent(ScreenGeometry), m_screen(s), m_geometry(g), m_availableGeometry(ag) { }
    std::shared_ptr<LScreen> m_screen;
    LRect<int> m_geometry;
    LRect<int> m_availableGeometry;
};

class LKGUIDEF LScreenLogicalDotsPerInchEvent : public LWinSysEvent {
public:
    LScreenLogicalDotsPerInchEvent(std::shared_ptr<LScreen> s, double dx, double dy)
        : LWinSysEvent(ScreenLogicalDotsPerInch), m_screen(s), m_dpiX(dx), m_dpiY(dy) { }
    std::shared_ptr<LScreen> m_screen;
    double m_dpiX;
    double m_dpiY;
};

class LKGUIDEF LScreenRefreshRateEvent : public LWinSysEvent {
public:
    LScreenRefreshRateEvent(std::shared_ptr<LScreen> s, double r)
        : LWinSysEvent(ScreenRefreshRate), m_screen(s), m_rate(r) { }
    std::shared_ptr<LScreen> m_screen;
    double m_rate;
};


//class LFileOpenEvent : public LWinSysEvent {
//public:
//    LFileOpenEvent(const std::string& fileName)
//        : LWinSysEvent(FileOpen), url(LUrl::fromLocalFile(fileName))
//    { }
//    LFileOpenEvent(const QUrl &url)
//        : LWinSysEvent(FileOpen), url(url)
//    { }
//    LUrl url;
//};

class LKGUIDEF LTabletEvent : public LInputEvent {
public:
    static void setPlatformSynthesizesMouse(bool v);

    LTabletEvent(std::shared_ptr<LWindow> w, uint32_t time,
                const LPoint<double> &local, const LPoint<double> &global,
                int device, int pointerType, LFlags<LMouseButton> b, double pressure,
                int xTilt, int yTilt, double tpressure,
                double rotation, int z, int64_t uid, LFlags<LKeyModifier> mods)
        : LInputEvent(w, time, Tablet, mods),
        m_buttons(b), m_local(local), m_global(global), m_device(device), m_pointerType(pointerType),
        m_pressure(pressure), m_xTilt(xTilt), m_yTilt(yTilt), m_tangentialPressure(tpressure),
        m_rotation(rotation), m_z(z), m_uid(uid) { }

    LFlags<LMouseButton> m_buttons;
    LPoint<double> m_local;
    LPoint<double> m_global;
    int m_device;
    int m_pointerType;
    double m_pressure;
    int m_xTilt;
    int m_yTilt;
    double m_tangentialPressure;
    double m_rotation;
    int m_z;
    int64_t m_uid;
    static bool m_platformSynthesizesMouse;
};

class LKGUIDEF LTabletProximityEvent : public LInputEvent {
public:
    LTabletProximityEvent(uint32_t time, Type t, int device, int pointerType, int64_t uid)
        : LInputEvent(nullptr, time, t, LKeyModifier::No),
        m_device(device), m_pointerType(pointerType), m_uid(uid) { }
    int m_device;
    int m_pointerType;
    int64_t m_uid;
};


class LKGUIDEF LContextMenuEvent : public LWinEvent {
public:
    LContextMenuEvent(std::shared_ptr<LWindow> w,
                      bool mouseTriggered,
                      const LPoint<int> &pos,
                      const LPoint<int> &globalPos,
                      LFlags<LKeyModifier> modifiers)
        : LWinEvent(ContextMenu, w), m_mouseTriggered(mouseTriggered), m_pos(pos),
        m_globalPos(globalPos), m_modifiers(modifiers) { }

    bool m_mouseTriggered;
    LPoint<int> m_pos;       // Only valid if triggered by mouse
    LPoint<int> m_globalPos; // Only valid if triggered by mouse
    LFlags<LKeyModifier> m_modifiers;
};

class LKGUIDEF LFlushEventsEvent : public LWinSysEvent {
public:
    LFlushEventsEvent(LFlags<LProcessEvents>  f = LProcessEvents::All)
        : LWinSysEvent(FlushEvents)
        , flags(f)
    { }
    LFlags<LProcessEvents> flags;
};

}


class LKGUIDEF LWinSysEventList {
    std::list<std::shared_ptr<LWinSysEvent> > m_impl;
    mutable std::mutex m_mutex;
public:
    LWinSysEventList() : m_impl(), m_mutex() {}
    ~LWinSysEventList() { clear(); }

    void clear(){
        const std::unique_lock<std::mutex> lock(m_mutex);
        m_impl.clear();
    }

    void prepend(std::shared_ptr<LWinSysEvent> e){
        if(e != nullptr){
            const std::unique_lock<std::mutex> lock(m_mutex);
            m_impl.push_front(e);
        }
    }

    std::shared_ptr<LWinSysEvent> takeFirstOrReturnNull(){
        const std::unique_lock<std::mutex> lock(m_mutex);
        if(m_impl.empty()){
            return nullptr;
        }
        std::shared_ptr<LWinSysEvent> ret = m_impl.front();
        m_impl.pop_front();
        return ret;
    }

    //Todo remove qt code no use for lk
    std::shared_ptr<LWinSysEvent> takeFirstNonUserInputOrReturnNull()
    {
        const std::unique_lock<std::mutex> lock(m_mutex);
        for (std::shared_ptr<LWinSysEvent> e : m_impl) {
            if (!(e->m_type & LWinSysEvent::UserInputEvent)){
                m_impl.remove(e);
                return e;
            }
        }
        return nullptr;
    }

    std::list<std::shared_ptr<LWinSysEvent> > takeAllNonUserInput(){
        std::list<std::shared_ptr<LWinSysEvent> > ret;
        const std::unique_lock<std::mutex> lock(m_mutex);
        std::list<std::shared_ptr<LWinSysEvent> >::iterator iter = m_impl.begin();
        for(; iter != m_impl.end();){
            std::shared_ptr<LWinSysEvent> e = *iter;
            if (!(e->m_type & LWinSysEvent::UserInputEvent)){
                iter = m_impl.erase(iter);
                ret.push_back(e);
            }else{
                ++iter;
            }
        }
        return ret;
    }

    bool hasNonUserInputEvents()
    {
        const std::unique_lock<std::mutex> lock(m_mutex);
        for (const std::shared_ptr<LWinSysEvent>& e : m_impl) {
            if (!(e->m_type & LWinSysEvent::UserInputEvent)){
                return true;
            }
        }
        return false;
    }

    void append(std::shared_ptr<LWinSysEvent> e){
        if(e != nullptr){
            const std::unique_lock<std::mutex> lock(m_mutex);
            m_impl.push_back(e);
        }
    }

    int count() const{
        const std::unique_lock<std::mutex> lock(m_mutex);
        return m_impl.size();
    }

    bool isEmpty() const{
        const std::unique_lock<std::mutex> lock(m_mutex);
        return m_impl.empty();
    }

    std::shared_ptr<LWinSysEvent> peekAtFirstOfType(LWinSysEvent::Type t) const
    {
        const std::unique_lock<std::mutex> lock(m_mutex);
        for (std::shared_ptr<LWinSysEvent> e : m_impl) {
            if (e->m_type == t){
                return e;
            }
        }
        return nullptr;
    }
    void remove(const std::shared_ptr<LWinSysEvent> e)
    {
        const std::unique_lock<std::mutex> lock(m_mutex);
        m_impl.remove(e);
    }
};


class LKGUIDEF LWindowSystemInterface
{
public:
    LWindowSystemInterface();

    static bool sendWinSysEvents(LFlags<LProcessEvents>  flags= LProcessEvents::All);

    static bool handleNativeEvent(std::shared_ptr<LWindow>  window, const std::string &eventType, void *message, lintptr *result);

    static bool handleWinSysEvent(std::shared_ptr<LWinSysEvent> ev,
                                        LDeliverType deliverType = LDeliverType::Default);

    static void handleExposeEvent(std::shared_ptr<LWindow>  window,
                                  const std::shared_ptr<LRegion> region,
                                  LDeliverType deliverType = LDeliverType::Default);

    static void handleGeometryChange(std::shared_ptr<LWindow> window,
                                     const LRect<int> &newRect,
                                     LDeliverType deliverType = LDeliverType::Default);

    static bool handleCloseEvent(std::shared_ptr<LWindow> window, LDeliverType deliverType = LDeliverType::Default);

    // Changes to the screen        
    static void handleScreenAdded(std::shared_ptr<LPlatformScreen> screen,
                                  bool isPrimary = false);
    static void handleScreenRemoved(std::shared_ptr<LPlatformScreen> screen);

    static void handlePrimaryScreenChanged(std::shared_ptr<LPlatformScreen> newPrimary);

    static void handleScreenOrientationChange(std::shared_ptr<LScreen> screen,
                                              LScreenOrientation newOrientation);
    static void handleScreenGeometryChange(std::shared_ptr<LScreen> screen,
                                           const LRect<int> &newGeometry,
                                           const LRect<int> &newAvailableGeometry);

    static void handleScreenLogicalDotsPerInchChange(std::shared_ptr<LScreen> screen,
                                                     double newDpiX,
                                                     double newDpiY);
    static void handleScreenRefreshRateChange(std::shared_ptr<LScreen> screen, double newRefreshRate);

    static void handleWindowScreenChanged(std::shared_ptr<LWindow> window,
                                          std::shared_ptr<LScreen> newScreen,
                                          LDeliverType deliverType = LDeliverType::Default);

    static void handleWindowStateChanged(std::shared_ptr<LWindow> window,
                                         LFlags<LWindowState> newState,
                                         int oldState = -1,
                                         LDeliverType deliverType = LDeliverType::Default);


    static bool handleMouseEvent(std::shared_ptr<LWindow> window,
                                 const LPoint<double> &local, const LPoint<double> &global,
                                 LFlags<LMouseButton> state, LMouseButton button, LEvent::Type type,
                                 LFlags<LKeyModifier> mods = LKeyModifier::No,
                                 LMouseEventSource source = LMouseEventSource::NotSynthesized,
                                 LDeliverType deliverType = LDeliverType::Default);

    static bool handleFrameStrutMouseEvent(std::shared_ptr<LWindow> window,
                                           const LPoint<double> &local, const LPoint<double> &global,
                                           LFlags<LMouseButton> state, LMouseButton button, LEvent::Type type,
                                           LFlags<LKeyModifier> mods = LKeyModifier::No,
                                           LMouseEventSource source = LMouseEventSource::NotSynthesized);


    static void handleEnterEvent(std::shared_ptr<LWindow> window,
                                 const LPoint<double> &local = LPoint<double>(),
                                 const LPoint<double>& global = LPoint<double>(),
                                 LDeliverType deliverType = LDeliverType::Default);

    static void handleLeaveEvent(std::shared_ptr<LWindow> window, LDeliverType deliverType = LDeliverType::Default);

    static void handleEnterLeaveEvent(std::shared_ptr<LWindow> enter,
                                      std::shared_ptr<LWindow> leave,
                                      const LPoint<double> &local = LPoint<double>(),
                                      const LPoint<double>& global = LPoint<double>());

    static void handleWindowActivated(std::shared_ptr<LWindow> window,
                                      LFocusReason r = LFocusReason::Other);


    static bool sendShortcutOverrideEvent(std::shared_ptr<LObject> o,
                                           uint64_t timestamp,
                                           int k,
                                           LFlags<LKeyModifier> mods,
                                           const std::wstring &text = std::wstring(),
                                           bool autorep = false,
                                           uint16_t count = 1);

    static bool handleShortcutEvent(std::shared_ptr<LWindow> window, uint64_t timestamp,
                                    int k, LFlags<LKeyModifier> mods, uint32_t nativeScanCode,
                                    uint32_t nativeVirtualKey, uint32_t nativeModifiers,
                                    const std::wstring& text = std::wstring(), bool autorep = false,
                                    uint16_t count = 1);

    static bool handleExtendedKeyEvent(std::shared_ptr<LWindow> window, LEvent::Type type,
                                       int key, LFlags<LKeyModifier> modifiers,
                                       uint32_t nativeScanCode, uint32_t nativeVirtualKey,
                                       uint32_t nativeModifiers,
                                       const std::wstring& text = std::wstring(), bool autorep = false,
                                       uint16_t count = 1);
    static bool handleExtendedKeyEvent(std::shared_ptr<LWindow>  window, uint64_t timestamp,
                                       LEvent::Type type,
                                       int key, LFlags<LKeyModifier> modifiers,
                                       uint32_t nativeScanCode, uint32_t nativeVirtualKey,
                                       uint32_t nativeModifiers,
                                       const std::wstring& text = std::wstring(), bool autorep = false,
                                       uint16_t count = 1);

    static bool handleWheelEvent(std::shared_ptr<LWindow> window,
                                 const LPoint<double> &local, const LPoint<double> &global,
                                 LPoint<int> pixelDelta, LPoint<int> angleDelta,
                                 LFlags<LKeyModifier> mods = LKeyModifier::No,
                                 LScrollPhase phase = LScrollPhase::No,
                                 LMouseEventSource source = LMouseEventSource::NotSynthesized);

    static bool handleWheelEvent(std::shared_ptr<LWindow> window, uint64_t timestamp,
                                 const LPoint<double> &local, const LPoint<double> &global,
                                 LPoint<int> pixelDelta, LPoint<int> angleDelta,
                                 LFlags<LKeyModifier> mods = LKeyModifier::No,
                                 LScrollPhase phase = LScrollPhase::No,
                                 LMouseEventSource source = LMouseEventSource::NotSynthesized,
                                 bool inverted = false);

    static bool flushWinSysEvents(LFlags<LProcessEvents>  flags= LProcessEvents::All);
    static void deferredFlushWinSysEvents(LFlags<LProcessEvents>  flags= LProcessEvents::All);

    static std::shared_ptr<LWinSysEvent> peekWindowSystemEvent(LWinSysEvent::Type t);
    static void removeWindowSystemEvent(std::shared_ptr<LWinSysEvent> event);

    static bool m_isDefaultSyncWinSysEvents;
protected:
    static LWinSysEventList m_winSysEventQueue;
    static LElapsedTimer m_eventTime;

    static std::condition_variable m_eventsFlushed;
    static std::mutex m_flushEventMutex;
};


LK_NAMESPACE_END
#endif // LWINDOWSYSTEMINTERFACE_H
