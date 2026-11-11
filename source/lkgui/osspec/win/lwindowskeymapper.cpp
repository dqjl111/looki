#include "lwindowskeymapper.h"
#include "lkcore/lcoreenums.h"
#include "lkgui/lwindowsysteminterface.h"
#include "lkcore/lstringutil.h"
#include "lkgui/lguiapplication.h"
#include "lwindowsctx.h"
#include "lwindowsinputctx.h"
#include "lwindowsintegration.h"


LK_NAMESPACE_BEGIN

// Key recorder ------------------------------------------------------------------------[ start ] --
struct LWinKeyRecord {
    LWinKeyRecord(int c, int a, int s, const std::wstring &t) : code(c), ascii(a), state(s), text(t) {}
    LWinKeyRecord() {}

    int code = 0;
    int ascii = 0;
    uint32_t state = 0;
    std::wstring text;
};

// We need to record the pressed keys in order to decide, whether the key event is an autorepeat
// event. As soon as its state changes, the chain of autorepeat events will be broken.
static const int LK_MAX_KEY_RECORDINGS = 64; // User has LOTS of fingers...
struct LWinKeyRecorder
{
    inline LWinKeyRecord *findKey(int code, bool remove);
    inline void storeKey(int code, int ascii, int state, const std::wstring& text);
    inline void clearKeys();

    int nrecs = 0;
    LWinKeyRecord deletedRecord; // A copy of last entry removed from records[]
    LWinKeyRecord records[LK_MAX_KEY_RECORDINGS];

    static LWinKeyRecorder& inst(){
        static LWinKeyRecorder t;
        return t;
    }
};


LWinKeyRecord *LWinKeyRecorder::findKey(int code, bool remove)
{
    LWinKeyRecord *result = nullptr;
    for (int i = 0; i < nrecs; ++i) {
        if (records[i].code == code) {
            if (remove) {
                deletedRecord = records[i];
                // Move rest down, and decrease count
                while (i + 1 < nrecs) {
                    records[i] = records[i + 1];
                    ++i;
                }
                --nrecs;
                result = &deletedRecord;
            } else {
                result = &records[i];
            }
            break;
        }
    }
    return result;
}

void LWinKeyRecorder::storeKey(int code, int ascii, int state, const std::wstring& text)
{

    if (nrecs >= LK_MAX_KEY_RECORDINGS) {
        return;
    }
    records[nrecs++] = LWinKeyRecord(code,ascii,state,text);
}

void LWinKeyRecorder::clearKeys()
{
    nrecs = 0;
}



enum { scancodeBitmask = 0x1ff };


// Key translation ---------------------------------------------------------------------[ start ] --
// Meaning of values:
//             0 = Character output key, needs keyboard driver mapping
//   Key_unknown = Unknown Virtual Key, no translation possible, ignore
static const uint32_t KeyTbl[] = { // Keyboard mapping table
    // Dec |  Hex | Windows Virtual key
    (uint32_t)LKey::Unknown,    //   0   0x00
    (uint32_t)LKey::Unknown,    //   1   0x01   VK_LBUTTON          | Left mouse button
    (uint32_t)LKey::Unknown,    //   2   0x02   VK_RBUTTON          | Right mouse button
    (uint32_t)LKey::Cancel,     //   3   0x03   VK_CANCEL           | Control-Break processing
    (uint32_t)LKey::Unknown,    //   4   0x04   VK_MBUTTON          | Middle mouse button
    (uint32_t)LKey::Unknown,    //   5   0x05   VK_XBUTTON1         | X1 mouse button
    (uint32_t)LKey::Unknown,    //   6   0x06   VK_XBUTTON2         | X2 mouse button
    (uint32_t)LKey::Unknown,    //   7   0x07   -- unassigned --
    (uint32_t)LKey::Backspace,  //   8   0x08   VK_BACK             | BackSpace key
    (uint32_t)LKey::Tab,        //   9   0x09   VK_TAB              | Tab key
    (uint32_t)LKey::Unknown,    //  10   0x0A   -- reserved --
    (uint32_t)LKey::Unknown,    //  11   0x0B   -- reserved --
    (uint32_t)LKey::Clear,      //  12   0x0C   VK_CLEAR            | Clear key
    (uint32_t)LKey::Return,     //  13   0x0D   VK_RETURN           | Enter key
    (uint32_t)LKey::Unknown,    //  14   0x0E   -- unassigned --
    (uint32_t)LKey::Unknown,    //  15   0x0F   -- unassigned --
    (uint32_t)LKey::Shift,      //  16   0x10   VK_SHIFT            | Shift key
    (uint32_t)LKey::Control,    //  17   0x11   VK_CONTROL          | Ctrl key
    (uint32_t)LKey::Alt,        //  18   0x12   VK_MENU             | Alt key
    (uint32_t)LKey::Pause,      //  19   0x13   VK_PAUSE            | Pause key
    (uint32_t)LKey::CapsLock,   //  20   0x14   VK_CAPITAL          | Caps-Lock
    (uint32_t)LKey::Unknown,    //  21   0x15   VK_KANA / VK_HANGUL | IME Kana or Hangul mode
    (uint32_t)LKey::Unknown,    //  22   0x16   -- unassigned --
    (uint32_t)LKey::Unknown,    //  23   0x17   VK_JUNJA            | IME Junja mode
    (uint32_t)LKey::Unknown,    //  24   0x18   VK_FINAL            | IME final mode
    (uint32_t)LKey::Unknown,    //  25   0x19   VK_HANJA / VK_KANJI | IME Hanja or Kanji mode
    (uint32_t)LKey::Unknown,    //  26   0x1A   -- unassigned --
    (uint32_t)LKey::Escape,     //  27   0x1B   VK_ESCAPE           | Esc key
    (uint32_t)LKey::Unknown,    //  28   0x1C   VK_CONVERT          | IME convert
    (uint32_t)LKey::Unknown,    //  29   0x1D   VK_NONCONVERT       | IME non-convert
    (uint32_t)LKey::Unknown,    //  30   0x1E   VK_ACCEPT           | IME accept
    (uint32_t)LKey::Mode_switch,//  31   0x1F   VK_MODECHANGE       | IME mode change request
    (uint32_t)LKey::Space,      //  32   0x20   VK_SPACE            | Spacebar
    (uint32_t)LKey::PageUp,     //  33   0x21   VK_PRIOR            | Page Up key
    (uint32_t)LKey::PageDown,   //  34   0x22   VK_NEXT             | Page Down key
    (uint32_t)LKey::End,        //  35   0x23   VK_END              | End key
    (uint32_t)LKey::Home,       //  36   0x24   VK_HOME             | Home key
    (uint32_t)LKey::Left,       //  37   0x25   VK_LEFT             | Left arrow key
    (uint32_t)LKey::Up,         //  38   0x26   VK_UP               | Up arrow key
    (uint32_t)LKey::Right,      //  39   0x27   VK_RIGHT            | Right arrow key
    (uint32_t)LKey::Down,       //  40   0x28   VK_DOWN             | Down arrow key
    (uint32_t)LKey::Select,     //  41   0x29   VK_SELECT           | Select key
    (uint32_t)LKey::Printer,    //  42   0x2A   VK_PRINT            | Print key
    (uint32_t)LKey::Execute,    //  43   0x2B   VK_EXECUTE          | Execute key
    (uint32_t)LKey::Print,      //  44   0x2C   VK_SNAPSHOT         | Print Screen key
    (uint32_t)LKey::Insert,     //  45   0x2D   VK_INSERT           | Ins key
    (uint32_t)LKey::Delete,     //  46   0x2E   VK_DELETE           | Del key
    (uint32_t)LKey::Help,       //  47   0x2F   VK_HELP             | Help key
    0,                  //  48   0x30   (VK_0)              | 0 key
    0,                  //  49   0x31   (VK_1)              | 1 key
    0,                  //  50   0x32   (VK_2)              | 2 key
    0,                  //  51   0x33   (VK_3)              | 3 key
    0,                  //  52   0x34   (VK_4)              | 4 key
    0,                  //  53   0x35   (VK_5)              | 5 key
    0,                  //  54   0x36   (VK_6)              | 6 key
    0,                  //  55   0x37   (VK_7)              | 7 key
    0,                  //  56   0x38   (VK_8)              | 8 key
    0,                  //  57   0x39   (VK_9)              | 9 key
    (uint32_t)LKey::Unknown,    //  58   0x3A   -- unassigned --
    (uint32_t)LKey::Unknown,    //  59   0x3B   -- unassigned --
    (uint32_t)LKey::Unknown,    //  60   0x3C   -- unassigned --
    (uint32_t)LKey::Unknown,    //  61   0x3D   -- unassigned --
    (uint32_t)LKey::Unknown,    //  62   0x3E   -- unassigned --
    (uint32_t)LKey::Unknown,    //  63   0x3F   -- unassigned --
    (uint32_t)LKey::Unknown,    //  64   0x40   -- unassigned --
    0,                  //  65   0x41   (VK_A)              | A key
    0,                  //  66   0x42   (VK_B)              | B key
    0,                  //  67   0x43   (VK_C)              | C key
    0,                  //  68   0x44   (VK_D)              | D key
    0,                  //  69   0x45   (VK_E)              | E key
    0,                  //  70   0x46   (VK_F)              | F key
    0,                  //  71   0x47   (VK_G)              | G key
    0,                  //  72   0x48   (VK_H)              | H key
    0,                  //  73   0x49   (VK_I)              | I key
    0,                  //  74   0x4A   (VK_J)              | J key
    0,                  //  75   0x4B   (VK_K)              | K key
    0,                  //  76   0x4C   (VK_L)              | L key
    0,                  //  77   0x4D   (VK_M)              | M key
    0,                  //  78   0x4E   (VK_N)              | N key
    0,                  //  79   0x4F   (VK_O)              | O key
    0,                  //  80   0x50   (VK_P)              | P key
    0,                  //  81   0x51   (VK_Q)              | Q key
    0,                  //  82   0x52   (VK_R)              | R key
    0,                  //  83   0x53   (VK_S)              | S key
    0,                  //  84   0x54   (VK_T)              | T key
    0,                  //  85   0x55   (VK_U)              | U key
    0,                  //  86   0x56   (VK_V)              | V key
    0,                  //  87   0x57   (VK_W)              | W key
    0,                  //  88   0x58   (VK_X)              | X key
    0,                  //  89   0x59   (VK_Y)              | Y key
    0,                  //  90   0x5A   (VK_Z)              | Z key
    (uint32_t)LKey::Meta,       //  91   0x5B   VK_LWIN             | Left Windows  - MS Natural kbd
    (uint32_t)LKey::Meta,       //  92   0x5C   VK_RWIN             | Right Windows - MS Natural kbd
    (uint32_t)LKey::Menu,       //  93   0x5D   VK_APPS             | Application key-MS Natural kbd
    (uint32_t)LKey::Unknown,    //  94   0x5E   -- reserved --
    (uint32_t)LKey::Sleep,      //  95   0x5F   VK_SLEEP
    (uint32_t)LKey::Num0,          //  96   0x60   VK_NUMPAD0          | Numeric keypad 0 key
    (uint32_t)LKey::Num1,          //  97   0x61   VK_NUMPAD1          | Numeric keypad 1 key
    (uint32_t)LKey::Num2,          //  98   0x62   VK_NUMPAD2          | Numeric keypad 2 key
    (uint32_t)LKey::Num3,          //  99   0x63   VK_NUMPAD3          | Numeric keypad 3 key
    (uint32_t)LKey::Num4,          // 100   0x64   VK_NUMPAD4          | Numeric keypad 4 key
    (uint32_t)LKey::Num5,          // 101   0x65   VK_NUMPAD5          | Numeric keypad 5 key
    (uint32_t)LKey::Num6,          // 102   0x66   VK_NUMPAD6          | Numeric keypad 6 key
    (uint32_t)LKey::Num7,          // 103   0x67   VK_NUMPAD7          | Numeric keypad 7 key
    (uint32_t)LKey::Num8,          // 104   0x68   VK_NUMPAD8          | Numeric keypad 8 key
    (uint32_t)LKey::Num9,          // 105   0x69   VK_NUMPAD9          | Numeric keypad 9 key
    (uint32_t)LKey::Asterisk,   // 106   0x6A   VK_MULTIPLY         | Multiply key
    (uint32_t)LKey::Plus,       // 107   0x6B   VK_ADD              | Add key
    (uint32_t)LKey::Unknown,    // 108   0x6C   VK_SEPARATOR        | Separator key (locale-dependent)
    (uint32_t)LKey::Minus,      // 109   0x6D   VK_SUBTRACT         | Subtract key
    (uint32_t)LKey::Unknown,    // 110   0x6E   VK_DECIMAL          | Decimal key (locale-dependent)
    (uint32_t)LKey::Slash,      // 111   0x6F   VK_DIVIDE           | Divide key
    (uint32_t)LKey::F1,         // 112   0x70   VK_F1               | F1 key
    (uint32_t)LKey::F2,         // 113   0x71   VK_F2               | F2 key
    (uint32_t)LKey::F3,         // 114   0x72   VK_F3               | F3 key
    (uint32_t)LKey::F4,         // 115   0x73   VK_F4               | F4 key
    (uint32_t)LKey::F5,         // 116   0x74   VK_F5               | F5 key
    (uint32_t)LKey::F6,         // 117   0x75   VK_F6               | F6 key
    (uint32_t)LKey::F7,         // 118   0x76   VK_F7               | F7 key
    (uint32_t)LKey::F8,         // 119   0x77   VK_F8               | F8 key
    (uint32_t)LKey::F9,         // 120   0x78   VK_F9               | F9 key
    (uint32_t)LKey::F10,        // 121   0x79   VK_F10              | F10 key
    (uint32_t)LKey::F11,        // 122   0x7A   VK_F11              | F11 key
    (uint32_t)LKey::F12,        // 123   0x7B   VK_F12              | F12 key
    (uint32_t)LKey::F13,        // 124   0x7C   VK_F13              | F13 key
    (uint32_t)LKey::F14,        // 125   0x7D   VK_F14              | F14 key
    (uint32_t)LKey::F15,        // 126   0x7E   VK_F15              | F15 key
    (uint32_t)LKey::F16,        // 127   0x7F   VK_F16              | F16 key
    (uint32_t)LKey::F17,        // 128   0x80   VK_F17              | F17 key
    (uint32_t)LKey::F18,        // 129   0x81   VK_F18              | F18 key
    (uint32_t)LKey::F19,        // 130   0x82   VK_F19              | F19 key
    (uint32_t)LKey::F20,        // 131   0x83   VK_F20              | F20 key
    (uint32_t)LKey::F21,        // 132   0x84   VK_F21              | F21 key
    (uint32_t)LKey::F22,        // 133   0x85   VK_F22              | F22 key
    (uint32_t)LKey::F23,        // 134   0x86   VK_F23              | F23 key
    (uint32_t)LKey::F24,        // 135   0x87   VK_F24              | F24 key
    (uint32_t)LKey::Unknown,    // 136   0x88   -- unassigned --
    (uint32_t)LKey::Unknown,    // 137   0x89   -- unassigned --
    (uint32_t)LKey::Unknown,    // 138   0x8A   -- unassigned --
    (uint32_t)LKey::Unknown,    // 139   0x8B   -- unassigned --
    (uint32_t)LKey::Unknown,    // 140   0x8C   -- unassigned --
    (uint32_t)LKey::Unknown,    // 141   0x8D   -- unassigned --
    (uint32_t)LKey::Unknown,    // 142   0x8E   -- unassigned --
    (uint32_t)LKey::Unknown,    // 143   0x8F   -- unassigned --
    (uint32_t)LKey::NumLock,    // 144   0x90   VK_NUMLOCK          | Num Lock key
    (uint32_t)LKey::ScrollLock, // 145   0x91   VK_SCROLL           | Scroll Lock key
    // Fujitsu/OASYS kbd --------------------
    0, //(uint32_t)LKey::Jisho, // 146   0x92   VK_OEM_FJ_JISHO     | 'Dictionary' key /
    //              VK_OEM_NEC_EQUAL  = key on numpad on NEC PC-9800 kbd
    (uint32_t)LKey::Massyo,     // 147   0x93   VK_OEM_FJ_MASSHOU   | 'Unregister word' key
    (uint32_t)LKey::Touroku,    // 148   0x94   VK_OEM_FJ_TOUROKU   | 'Register word' key
    0, //(uint32_t)LKey::Oyayubi_Left,//149   0x95  VK_OEM_FJ_LOYA  | 'Left OYAYUBI' key
    0, //(uint32_t)LKey::Oyayubi_Right,//150  0x96  VK_OEM_FJ_ROYA  | 'Right OYAYUBI' key
    (uint32_t)LKey::Unknown,    // 151   0x97   -- unassigned --
    (uint32_t)LKey::Unknown,    // 152   0x98   -- unassigned --
    (uint32_t)LKey::Unknown,    // 153   0x99   -- unassigned --
    (uint32_t)LKey::Unknown,    // 154   0x9A   -- unassigned --
    (uint32_t)LKey::Unknown,    // 155   0x9B   -- unassigned --
    (uint32_t)LKey::Unknown,    // 156   0x9C   -- unassigned --
    (uint32_t)LKey::Unknown,    // 157   0x9D   -- unassigned --
    (uint32_t)LKey::Unknown,    // 158   0x9E   -- unassigned --
    (uint32_t)LKey::Unknown,    // 159   0x9F   -- unassigned --
    (uint32_t)LKey::Shift,      // 160   0xA0   VK_LSHIFT           | Left Shift key
    (uint32_t)LKey::Shift,      // 161   0xA1   VK_RSHIFT           | Right Shift key
    (uint32_t)LKey::Control,    // 162   0xA2   VK_LCONTROL         | Left Ctrl key
    (uint32_t)LKey::Control,    // 163   0xA3   VK_RCONTROL         | Right Ctrl key
    (uint32_t)LKey::Alt,        // 164   0xA4   VK_LMENU            | Left Menu key
    (uint32_t)LKey::Alt,        // 165   0xA5   VK_RMENU            | Right Menu key
    (uint32_t)LKey::Back,       // 166   0xA6   VK_BROWSER_BACK     | Browser Back key
    (uint32_t)LKey::Forward,    // 167   0xA7   VK_BROWSER_FORWARD  | Browser Forward key
    (uint32_t)LKey::Refresh,    // 168   0xA8   VK_BROWSER_REFRESH  | Browser Refresh key
    (uint32_t)LKey::Stop,       // 169   0xA9   VK_BROWSER_STOP     | Browser Stop key
    (uint32_t)LKey::Search,     // 170   0xAA   VK_BROWSER_SEARCH   | Browser Search key
    (uint32_t)LKey::Favorites,  // 171   0xAB   VK_BROWSER_FAVORITES| Browser Favorites key
    (uint32_t)LKey::HomePage,   // 172   0xAC   VK_BROWSER_HOME     | Browser Start and Home key
    (uint32_t)LKey::VolumeMute, // 173   0xAD   VK_VOLUME_MUTE      | Volume Mute key
    (uint32_t)LKey::VolumeDown, // 174   0xAE   VK_VOLUME_DOWN      | Volume Down key
    (uint32_t)LKey::VolumeUp,   // 175   0xAF   VK_VOLUME_UP        | Volume Up key
    (uint32_t)LKey::MediaNext,  // 176   0xB0   VK_MEDIA_NEXT_TRACK | Next Track key
    (uint32_t)LKey::MediaPrevious, //177 0xB1   VK_MEDIA_PREV_TRACK | Previous Track key
    (uint32_t)LKey::MediaStop,  // 178   0xB2   VK_MEDIA_STOP       | Stop Media key
    (uint32_t)LKey::MediaPlay,  // 179   0xB3   VK_MEDIA_PLAY_PAUSE | Play/Pause Media key
    (uint32_t)LKey::LaunchMail, // 180   0xB4   VK_LAUNCH_MAIL      | Start Mail key
    (uint32_t)LKey::LaunchMedia,// 181   0xB5   VK_LAUNCH_MEDIA_SELECT Select Media key
    (uint32_t)LKey::Launch0,    // 182   0xB6   VK_LAUNCH_APP1      | Start Application 1 key
    (uint32_t)LKey::Launch1,    // 183   0xB7   VK_LAUNCH_APP2      | Start Application 2 key
    (uint32_t)LKey::Unknown,    // 184   0xB8   -- reserved --
    (uint32_t)LKey::Unknown,    // 185   0xB9   -- reserved --
    0,                  // 186   0xBA   VK_OEM_1            | ';:' for US
    0,                  // 187   0xBB   VK_OEM_PLUS         | '+' any country
    0,                  // 188   0xBC   VK_OEM_COMMA        | ',' any country
    0,                  // 189   0xBD   VK_OEM_MINUS        | '-' any country
    0,                  // 190   0xBE   VK_OEM_PERIOD       | '.' any country
    0,                  // 191   0xBF   VK_OEM_2            | '/?' for US
    0,                  // 192   0xC0   VK_OEM_3            | '`~' for US
    (uint32_t)LKey::Unknown,    // 193   0xC1   -- reserved --
    (uint32_t)LKey::Unknown,    // 194   0xC2   -- reserved --
    (uint32_t)LKey::Unknown,    // 195   0xC3   -- reserved --
    (uint32_t)LKey::Unknown,    // 196   0xC4   -- reserved --
    (uint32_t)LKey::Unknown,    // 197   0xC5   -- reserved --
    (uint32_t)LKey::Unknown,    // 198   0xC6   -- reserved --
    (uint32_t)LKey::Unknown,    // 199   0xC7   -- reserved --
    (uint32_t)LKey::Unknown,    // 200   0xC8   -- reserved --
    (uint32_t)LKey::Unknown,    // 201   0xC9   -- reserved --
    (uint32_t)LKey::Unknown,    // 202   0xCA   -- reserved --
    (uint32_t)LKey::Unknown,    // 203   0xCB   -- reserved --
    (uint32_t)LKey::Unknown,    // 204   0xCC   -- reserved --
    (uint32_t)LKey::Unknown,    // 205   0xCD   -- reserved --
    (uint32_t)LKey::Unknown,    // 206   0xCE   -- reserved --
    (uint32_t)LKey::Unknown,    // 207   0xCF   -- reserved --
    (uint32_t)LKey::Unknown,    // 208   0xD0   -- reserved --
    (uint32_t)LKey::Unknown,    // 209   0xD1   -- reserved --
    (uint32_t)LKey::Unknown,    // 210   0xD2   -- reserved --
    (uint32_t)LKey::Unknown,    // 211   0xD3   -- reserved --
    (uint32_t)LKey::Unknown,    // 212   0xD4   -- reserved --
    (uint32_t)LKey::Unknown,    // 213   0xD5   -- reserved --
    (uint32_t)LKey::Unknown,    // 214   0xD6   -- reserved --
    (uint32_t)LKey::Unknown,    // 215   0xD7   -- reserved --
    (uint32_t)LKey::Unknown,    // 216   0xD8   -- unassigned --
    (uint32_t)LKey::Unknown,    // 217   0xD9   -- unassigned --
    (uint32_t)LKey::Unknown,    // 218   0xDA   -- unassigned --
    0,                  // 219   0xDB   VK_OEM_4            | '[{' for US
    0,                  // 220   0xDC   VK_OEM_5            | '\|' for US
    0,                  // 221   0xDD   VK_OEM_6            | ']}' for US
    0,                  // 222   0xDE   VK_OEM_7            | ''"' for US
    0,                  // 223   0xDF   VK_OEM_8
    (uint32_t)LKey::Unknown,    // 224   0xE0   -- reserved --
    (uint32_t)LKey::Unknown,    // 225   0xE1   VK_OEM_AX           | 'AX' key on Japanese AX kbd
    (uint32_t)LKey::Unknown,    // 226   0xE2   VK_OEM_102          | "<>" or "\|" on RT 102-key kbd
    (uint32_t)LKey::Unknown,    // 227   0xE3   VK_ICO_HELP         | Help key on ICO
    (uint32_t)LKey::Unknown,    // 228   0xE4   VK_ICO_00           | 00 key on ICO
    (uint32_t)LKey::Unknown,    // 229   0xE5   VK_PROCESSKEY       | IME Process key
    (uint32_t)LKey::Unknown,    // 230   0xE6   VK_ICO_CLEAR        |
    (uint32_t)LKey::Unknown,    // 231   0xE7   VK_PACKET           | Unicode char as keystrokes
    (uint32_t)LKey::Unknown,    // 232   0xE8   -- unassigned --
    // Nokia/Ericsson definitions ---------------
    (uint32_t)LKey::Unknown,    // 233   0xE9   VK_OEM_RESET
    (uint32_t)LKey::Unknown,    // 234   0xEA   VK_OEM_JUMP
    (uint32_t)LKey::Unknown,    // 235   0xEB   VK_OEM_PA1
    (uint32_t)LKey::Unknown,    // 236   0xEC   VK_OEM_PA2
    (uint32_t)LKey::Unknown,    // 237   0xED   VK_OEM_PA3
    (uint32_t)LKey::Unknown,    // 238   0xEE   VK_OEM_WSCTRL
    (uint32_t)LKey::Unknown,    // 239   0xEF   VK_OEM_CUSEL
    (uint32_t)LKey::Unknown,    // 240   0xF0   VK_OEM_ATTN
    (uint32_t)LKey::Unknown,    // 241   0xF1   VK_OEM_FINISH
    (uint32_t)LKey::Unknown,    // 242   0xF2   VK_OEM_COPY
    (uint32_t)LKey::Unknown,    // 243   0xF3   VK_OEM_AUTO
    (uint32_t)LKey::Unknown,    // 244   0xF4   VK_OEM_ENLW
    (uint32_t)LKey::Unknown,    // 245   0xF5   VK_OEM_BACKTAB
    (uint32_t)LKey::Unknown,    // 246   0xF6   VK_ATTN             | Attn key
    (uint32_t)LKey::Unknown,    // 247   0xF7   VK_CRSEL            | CrSel key
    (uint32_t)LKey::Unknown,    // 248   0xF8   VK_EXSEL            | ExSel key
    (uint32_t)LKey::Unknown,    // 249   0xF9   VK_EREOF            | Erase EOF key
    (uint32_t)LKey::Play,       // 250   0xFA   VK_PLAY             | Play key
    (uint32_t)LKey::Zoom,       // 251   0xFB   VK_ZOOM             | Zoom key
    (uint32_t)LKey::Unknown,    // 252   0xFC   VK_NONAME           | Reserved
    (uint32_t)LKey::Unknown,    // 253   0xFD   VK_PA1              | PA1 key
    (uint32_t)LKey::Clear,      // 254   0xFE   VK_OEM_CLEAR        | Clear key
    0
};

static const uint32_t CmdTbl[] = { // Multimedia keys mapping table
    // Dec |  Hex | AppCommand
    (uint32_t)LKey::Unknown,        //   0   0x00
    (uint32_t)LKey::Back,           //   1   0x01   APPCOMMAND_BROWSER_BACKWARD
    (uint32_t)LKey::Forward,        //   2   0x02   APPCOMMAND_BROWSER_FORWARD
    (uint32_t)LKey::Refresh,        //   3   0x03   APPCOMMAND_BROWSER_REFRESH
    (uint32_t)LKey::Stop,           //   4   0x04   APPCOMMAND_BROWSER_STOP
    (uint32_t)LKey::Search,         //   5   0x05   APPCOMMAND_BROWSER_SEARCH
    (uint32_t)LKey::Favorites,      //   6   0x06   APPCOMMAND_BROWSER_FAVORITES
    (uint32_t)LKey::Home,           //   7   0x07   APPCOMMAND_BROWSER_HOME
    (uint32_t)LKey::VolumeMute,     //   8   0x08   APPCOMMAND_VOLUME_MUTE
    (uint32_t)LKey::VolumeDown,     //   9   0x09   APPCOMMAND_VOLUME_DOWN
    (uint32_t)LKey::VolumeUp,       //  10   0x0a   APPCOMMAND_VOLUME_UP
    (uint32_t)LKey::MediaNext,      //  11   0x0b   APPCOMMAND_MEDIA_NEXTTRACK
    (uint32_t)LKey::MediaPrevious,  //  12   0x0c   APPCOMMAND_MEDIA_PREVIOUSTRACK
    (uint32_t)LKey::MediaStop,      //  13   0x0d   APPCOMMAND_MEDIA_STOP
    (uint32_t)LKey::MediaTogglePlayPause,   //  14   0x0e   APPCOMMAND_MEDIA_PLAYPAUSE
    (uint32_t)LKey::LaunchMail,     //  15   0x0f   APPCOMMAND_LAUNCH_MAIL
    (uint32_t)LKey::LaunchMedia,    //  16   0x10   APPCOMMAND_LAUNCH_MEDIA_SELECT
    (uint32_t)LKey::Launch0,        //  17   0x11   APPCOMMAND_LAUNCH_APP1
    (uint32_t)LKey::Launch1,        //  18   0x12   APPCOMMAND_LAUNCH_APP2
    (uint32_t)LKey::BassDown,       //  19   0x13   APPCOMMAND_BASS_DOWN
    (uint32_t)LKey::BassBoost,      //  20   0x14   APPCOMMAND_BASS_BOOST
    (uint32_t)LKey::BassUp,         //  21   0x15   APPCOMMAND_BASS_UP
    (uint32_t)LKey::TrebleDown,     //  22   0x16   APPCOMMAND_TREBLE_DOWN
    (uint32_t)LKey::TrebleUp,       //  23   0x17   APPCOMMAND_TREBLE_UP
    (uint32_t)LKey::MicMute,        //  24   0x18   APPCOMMAND_MICROPHONE_VOLUME_MUTE
    (uint32_t)LKey::MicVolumeDown,  //  25   0x19   APPCOMMAND_MICROPHONE_VOLUME_DOWN
    (uint32_t)LKey::MicVolumeUp,    //  26   0x1a   APPCOMMAND_MICROPHONE_VOLUME_UP
    (uint32_t)LKey::Help,           //  27   0x1b   APPCOMMAND_HELP
    (uint32_t)LKey::Find,           //  28   0x1c   APPCOMMAND_FIND
    (uint32_t)LKey::New,            //  29   0x1d   APPCOMMAND_NEW
    (uint32_t)LKey::Open,           //  30   0x1e   APPCOMMAND_OPEN
    (uint32_t)LKey::Close,          //  31   0x1f   APPCOMMAND_CLOSE
    (uint32_t)LKey::Save,           //  32   0x20   APPCOMMAND_SAVE
    (uint32_t)LKey::Printer,        //  33   0x21   APPCOMMAND_PRINT
    (uint32_t)LKey::Undo,           //  34   0x22   APPCOMMAND_UNDO
    (uint32_t)LKey::Redo,           //  35   0x23   APPCOMMAND_REDO
    (uint32_t)LKey::Copy,           //  36   0x24   APPCOMMAND_COPY
    (uint32_t)LKey::Cut,            //  37   0x25   APPCOMMAND_CUT
    (uint32_t)LKey::Paste,          //  38   0x26   APPCOMMAND_PASTE
    (uint32_t)LKey::Reply,          //  39   0x27   APPCOMMAND_REPLY_TO_MAIL
    (uint32_t)LKey::MailForward,    //  40   0x28   APPCOMMAND_FORWARD_MAIL
    (uint32_t)LKey::Send,           //  41   0x29   APPCOMMAND_SEND_MAIL
    (uint32_t)LKey::Spell,          //  42   0x2a   APPCOMMAND_SPELL_CHECK
    (uint32_t)LKey::Unknown,        //  43   0x2b   APPCOMMAND_DICTATE_OR_COMMAND_CONTROL_TOGGLE
    (uint32_t)LKey::Unknown,        //  44   0x2c   APPCOMMAND_MIC_ON_OFF_TOGGLE
    (uint32_t)LKey::Unknown,        //  45   0x2d   APPCOMMAND_CORRECTION_LIST
    (uint32_t)LKey::MediaPlay,      //  46   0x2e   APPCOMMAND_MEDIA_PLAY
    (uint32_t)LKey::MediaPause,     //  47   0x2f   APPCOMMAND_MEDIA_PAUSE
    (uint32_t)LKey::MediaRecord,    //  48   0x30   APPCOMMAND_MEDIA_RECORD
    (uint32_t)LKey::AudioForward,   //  49   0x31   APPCOMMAND_MEDIA_FAST_FORWARD
    (uint32_t)LKey::AudioRewind,    //  50   0x32   APPCOMMAND_MEDIA_REWIND
    (uint32_t)LKey::ChannelDown,    //  51   0x33   APPCOMMAND_MEDIA_CHANNEL_DOWN
    (uint32_t)LKey::ChannelUp       //  52   0x34   APPCOMMAND_MEDIA_CHANNEL_UP
};

// Possible modifier states.
static const LFlags<LKeyModifier> ModsTbl[] = {
    LKeyModifier::No,                                             // 0
    LKeyModifier::Shift,                                          // 1
    LKeyModifier::Ctrl,                                        // 2
    LKeyModifier::Ctrl | LKeyModifier::Shift,                    // 3
    LKeyModifier::Alt,                                            // 4
    LKeyModifier::Alt | LKeyModifier::Shift,                        // 5
    LKeyModifier::Alt | LKeyModifier::Ctrl,                      // 6
    LKeyModifier::Alt | LKeyModifier::Shift | LKeyModifier::Ctrl,  // 7
    LKeyModifier::No,                                             // Fall-back to raw Key_*
};
static const size_t NumMods = sizeof(ModsTbl) / sizeof(*ModsTbl);


inline uint32_t winceKeyBend(uint32_t keyCode)
{
    return KeyTbl[keyCode];
}

//Check for AltGr found on some keyboards
// which is a sequence of left Ctrl (SYSKEY) + right Menu (Alt).
static bool isAltGr(MSG *msg)
{
    enum : LONG_PTR { RightFlag = 0x1000000 };
    if (msg->wParam != VK_CONTROL || (msg->lParam & RightFlag) != 0
        || (msg->message != WM_KEYDOWN && msg->message != WM_SYSKEYUP)) {
        return false;
    }
    const UINT expectedMessage = msg->message == WM_SYSKEYUP
                                     ? WM_KEYUP : msg->message;
    MSG peekedMsg;
    if (PeekMessage(&peekedMsg, msg->hwnd, 0, 0, PM_NOREMOVE) == FALSE
        || peekedMsg.message != expectedMessage || peekedMsg.wParam != VK_MENU
        || (peekedMsg.lParam & RightFlag) == 0) {
        return false;
    }
    *msg = peekedMsg;
    PeekMessage(&peekedMsg, msg->hwnd, 0, 0, PM_REMOVE);
    return true;
}

static inline void sendExtendedPressRelease(std::shared_ptr<LWindow> w, int k,
                                            LFlags<LKeyModifier> mods,
                                            uint32_t nativeScanCode,
                                            uint32_t nativeVirtualKey,
                                            uint32_t nativeModifiers,
                                            const std::wstring & text = std::wstring(),
                                            bool autorep = false,
                                            uint16_t count = 1)
{
    LWindowSystemInterface::handleExtendedKeyEvent(w, LEvent::KeyPress, k, mods, nativeScanCode, nativeVirtualKey, nativeModifiers, text, autorep, count);
    LWindowSystemInterface::handleExtendedKeyEvent(w, LEvent::KeyRelease, k, mods, nativeScanCode, nativeVirtualKey, nativeModifiers, text, autorep, count);
}


static inline std::wstring messageKeyText(const MSG &msg)
{
    std::wstring ch;
    ch += wchar_t(msg.wParam);
    return ch;
}

static inline int asciiToKeycode(char a, LFlags<LKeyModifier> state)
{
    if (a >= 'a' && a <= 'z'){
        a = toupper(a);
    }
    if (uint32_t(state & LKeyModifier::Ctrl) != 0) {
        if (a >= 0 && a <= 31){              // Ctrl+@..Ctrl+A..CTRL+Z..Ctrl+_
            a += '@';                       // to @..A..Z.._
        }
    }
    return a & 0xff;
}

// Translate a VK into a LK key code, or unicode character
static inline uint32_t toKeyOrUnicode(uint32_t vk, uint32_t scancode, unsigned char *kbdBuffer, bool *isDeadkey = nullptr)
{
    uint32_t code = 0;
    wchar_t unicodeBuffer[5];
    int res = ToUnicode(vk, scancode, kbdBuffer, reinterpret_cast<LPWSTR>(unicodeBuffer), 5, 0);
    // When Ctrl modifier is used ToUnicode does not return correct values. In order to assign the
    // right key the control modifier is removed for just that function if the previous call failed.
    if (res == 0 && kbdBuffer[VK_CONTROL]) {
        const unsigned char controlState = kbdBuffer[VK_CONTROL];
        kbdBuffer[VK_CONTROL] = 0;
        res = ToUnicode(vk, scancode, kbdBuffer, reinterpret_cast<LPWSTR>(unicodeBuffer), 5, 0);
        kbdBuffer[VK_CONTROL] = controlState;
    }
    if (res){
        code = LStringUtil::toUpper(unicodeBuffer[0]);
    }

    // Key_*'s are not encoded below 0x20, so try again, and DEL keys (0x7f) is encoded with a
    // proper Key_ code
    if (code < 0x20 || code == 0x7f){ // Handles res==0 too
        code = winceKeyBend(vk);
    }

    if (isDeadkey){
        *isDeadkey = (res == -1);
    }

    return (code == (uint32_t)LKey::Unknown) ? 0 : code;
}

static void showSystemMenu(std::shared_ptr<LWindow> w)
{
    std::shared_ptr<LWindow> topLevel = LWindowsWin::topLevelOf(w);
    HWND topLevelHwnd = LWindowsWin::handleOf(topLevel);
    HMENU menu = GetSystemMenu(topLevelHwnd, FALSE);
    if (!menu){
        return; // no menu for this window
    }

#define enabled (MF_BYCOMMAND | MF_ENABLED)
#define disabled (MF_BYCOMMAND | MF_GRAYED)

    EnableMenuItem(menu, SC_MINIMIZE, (topLevel->flags() & LWindowType::WindowMinimizeButtonHint)?enabled:disabled);
    bool maximized = IsZoomed(topLevelHwnd);

    EnableMenuItem(menu, SC_MAXIMIZE, ! (topLevel->flags() & LWindowType::WindowMaximizeButtonHint) || maximized?disabled:enabled);
    EnableMenuItem(menu, SC_RESTORE, maximized?enabled:disabled);

    // We should _not_ check with the setFixedSize(x,y) case here, since Windows is not able to check
    // this and our menu here would be out-of-sync with the menu produced by mouse-click on the
    // System Menu, or right-click on the title bar.
    EnableMenuItem(menu, SC_SIZE, (topLevel->flags() & LWindowType::MSWindowsFixedSizeDialogHint) || maximized?disabled:enabled);
    EnableMenuItem(menu, SC_MOVE, maximized?disabled:enabled);
    EnableMenuItem(menu, SC_CLOSE, enabled);
    // Set bold on close menu item
    MENUITEMINFO closeItem;
    closeItem.cbSize = sizeof(MENUITEMINFO);
    closeItem.fMask = MIIM_STATE;
    closeItem.fState = MFS_DEFAULT;
    SetMenuItemInfo(menu, SC_CLOSE, FALSE, &closeItem);

#undef enabled
#undef disabled
    const LPoint<int> pos = LHighDpi::toNativePixels(topLevel->geometry().topLeft(), topLevel.get());
    const int ret = TrackPopupMenuEx(menu,
                                     TPM_LEFTALIGN  | TPM_TOPALIGN | TPM_NONOTIFY | TPM_RETURNCMD,
                                     pos.m_x, pos.m_y,
                                     topLevelHwnd,
                                     nullptr);
    if (ret){
        lWindowsWndProc(topLevelHwnd, WM_SYSCOMMAND, WPARAM(ret), 0);
    }
}

// Helper function that is used when obtaining the list of characters that can be produced by one key and
// every possible combination of modifiers
inline void setKbdState(unsigned char *kbd, bool shift, bool ctrl, bool alt)
{
    kbd[VK_LSHIFT  ] = (shift ? 0x80 : 0);
    kbd[VK_SHIFT   ] = (shift ? 0x80 : 0);
    kbd[VK_LCONTROL] = (ctrl ? 0x80 : 0);
    kbd[VK_CONTROL ] = (ctrl ? 0x80 : 0);
    kbd[VK_RMENU   ] = (alt ? 0x80 : 0);
    kbd[VK_MENU    ] = (alt ? 0x80 : 0);
}


LWindowsKeyMapper::LWindowsKeyMapper()
{
    memset(m_keyLayout, 0, sizeof(KeyboardLayoutItem)*m_numKeyboardLayoutItems);
//    LObject::connect(lguiApp, &LGuiApplication::applicationStateChanged,
//                     app, clearKeyRecorderOnApplicationInActive);
    changeKeyboard();
}


void LWindowsKeyMapper::changeKeyboard()
{
    deleteLayouts();
    m_seenAltGr = false;
}

LFlags<LKeyModifier> LWindowsKeyMapper::queryKeyboardModifiers()
{
    LFlags<LKeyModifier> modifiers = LKeyModifier::No;
    if (GetKeyState(VK_SHIFT) < 0){
        modifiers |= LKeyModifier::Shift;
    }
    if (GetKeyState(VK_CONTROL) < 0){
        modifiers |= LKeyModifier::Ctrl;
    }
    if (GetKeyState(VK_MENU) < 0){
        modifiers |= LKeyModifier::Alt;
    }
    if (GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0){
        modifiers |= LKeyModifier::Meta;
    }
    return modifiers;
}

std::list<uint32_t> LWindowsKeyMapper::possibleKeys(const std::shared_ptr<LKeyEvent> e) const
{
    std::list<uint32_t> result;

    const uint32_t nativeVirtualKey = e->nativeVirtualKey();
    if (nativeVirtualKey > 255){
        return result;
    }

    const KeyboardLayoutItem &kbItem = m_keyLayout[nativeVirtualKey];
    if (!kbItem.exists){
        return result;
    }

    uint32_t baseKey = kbItem.lkKey[0];
    uint32_t  keyMods = (uint32_t)e->modifiers();
    if (baseKey == (uint32_t)LKey::Return && (e->nativeModifiers() & (uint32_t)ExtendedKey)) {
        result.push_back((uint32_t)LKey::Enter + keyMods);
        return result;
    }
    result.push_back(baseKey + keyMods); // The base key is _always_ valid, of course

    for (size_t i = 1; i < NumMods; ++i) {
        uint32_t neededMods = (uint32_t)ModsTbl[i];
        uint32_t key = kbItem.lkKey[i];
        if (key && key != baseKey && ((keyMods & neededMods) == neededMods)) {
            const uint32_t missingMods = keyMods & ~neededMods;
            const uint32_t matchedKey = key + missingMods;
            const auto it =
                std::find_if(result.begin(), result.end(),
                                         [key] (int k) { return (k & ~(uint32_t)LKeyModifier::Mask2) == key; });

            if (it == result.end()){
                result.push_back(matchedKey);
            }
            else if (missingMods > (*it & (uint32_t)LKeyModifier::Mask2)){
                *it = matchedKey;
            }
        }
    }
    return result;
}


void LWindowsKeyMapper::deleteLayouts()
{
    for (KeyboardLayoutItem &k : m_keyLayout){
        k.exists = false;
    }
}

void LWindowsKeyMapper::updateKeyMap(const MSG &msg)
{
    unsigned char kbdBuffer[m_numKeyboardLayoutItems];
    GetKeyboardState(kbdBuffer);
    uint32_t vkKey = uint32_t(msg.wParam);
    if (!vkKey || (m_keyLayout[vkKey].exists && !m_keyLayout[vkKey].dirty)){
        return;
    }

    {
         const uint32_t scancode = (msg.lParam >> 16) & scancodeBitmask;
        // Copy keyboard state, so we can modify and query output for each possible permutation
        unsigned char buffer[m_numKeyboardLayoutItems];
        memcpy(buffer, kbdBuffer, sizeof(buffer));
        // Always 0, as Windows doesn't treat these as modifiers;
        buffer[VK_LWIN    ] = 0;
        buffer[VK_RWIN    ] = 0;
        buffer[VK_CAPITAL ] = 0;
        buffer[VK_NUMLOCK ] = 0;
        buffer[VK_SCROLL  ] = 0;
        // Always 0, since we'll only change the other versions
        buffer[VK_RSHIFT  ] = 0;
        buffer[VK_RCONTROL] = 0;
        buffer[VK_LMENU   ] = 0; // Use right Alt, since left Ctrl + right Alt is considered AltGraph

        // m_keyLayout contains the actual characters which can be written using the vk_key together with the
        // different modifiers. '2' together with shift will for example cause the character
        // to be @ for a US key layout (thus m_keyLayout[vk_key].qtKey[1] will be @). In addition to that
        // it stores whether the resulting key is a dead key as these keys have to be handled later.
        bool isDeadKey = false;
        m_keyLayout[vkKey].deadkeys = 0;
        m_keyLayout[vkKey].dirty = false;
        m_keyLayout[vkKey].exists = true;
        setKbdState(buffer, false, false, false);
        m_keyLayout[vkKey].lkKey[0] = toKeyOrUnicode(vkKey, scancode, buffer, &isDeadKey);
        m_keyLayout[vkKey].deadkeys |= isDeadKey ? 0x01 : 0;
        setKbdState(buffer, true, false, false);
        m_keyLayout[vkKey].lkKey[1] = toKeyOrUnicode(vkKey, scancode, buffer, &isDeadKey);
        m_keyLayout[vkKey].deadkeys |= isDeadKey ? 0x02 : 0;
        setKbdState(buffer, false, true, false);
        m_keyLayout[vkKey].lkKey[2] = toKeyOrUnicode(vkKey, scancode, buffer, &isDeadKey);
        m_keyLayout[vkKey].deadkeys |= isDeadKey ? 0x04 : 0;
        setKbdState(buffer, true, true, false);
        m_keyLayout[vkKey].lkKey[3] = toKeyOrUnicode(vkKey, scancode, buffer, &isDeadKey);
        m_keyLayout[vkKey].deadkeys |= isDeadKey ? 0x08 : 0;
        setKbdState(buffer, false, false, true);
        m_keyLayout[vkKey].lkKey[4] = toKeyOrUnicode(vkKey, scancode, buffer, &isDeadKey);
        m_keyLayout[vkKey].deadkeys |= isDeadKey ? 0x10 : 0;
        setKbdState(buffer, true, false, true);
        m_keyLayout[vkKey].lkKey[5] = toKeyOrUnicode(vkKey, scancode, buffer, &isDeadKey);
        m_keyLayout[vkKey].deadkeys |= isDeadKey ? 0x20 : 0;
        setKbdState(buffer, false, true, true);
        m_keyLayout[vkKey].lkKey[6] = toKeyOrUnicode(vkKey, scancode, buffer, &isDeadKey);
        m_keyLayout[vkKey].deadkeys |= isDeadKey ? 0x40 : 0;
        setKbdState(buffer, true, true, true);
        m_keyLayout[vkKey].lkKey[7] = toKeyOrUnicode(vkKey, scancode, buffer, &isDeadKey);
        m_keyLayout[vkKey].deadkeys |= isDeadKey ? 0x80 : 0;
        // Add a fall back key for layouts which don't do composition and show non-latin1 characters
        uint32_t fallbackKey = winceKeyBend(vkKey);
        if (!fallbackKey || fallbackKey == (uint32_t)LKey::Unknown) {
            fallbackKey = 0;
            if ((vkKey != m_keyLayout[vkKey].lkKey[0])
                && (vkKey != m_keyLayout[vkKey].lkKey[1])
                && (vkKey < uint32_t(0x5B))
                && (vkKey > uint32_t(0x2F))){
                fallbackKey = vkKey;
            }
        }
        m_keyLayout[vkKey].lkKey[8] = fallbackKey;

        // If one of the values inserted into the m_keyLayout above, can be considered a dead key, we have
        // to run the workaround below.
        if (m_keyLayout[vkKey].deadkeys) {
            // Push a Space, then the original key through the low-level ToAscii functions.
            // We do this because these functions (ToAscii / ToUnicode) will alter the internal state of
            // the keyboard driver By doing the following, we set the keyboard driver state back to what
            // it was before we wrecked it with the code above.
            // We need to push the space with an empty keystate map, since the driver checks the map for
            // transitions in modifiers, so this helps us capture all possible deadkeys.
            unsigned char emptyBuffer[256];
            memset(emptyBuffer, 0, sizeof(emptyBuffer));
            ::ToAscii(VK_SPACE, 0, emptyBuffer, reinterpret_cast<LPWORD>(&buffer), 0);
            ::ToAscii(vkKey, scancode, kbdBuffer, reinterpret_cast<LPWORD>(&buffer), 0);
        }
    }
}

bool LWindowsKeyMapper::translateKeyEvent(std::shared_ptr<LWindow> widget, HWND hwnd, const MSG &msg, LRESULT *result)
{
    *result = 0;

    // Reset layout map when system keyboard layout is changed
    if (msg.message == WM_INPUTLANGCHANGE) {
        changeKeyboard();
        return true;
    }

    if (msg.message == WM_APPCOMMAND){
        return translateMultimediaKeyEventInternal(widget, msg);
    }

    // WM_(IME_)CHAR messages already contain the character in question so there is
    // no need to fiddle with our key map. In any other case add this key to the
    // keymap if it is not present yet.
    if (msg.message != WM_CHAR && msg.message != WM_IME_CHAR){
        updateKeyMap(msg);
    }

    MSG peekedMsg;
    // consume dead chars?(for example, typing '`','a' resulting in a-accent).
    if (PeekMessage(&peekedMsg, hwnd, 0, 0, PM_NOREMOVE) && peekedMsg.message == WM_DEADCHAR){
        return true;
    }

    return translateKeyEventInternal(widget, msg, false, result);
}


bool LWindowsKeyMapper::translateKeyEventInternal(std::shared_ptr<LWindow> window, MSG msg, bool grab, LRESULT *lResult)
{
    const bool altGr = m_detectAltGrModifier && isAltGr(&msg);
    if (altGr){
        m_seenAltGr = true;
    }
    const UINT msgType = msg.message;

    const uint32_t scancode = (msg.lParam >> 16) & scancodeBitmask;
    auto vk_key = uint32_t(msg.wParam);
    uint32_t nModifiers = 0;

    std::shared_ptr<LWindow> receiver = m_keyGrabber.lock();
    if(receiver == nullptr){
        receiver = window;
    }

    // Map native modifiers to some bit representation
    nModifiers |= (GetKeyState(VK_LSHIFT  ) & 0x80 ? ShiftLeft : 0);
    nModifiers |= (GetKeyState(VK_RSHIFT  ) & 0x80 ? ShiftRight : 0);
    nModifiers |= (GetKeyState(VK_LCONTROL) & 0x80 ? ControlLeft : 0);
    nModifiers |= (GetKeyState(VK_RCONTROL) & 0x80 ? ControlRight : 0);
    nModifiers |= (GetKeyState(VK_LMENU   ) & 0x80 ? AltLeft : 0);
    nModifiers |= (GetKeyState(VK_RMENU   ) & 0x80 ? AltRight : 0);
    nModifiers |= (GetKeyState(VK_LWIN    ) & 0x80 ? MetaLeft : 0);
    nModifiers |= (GetKeyState(VK_RWIN    ) & 0x80 ? MetaRight : 0);
    // Add Lock keys to the same bits
    nModifiers |= (GetKeyState(VK_CAPITAL ) & 0x01 ? CapsLock : 0);
    nModifiers |= (GetKeyState(VK_NUMLOCK ) & 0x01 ? NumLock : 0);
    nModifiers |= (GetKeyState(VK_SCROLL  ) & 0x01 ? ScrollLock : 0);

    if (msg.lParam & ExtendedKey){
        nModifiers |= msg.lParam & ExtendedKey;
    }

    // Get the modifier states (may be altered later, depending on key code)
    LFlags<LKeyModifier> state = LKeyModifier::No;
    state |= (nModifiers & ShiftAny ? LKeyModifier::Shift : LKeyModifier::No);
    state |= (nModifiers & AltLeft ? LKeyModifier::Shift : LKeyModifier::No);
    if ((nModifiers & AltRight) != 0){
        state |= m_seenAltGr ? LKeyModifier::GroupSwitch : LKeyModifier::Alt;
    }
    if ((nModifiers & ControlAny) != 0 && uint32_t(state & LKeyModifier::GroupSwitch) == 0){
        state |= LKeyModifier::Ctrl;
    }
    state |= (nModifiers & MetaAny ? LKeyModifier::Meta : LKeyModifier::No);
    // A multi-character key or a Input method character
    // not found by our look-ahead
    if (msgType == WM_CHAR || msgType == WM_IME_CHAR) {
        sendExtendedPressRelease(receiver, 0, state, scancode, vk_key, nModifiers, messageKeyText(msg), false);
        return true;
    }

    // Enable Alt accelerators ("&File") on menus
    if (msgType == WM_SYSKEYDOWN && (nModifiers & AltAny) != 0 && GetMenu(msg.hwnd) != nullptr){
        return false;
    }
    if (msgType == WM_SYSKEYUP && nModifiers == 0 && GetMenu(msg.hwnd) != nullptr){
        return false;
    }

    bool result = false;
    // handle Directionality changes (BiDi) with RTL extensions
    if (m_useRTLExtensions) {
        static int dirStatus = 0;
        if (!dirStatus && state == LKeyModifier::Ctrl
            && msg.wParam == VK_CONTROL
            && msgType == WM_KEYDOWN) {
            if (GetKeyState(VK_LCONTROL) < 0)
                dirStatus = VK_LCONTROL;
            else if (GetKeyState(VK_RCONTROL) < 0)
                dirStatus = VK_RCONTROL;
        } else if (dirStatus) {
            if (msgType == WM_KEYDOWN) {
                if (msg.wParam == VK_SHIFT) {
                    if (dirStatus == VK_LCONTROL && GetKeyState(VK_LSHIFT) < 0)
                        dirStatus = VK_LSHIFT;
                    else if (dirStatus == VK_RCONTROL && GetKeyState(VK_RSHIFT) < 0)
                        dirStatus = VK_RSHIFT;
                } else {
                    dirStatus = 0;
                }
            } else if (msgType == WM_KEYUP) {
                if (dirStatus == VK_LSHIFT
                    && ((msg.wParam == VK_SHIFT && GetKeyState(VK_LCONTROL))
                        || (msg.wParam == VK_CONTROL && GetKeyState(VK_LSHIFT)))) {
                    sendExtendedPressRelease(receiver, (uint32_t)LKey::Direction_L, {},
                                             scancode, vk_key, nModifiers, std::wstring(), false);
                    result = true;
                    dirStatus = 0;
                } else if (dirStatus == VK_RSHIFT
                           && ( (msg.wParam == VK_SHIFT && GetKeyState(VK_RCONTROL))
                               || (msg.wParam == VK_CONTROL && GetKeyState(VK_RSHIFT)))) {
                    sendExtendedPressRelease(receiver, (uint32_t)LKey::Direction_R, {},
                                             scancode, vk_key, nModifiers, std::wstring(), false);
                    result = true;
                    dirStatus = 0;
                } else {
                    dirStatus = 0;
                }
            } else {
                dirStatus = 0;
            }
        }
    } // RTL

    // IME will process these keys, so simply return
    if (msg.wParam == VK_PROCESSKEY){
        return true;
    }

    // Ignore invalid virtual keycodes
    if (msg.wParam == 0 || msg.wParam == 0xFF){
        return true;
    }

    // Translate VK_* (native) -> Key_* (Lk) keys
    int modifiersIndex = 0;
    modifiersIndex |= (nModifiers & ShiftAny ? 0x1 : 0);
    modifiersIndex |= (nModifiers & ControlAny ? 0x2 : 0);
    modifiersIndex |= (nModifiers & AltAny ? 0x4 : 0);

    // Note: For the resulting key, AltGr is equivalent to Alt + Ctrl (as
    // opposed to Linux); hence no entry in KeyboardLayoutItem is required
    uint32_t code = m_keyLayout[vk_key].lkKey[modifiersIndex];

    // If the bit 24 of lParm is set you received a enter,
    // otherwise a Return. (This is the extended key bit)
    if ((code == (uint32_t)LKey::Return) && (msg.lParam & 0x1000000)){
        code = (uint32_t)LKey::Enter;
    }
    else if (altGr){
        code = (uint32_t)LKey::AltGr;
    }

    // Invert state logic:
    // If the key actually pressed is a modifier key, then we remove its modifier key from the
    // state, since a modifier-key can't have itself as a modifier
    if (code == (uint32_t)LKey::Control){
        state = state ^ LKeyModifier::Ctrl;
    }
    else if (code == (uint32_t)LKey::Shift){
        state = state ^ LKeyModifier::Shift;
    }
    else if (code == (uint32_t)LKey::Alt){
        state = state ^ LKeyModifier::Alt;
    }
    else if (code == (uint32_t)LKey::AltGr){
        state = state ^ LKeyModifier::GroupSwitch;
    }

    // All cursor keys without extended bit
    if (!(msg.lParam & 0x1000000)) {
        switch (code) {
        case (uint32_t)LKey::Left:
        case (uint32_t)LKey::Right:
        case (uint32_t)LKey::Up:
        case (uint32_t)LKey::Down:
        case (uint32_t)LKey::PageUp:
        case (uint32_t)LKey::PageDown:
        case (uint32_t)LKey::Home:
        case (uint32_t)LKey::End:
        case (uint32_t)LKey::Insert:
        case (uint32_t)LKey::Delete:
        case (uint32_t)LKey::Asterisk:
        case (uint32_t)LKey::Plus:
        case (uint32_t)LKey::Minus:
        case (uint32_t)LKey::Period:
        case (uint32_t)LKey::Comma:
        case (uint32_t)LKey::Num0:
        case (uint32_t)LKey::Num1:
        case (uint32_t)LKey::Num2:
        case (uint32_t)LKey::Num3:
        case (uint32_t)LKey::Num4:
        case (uint32_t)LKey::Num5:
        case (uint32_t)LKey::Num6:
        case (uint32_t)LKey::Num7:
        case (uint32_t)LKey::Num8:
        case (uint32_t)LKey::Num9:{
            state |= ((msg.wParam >= '0' && msg.wParam <= '9')
                      || (msg.wParam >= VK_OEM_PLUS && msg.wParam <= VK_OEM_3))
                         ? LKeyModifier::No : LKeyModifier::Keypad;
            //No break;
        }
        default:{
            if (uint32_t(msg.lParam) == 0x004c0001 || uint32_t(msg.lParam) == 0xc04c0001){
                state |= LKeyModifier::Keypad;
            }
            break;
        }
        }
    }
    // Other keys with with extended bit
    else {
        switch (code) {
        case (uint32_t)LKey::Enter:
        case (uint32_t)LKey::Slash:
        case (uint32_t)LKey::NumLock:{
            state |= LKeyModifier::Keypad;
            break;
        }
        default:{
            break;
        }
        }
    }

    // KEYDOWN ---------------------------------------------------------------------------------
    if (msgType == WM_KEYDOWN || msgType == WM_IME_KEYDOWN || msgType == WM_SYSKEYDOWN) {
        // Get the last record of this key press, so we can validate the current state
        // The record is not removed from the list
        LWinKeyRecord *rec = LWinKeyRecorder::inst().findKey(int(msg.wParam), false);

        // If rec's state doesn't match the current state, something has changed behind our back
        // (Consumed by modal widget is one possibility) So, remove the record from the list
        // This will stop the auto-repeat of the key, should a modifier change, for example
        if (rec && rec->state != uint32_t(state)) {
            LWinKeyRecorder::inst().findKey(int(msg.wParam), true);
            rec = nullptr;
        }

        // Find unicode character from Windows Message Queue
        MSG wm_char;
        UINT charType = (msgType == WM_KEYDOWN
                             ? WM_CHAR
                             : msgType == WM_IME_KEYDOWN ? WM_IME_CHAR : WM_SYSCHAR);

        wchar_t uch = 0;
        if (PeekMessage(&wm_char, nullptr, charType, charType, PM_REMOVE)) {
            if (LWindowsCtx::filterNativeEvent(&wm_char, lResult)){
                return true;
            }
            if (receiver && LWindowsCtx::filterNativeEvent(receiver, &wm_char, lResult)){
                return true;
            }
            // Found a ?_CHAR
            uch = wchar_t(wm_char.wParam);
            if (LStringUtil::isHighSurrogate(uch)) {
                m_lastHighSurrogate = uch;
                return true;
            }
            if (LStringUtil::isLowSurrogate(uch) && m_lastHighSurrogate !=0) {
                std::shared_ptr<LObject> focusObject = LGuiApplication::focusObject();
                if (focusObject != nullptr) {
                    std::wstring ws;
                    ws += (wchar_t)m_lastHighSurrogate;
                    ws += (wchar_t)uch;
                    std::shared_ptr<LInputMethodEvent> event = std::make_shared<LInputMethodEvent>();

                    event->setCommitString(ws);
                    LCoreApplication::sendEvent(focusObject, event);
                }
                m_lastHighSurrogate = 0;
                return true;
            } else {
                m_lastHighSurrogate = 0;
            }
            if (msgType == WM_SYSKEYDOWN && LStringUtil::isLetter(uch) && (msg.lParam & KF_ALTDOWN)){
                uch = LStringUtil::toLower(uch); // (See doc of WM_SYSCHAR) Alt-letter
            }
            if (!code && !LStringUtil::row(uch)){
                code = asciiToKeycode(LStringUtil::cell(uch), state);
            }
        }

        // Special handling for the WM_IME_KEYDOWN message. Microsoft IME (Korean) will not
        // generate a WM_IME_CHAR message corresponding to this message. We might get wrong
        // results, if we map this virtual key-code directly (for eg '?' US layouts). So try
        // to find the correct key using the current message parameters & keyboard state.
        if (uch == 0 && msgType == WM_IME_KEYDOWN) {
            std::shared_ptr<LPlatformInputCtx> t =
                LWindowsIntegration::instance()->inputContext();

            std::shared_ptr<LWindowsInputCtx> windowsInputContext
                = std::dynamic_pointer_cast<LWindowsInputCtx>(t);

            if(windowsInputContext == nullptr){
                if(windowsInputContext->isComposing()){
                    vk_key = ImmGetVirtualKey(reinterpret_cast<HWND>(window->winId()));
                }
            }

            BYTE keyState[256];
            wchar_t newKey[3] = {0};
            GetKeyboardState(keyState);
            int val = ToUnicode(vk_key, scancode, keyState, newKey, 2,  0);
            if (val == 1) {
                uch = newKey[0];
            } else {
                // If we are still not able to find a unicode key, pass the WM_IME_KEYDOWN
                // message to DefWindowProc() for generating a proper WM_KEYDOWN.
                return false;
            }
        }

        // If no ?_CHAR was found in the queue; deduct character from the ?_KEYDOWN parameters
        if (uch == 0) {
            if (msg.wParam == VK_DELETE) {
                uch = 0x7f; // Windows doesn't know this one.
            } else {
                if (msgType != WM_SYSKEYDOWN || !code) {
                    UINT map = MapVirtualKey(UINT(msg.wParam), 2);
                    // If the high bit of the return value is set, it's a deadkey
                    if (!(map & 0x80000000)){
                        uch = uint16_t(map);
                    }
                }
            }
            if (!code && !LStringUtil::row(uch)){
                code = asciiToKeycode(LStringUtil::cell(uch), state);
            }
        }

        // Special handling of global Windows hotkeys
        if (state == LKeyModifier::Alt) {
            switch (code) {
            case (uint32_t)LKey::Escape:
            case (uint32_t)LKey::Tab:
            case (uint32_t)LKey::Enter:
            case (uint32_t)LKey::F4:{
                return false; // Send the event on to Windows
            }
            case (uint32_t)LKey::Space:{
                // do not pass this key to windows, we will process it ourselves
                showSystemMenu(receiver);
                return true;
            }
            default:{
                break;
            }
            }
        }

        // Map SHIFT + Tab to SHIFT + BackTab, LShortcutMap knows about this translation
        if (code == (uint32_t)LKey::Tab && (state & LKeyModifier::Shift) == LKeyModifier::Shift){
            code = (uint32_t)LKey::Backtab;
        }

        // If we have a record, it means that the key is already pressed, the state is the same
        // so, we have an auto-repeating key
        if (rec) {
            if (code < (uint32_t)LKey::Shift || code > (uint32_t)LKey::ScrollLock) {
                LWindowSystemInterface::handleExtendedKeyEvent(receiver, LEvent::KeyRelease, code,
                                                               state, scancode, uint32_t(msg.wParam), nModifiers, rec->text, true);
                LWindowSystemInterface::handleExtendedKeyEvent(receiver, LEvent::KeyPress, code,
                                                               state, scancode, uint32_t(msg.wParam), nModifiers, rec->text, true);
                result = true;
            }
        }
        // No record of the key being previous pressed, so we now send a QEvent::KeyPress event,
        // and store the key data into our records.
        else {
            std::wstring text;
            if(uch !=0){
                text += uch;
            }
            const char a = LStringUtil::row(uch) ? char(0) : LStringUtil::cell(uch);
            const LFlags<LKeyModifier> modifiers(state);

            if (modifiers == LKeyModifier::Shift && code == (uint32_t)LKey::F10
                && !LGuiApplication::guiInst()->m_shortcutMan.hasShortcutForKeySequence(LKeySequence((uint32_t)LKeyModifier::Shift + (uint32_t)LKey::F10))) {
                return false;
            }
            LWinKeyRecorder::inst().storeKey(int(msg.wParam), a, state, text);

            // VK_PACKET specifies multiple characters. The system only sends the first
            // character of this sequence for each.
            if (msg.wParam == VK_PACKET){
                code = asciiToKeycode(LStringUtil::cell(uch), state);
            }

            LWindowSystemInterface::handleExtendedKeyEvent(receiver, LEvent::KeyPress, code,
                                                           modifiers, scancode, uint32_t(msg.wParam), nModifiers, text, false);
            result =true;
            bool store = true;
            // Alt+<alphanumerical> go to the Win32 menu system if unhandled by Qt
            if (msgType == WM_SYSKEYDOWN && !result && a) {
                HWND parent = GetParent(LWindowsWin::handleOf(receiver));
                while (parent) {
                    if (GetMenu(parent)) {
                        SendMessage(parent, WM_SYSCOMMAND, SC_KEYMENU, a);
                        store = false;
                        result = true;
                        break;
                    }
                    parent = GetParent(parent);
                }
            }
            if (!store){
                LWinKeyRecorder::inst().findKey(int(msg.wParam), true);
            }
        }
    }

    // KEYUP -----------------------------------------------------------------------------------
    else {
        // Try to locate the key in our records, and remove it if it exists.
        // The key may not be in our records if, for example, the down event was handled by
        // win32 natively, or our window gets focus while a key is already press, but now gets
        // the key release event.
        const LWinKeyRecord *rec = LWinKeyRecorder::inst().findKey(int(msg.wParam), true);
        if (!rec && !(code == (uint32_t)LKey::Shift
                      || code == (uint32_t)LKey::Control
                      || code == (uint32_t)LKey::Meta
                      || code == (uint32_t)LKey::Alt)) {

            // The Surface Pen eraser button generates Meta+F18/19/20 keystrokes,
            // but when it is not touching the screen the Fn Down is eaten and only
            // a Fn Up with the previous state as "not pressed" is generated, which
            // would be ignored. We detect this case and synthesize the expected events.
            if ((msg.lParam & 0x40000000) == 0 &&
                state == LKeyModifier::No &&
                ((code == (uint32_t)LKey::F18) || (code == (uint32_t)LKey::F19) || (code == (uint32_t)LKey::F20))) {
                LWindowSystemInterface::handleExtendedKeyEvent(receiver, LEvent::KeyPress, code,
                                                               LKeyModifier::Meta, scancode,
                                                               uint32_t(msg.wParam), MetaLeft);
                LWindowSystemInterface::handleExtendedKeyEvent(receiver, LEvent::KeyRelease, code,
                                                               LKeyModifier::No, scancode,
                                                               uint32_t(msg.wParam), 0);
                result = true;
            }
        } else {
            if (!code){
                code = asciiToKeycode(rec->ascii ? char(rec->ascii) : char(msg.wParam), state);
            }


            // Map SHIFT + Tab to SHIFT + BackTab, LShortcutMap knows about this translation
            if (code == (uint32_t)LKey::Tab && (state & LKeyModifier::Shift) == LKeyModifier::Shift){
                code = (uint32_t)LKey::Backtab;
            }
            LWindowSystemInterface::handleExtendedKeyEvent(receiver, LEvent::KeyRelease, code,
                                                           state, scancode, uint32_t(msg.wParam),
                                                           nModifiers,
                                                           (rec ? rec->text : std::wstring()), false);

            result = true;
            // don't pass Alt to Windows unless we are embedded in other kind of window
            if (code == (uint32_t)LKey::Alt) {
                const LWindowsCtx *context = LWindowsCtx::instance();
                HWND parent = GetParent(LWindowsWin::handleOf(receiver));
                while (parent) {
                    if (!context->findPlatformWindow(parent) && GetMenu(parent)) {
                        result = false;
                        break;
                    }
                    parent = GetParent(parent);
                }
            }
        }
    }
    return result;
}

bool LWindowsKeyMapper::translateMultimediaKeyEventInternal(std::shared_ptr<LWindow> window, const MSG &msg)
{
    const int cmd = GET_APPCOMMAND_LPARAM(msg.lParam);
    // do not send mouse-synthesized commands as key events in addition
    bool skipPressRelease = false;
    switch (GET_DEVICE_LPARAM(msg.lParam)) {
    case FAPPCOMMAND_MOUSE:{
        return false;
    }
    case FAPPCOMMAND_KEY:{
        //  use WM_KEYDOWN/WM_KEYUP for commands that are reflected
        // in VK(s) like VK_MEDIA_NEXT_TRACK, to get correct codes and autorepeat.
        // Don't do that for APPCOMMAND_BROWSER_HOME as that one does not trigger two events.
        if (cmd != APPCOMMAND_BROWSER_HOME){
            skipPressRelease = true;
        }
        break;
    }
    }

    const int dwKeys = GET_KEYSTATE_LPARAM(msg.lParam);
    LFlags<LKeyModifier> state = LKeyModifier::No;
    state |= (dwKeys & MK_SHIFT ? LKeyModifier::Shift : LKeyModifier::No);
    state |= (dwKeys & MK_CONTROL ? LKeyModifier::Ctrl : LKeyModifier::No);

    std::shared_ptr<LWindow> receiver = m_keyGrabber.lock();
    if(receiver == nullptr){
        receiver = window;
    }

    if ((cmd < 0) || (cmd > 52)){
        return false;
    }

    const uint32_t lkKey = int(CmdTbl[cmd]);
    if (!skipPressRelease){
        sendExtendedPressRelease(receiver, lkKey, state , 0, 0, 0);
    }
        // Make sure to return false if  does not handle the key, otherwise,
        // the keys are not passed to the active media player.
    const LKeySequence sequence(state + lkKey);
    return LGuiApplication::guiInst()->m_shortcutMan.hasShortcutForKeySequence(sequence);
}

void LWindowsKeyMapper::clearKeyRecorderOnApplicationInActive(LApplicationState state)
{
    if (state == LApplicationState::Inactive){
        LWinKeyRecorder::inst().clearKeys();
    }
}


LK_NAMESPACE_END
