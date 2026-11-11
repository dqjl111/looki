#ifndef LWINDOWSKEYMAPPER_H
#define LWINDOWSKEYMAPPER_H
#include "lwindows.h"
#include "lkgui/lwindow.h"

LK_NAMESPACE_BEGIN
class LWindowsKeyMapper
{
public:
    LWindowsKeyMapper();

    enum WindowsNativeModifiers {
        ShiftLeft            = 0x00000001,
        ControlLeft          = 0x00000002,
        AltLeft              = 0x00000004,
        MetaLeft             = 0x00000008,
        ShiftRight           = 0x00000010,
        ControlRight         = 0x00000020,
        AltRight             = 0x00000040,
        MetaRight            = 0x00000080,
        CapsLock             = 0x00000100,
        NumLock              = 0x00000200,
        ScrollLock           = 0x00000400,
        ExtendedKey          = 0x01000000,

        // Convenience mappings
        ShiftAny             = 0x00000011,
        ControlAny           = 0x00000022,
        AltAny               = 0x00000044,
        MetaAny              = 0x00000088,
        LockAny              = 0x00000700
    };

    struct KeyboardLayoutItem {
        uint32_t dirty : 1;
        uint32_t exists : 1; // whether this item has been initialized (by updatePossibleKeyCodes)
        uint8_t deadkeys;
        static const size_t NumLkKeys = 9;
        uint32_t lkKey[NumLkKeys]; // Can by any Lk::Key_<foo>, or unicode character
    };

    bool translateKeyEvent(std::shared_ptr<LWindow> widget, HWND hwnd, const MSG &msg, LRESULT *result);
    void changeKeyboard();

    inline std::shared_ptr<LWindow> keyGrabber() const      { return m_keyGrabber.lock(); }
    inline void setKeyGrabber(std::shared_ptr<LWindow> w)   { m_keyGrabber = w; }

    static LFlags<LKeyModifier> queryKeyboardModifiers();
    static void clearKeyRecorderOnApplicationInActive(LApplicationState state);

    std::list<uint32_t> possibleKeys(const std::shared_ptr<LKeyEvent> e) const;
private:
    bool translateKeyEventInternal(std::shared_ptr<LWindow> receiver, MSG msg, bool grab, LRESULT *lResult);
    bool translateMultimediaKeyEventInternal(std::shared_ptr<LWindow> receiver, const MSG &msg);

    void deleteLayouts();
    void updateKeyMap(const MSG &msg);

    std::weak_ptr<LWindow> m_keyGrabber;
    bool m_detectAltGrModifier =  false;
    bool m_seenAltGr = false;
    bool m_useRTLExtensions = false;
    wchar_t m_lastHighSurrogate;

    static const size_t m_numKeyboardLayoutItems = 256;
    KeyboardLayoutItem m_keyLayout[m_numKeyboardLayoutItems];
};

LK_NAMESPACE_END
#endif // LWINDOWSKEYMAPPER_H
