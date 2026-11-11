#include "lkeysequence.h"
#include <lkcore/ldebug.h>

LK_NAMESPACE_BEGIN

bool LKeySequence::m_bindingInit = false;
std::map<uint32_t, LKeySequence::StandardKey> LKeySequence::m_innerBindings;

LKeySequence::LKeySequence(): std::array<uint32_t, 4>({})
{

}

LKeySequence::LKeySequence(uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3)
    : std::array<uint32_t, 4>({v0, v1, v2, v3})
{

}

LKeySequence::LKeySequence(const LKeySequence &ks)
     : std::array<uint32_t, 4>({ks[0], ks[1], ks[2], ks[3]})
{
}

LKeySequence::LKeySequence(const StandardKey &key): std::array<uint32_t, 4>({}){
    initBindings();
    std::map<uint32_t, StandardKey>::const_iterator iter = m_innerBindings.begin();

    for (; iter != m_innerBindings.end(); ++iter) {
        if(iter->second == key){
            _Elems[0] = iter->first;
        }
    }
}

uint32_t LKeySequence::count() const
{
    uint32_t i=0;
    for(; i<size(); ++i){
        if(_Elems[i] ==0){
            return i;
        }
    }
    return i;
}

bool LKeySequence::isValid()  const
{
    return _Elems[0] != 0;
}

bool LKeySequence::isEmpty()  const
{
    return _Elems[0] == 0;
}

LKeySequence::SequenceMatch LKeySequence::matches(const LKeySequence &seq) const
{
    uint32_t userN = count(), seqN = seq.count();

    if (userN > seqN){
        return NoMatch;
    }

    for (uint32_t i = 0; i < userN; ++i) {
        if (_Elems[i] != seq[i]){
            return NoMatch;
        }
    }
    return (userN == seqN ? ExactMatch : PartialMatch);
}

std::wstring LKeySequence::toString() const
{
    std::wstring finalString;
    int end = count();
    int i=0;
    if(i<end){
        finalString += encodeString(_Elems[i]);
    }
    for (i = 1; i < end; ++i) {
        finalString += L", ";
        finalString += encodeString(_Elems[i]);

    }
    return finalString;
}

bool LKeySequence::operator<(const LKeySequence &other) const
{
    return std::lexicographical_compare(_Elems, _Elems + maxSize(),
                                        other._Elems, other._Elems + maxSize());
}

bool LKeySequence::operator==(const LKeySequence &other) const
{
    return (_Elems[0] == other._Elems[0] &&
            _Elems[1] == other._Elems[1] &&
            _Elems[2] == other._Elems[2] &&
            _Elems[3] == other._Elems[3]);
}

void LKeySequence::initBindings()
{
    if(m_bindingInit == false){
        m_bindingInit = true;

        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::F1)] = LKeySequence::HelpContents;
        m_innerBindings[uint32_t(LKeyModifier::Shift) | uint32_t(LKey::F1)] = LKeySequence::WhatsThis;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::O)] = LKeySequence::Open;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::F4)] = LKeySequence::Close;
        //        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::W)] = LKeySequence::Close;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::S)] = LKeySequence::Save;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::N)] = LKeySequence::New;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::D)] = LKeySequence::Delete;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::X)] = LKeySequence::Cut;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::C)] = LKeySequence::Copy;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::V)] = LKeySequence::Paste;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::Z)] = LKeySequence::Undo;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::Y)] = LKeySequence::Redo;

        m_innerBindings[uint32_t(LKeyModifier::Alt) | uint32_t(LKey::Left)] = LKeySequence::Back;
        m_innerBindings[uint32_t(LKeyModifier::Alt) | uint32_t(LKey::Right)] = LKeySequence::Forward;
        m_innerBindings[uint32_t(LKey::F5)] = LKeySequence::Refresh;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::Plus)] = LKeySequence::ZoomIn;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::Minus)] = LKeySequence::ZoomOut;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::P)] = LKeySequence::Print;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::T)] = LKeySequence::AddTab;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::Tab)] = LKeySequence::NextChild;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKeyModifier::Shift) | uint32_t(LKey::Tab)]
            = LKeySequence::PreviousChild;

        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::F)] = LKeySequence::Find;
        m_innerBindings[uint32_t(LKey::F3)] = LKeySequence::FindNext;
        m_innerBindings[uint32_t(LKeyModifier::Shift) |uint32_t(LKey::F3)] = LKeySequence::FindPrevious;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::R)] = LKeySequence::Replace;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::A)] = LKeySequence::SelectAll;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::B)] = LKeySequence::Bold;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::I)] = LKeySequence::Italic;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::U)] = LKeySequence::Underline;
        m_innerBindings[uint32_t(LKey::Right)] = LKeySequence::MoveToNextChar;
        m_innerBindings[uint32_t(LKey::Left)] = LKeySequence::MoveToPreviousChar;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::Right)] = LKeySequence::MoveToNextWord;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::Left)] = LKeySequence::MoveToPreviousWord;

        m_innerBindings[uint32_t(LKey::Down)] = LKeySequence::MoveToNextLine;
        m_innerBindings[uint32_t(LKey::Up)] = LKeySequence::MoveToPreviousLine;
        m_innerBindings[uint32_t(LKey::PageDown)] = LKeySequence::MoveToNextPage;
        m_innerBindings[uint32_t(LKey::PageUp)] = LKeySequence::MoveToPreviousPage;

        m_innerBindings[uint32_t(LKey::Home)] = LKeySequence::MoveToStartOfLine;
        m_innerBindings[uint32_t(LKey::End)] = LKeySequence::MoveToEndOfLine;

        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::Home)] = LKeySequence::MoveToStartOfDocument;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::End)] = LKeySequence::MoveToEndOfDocument;


        m_innerBindings[uint32_t(LKeyModifier::Shift) | uint32_t(LKey::Right)] = LKeySequence::SelectNextChar;
        m_innerBindings[uint32_t(LKeyModifier::Shift) | uint32_t(LKey::Left)] = LKeySequence::SelectPreviousChar;

        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKeyModifier::Shift) | uint32_t(LKey::Right)]
            = LKeySequence::SelectNextWord;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKeyModifier::Shift) | uint32_t(LKey::Left)]
            = LKeySequence::SelectPreviousWord;

        m_innerBindings[uint32_t(LKeyModifier::Shift) | uint32_t(LKey::Down)] = LKeySequence::SelectNextLine;
        m_innerBindings[uint32_t(LKeyModifier::Shift) | uint32_t(LKey::Up)] = LKeySequence::SelectPreviousLine;

        m_innerBindings[uint32_t(LKeyModifier::Shift) | uint32_t(LKey::PageDown)] = LKeySequence::SelectNextPage;
        m_innerBindings[uint32_t(LKeyModifier::Shift) | uint32_t(LKey::PageUp)] = LKeySequence::SelectPreviousPage;

        m_innerBindings[uint32_t(LKeyModifier::Shift) | uint32_t(LKey::Home)] = LKeySequence::SelectStartOfLine;
        m_innerBindings[uint32_t(LKeyModifier::Shift) | uint32_t(LKey::End)] = LKeySequence::SelectEndOfLine;

        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKeyModifier::Shift) | uint32_t(LKey::Home)]
            = LKeySequence::SelectStartOfDocument;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKeyModifier::Shift) | uint32_t(LKey::End)]
            = LKeySequence::SelectEndOfDocument;

        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::Backspace)] = LKeySequence::DeleteStartOfWord;
        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::Delete)] = LKeySequence::DeleteEndOfWord;

        //m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::K)] = LKeySequence::DeleteEndOfLine; //emacs only

        m_innerBindings[uint32_t(LKey::Enter)] = LKeySequence::InsertParagraphSeparator;
        m_innerBindings[uint32_t(LKey::Return)] = LKeySequence::InsertParagraphSeparator;

        m_innerBindings[uint32_t(LKeyModifier::Shift) |uint32_t(LKey::Enter)] = LKeySequence::InsertLineSeparator;
        m_innerBindings[uint32_t(LKeyModifier::Shift) |uint32_t(LKey::Return)] = LKeySequence::InsertLineSeparator;

        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKeyModifier::Shift) | uint32_t(LKey::S)]
            = LKeySequence::SaveAs;

        m_innerBindings[uint32_t(LKeyModifier::Ctrl) | uint32_t(LKey::Q)] = LKeySequence::Quit;
        m_innerBindings[uint32_t(LKey::F11)] = LKeySequence::FullScreen;

        m_innerBindings[uint32_t(LKey::Escape)] = LKeySequence::Cancel;
    }
}

const std::list<LKeySequence> LKeySequence::keyBindings(StandardKey key)
{
    initBindings();
    std::list<LKeySequence> ret;
    std::map<uint32_t, StandardKey>::const_iterator iter = m_innerBindings.begin();

    for (; iter != m_innerBindings.end(); ++iter) {
        if(iter->second == key){
            ret.push_back(LKeySequence(iter->first));
        }
    }
    return ret;
}

static inline void addKey(std::wstring &str, const std::wstring &theKey)
{
    if (!str.empty()) {
        str += wchar_t('+');
    }
    str += theKey;
}

const std::wstring LKeySequence::encodeString(uint32_t key)
{
    std::wstring s;
    if (key == uint32_t(-1) || key == (uint32_t)LKey::Unknown){
        return s;
    }

    {
        // On other systems the order is Meta, Control, Alt, Shift
        if ((key & (uint32_t)LKeyModifier::Meta) == (uint32_t)LKeyModifier::Meta){
            s = L"Meta";
        }
        if ((key & (uint32_t)LKeyModifier::Ctrl)  == (uint32_t)LKeyModifier::Ctrl){
            addKey(s, L"Ctrl");
        }
        if ((key & (uint32_t)LKeyModifier::Alt)  == (uint32_t)LKeyModifier::Alt){
            addKey(s, L"Alt");
        }
        if ((key & (uint32_t)LKeyModifier::Shift)  == (uint32_t)LKeyModifier::Shift){
            addKey(s, L"Shift");
        }
    }
    if ((key & (uint32_t)LKeyModifier::Keypad) == (uint32_t)LKeyModifier::Keypad){
        addKey(s, L"Num");
    }

    std::wstring p = keyName(key);
    addKey(s, p);
    return s;
}

const std::map<uint32_t, LKeySequence::StandardKey>& LKeySequence::innnerBindings()
{
    initBindings();
    return m_innerBindings;
}

void LKeySequence::setInnerBindings(const std::map<uint32_t, StandardKey> &bindings)
{
    m_innerBindings = bindings;
}

const std::wstring LKeySequence::keyName(uint32_t key)
{
    key &= ~uint32_t(LKeyModifier::Mask0);
    std::wstring p;

    if (key && key < (uint32_t)LKey::Escape && key != (uint32_t)LKey::Space) {
        if (!LStringUtil::requiresSurrogates(key)) {
            p = wchar_t(LStringUtil::toUpper(key));
        } else {
            p += wchar_t(LStringUtil::highSurrogate(key));
            p += wchar_t(LStringUtil::lowSurrogate(key));
        }
    } else if (key >= (uint32_t)LKey::F1 && key <= (uint32_t)LKey::F35) {
        p = L"F" + std::to_wstring(key - (uint32_t)LKey::F1 + 1);
    } else if (key) {
        p = keyNameHelper(key);

        if (p.empty()) {
            if (!LStringUtil::requiresSurrogates(key)) {
                p = wchar_t(LStringUtil::toUpper(key));
            } else {
                p = wchar_t(LStringUtil::highSurrogate(key));
                p += wchar_t(LStringUtil::lowSurrogate(key));
            }
        }
    }
    return p;
}

const std::wstring& LKeySequence::keyNameHelper(const uint32_t &key)
{
    switch((LKey)key){
    case LKey::Space:        L_RET_CONST_STRING(L"Space")
    case LKey::Escape:       L_RET_CONST_STRING(L"Esc")
    case LKey::Tab:          L_RET_CONST_STRING(L"Tab")
    case LKey::Backtab:      L_RET_CONST_STRING(L"Backtab")
    case LKey::Backspace:    L_RET_CONST_STRING(L"Backspace")
    case LKey::Return:       L_RET_CONST_STRING(L"Return")
    case LKey::Enter:        L_RET_CONST_STRING(L"Enter")
    case LKey::Insert:       L_RET_CONST_STRING(L"Ins")
    case LKey::Delete:       L_RET_CONST_STRING(L"Del")
    case LKey::Pause:        L_RET_CONST_STRING(L"Pause")
    case LKey::Print:        L_RET_CONST_STRING(L"Print")
    case LKey::SysReq:       L_RET_CONST_STRING(L"SysReq")
    case LKey::Home:         L_RET_CONST_STRING(L"Home")
    case LKey::End:          L_RET_CONST_STRING(L"End")
    case LKey::Left:         L_RET_CONST_STRING(L"Left")
    case LKey::Up:           L_RET_CONST_STRING(L"Up")
    case LKey::Right:        L_RET_CONST_STRING(L"Right")
    case LKey::Down:         L_RET_CONST_STRING(L"Down")
    case LKey::PageUp:       L_RET_CONST_STRING(L"PgUp")
    case LKey::PageDown:     L_RET_CONST_STRING(L"PgDown")
    case LKey::CapsLock:     L_RET_CONST_STRING(L"CapsLock")
    case LKey::NumLock:      L_RET_CONST_STRING(L"NumLock")
    case LKey::ScrollLock:   L_RET_CONST_STRING(L"ScrollLock")
    case LKey::Menu:         L_RET_CONST_STRING(L"Menu")
    case LKey::Help:         L_RET_CONST_STRING(L"Help")
    case LKey::Back:                       L_RET_CONST_STRING(L"Back")
    case LKey::Forward:                    L_RET_CONST_STRING(L"Forward")
    case LKey::Stop:                       L_RET_CONST_STRING(L"Stop")
    case LKey::Refresh:                    L_RET_CONST_STRING(L"Refresh")
    case LKey::VolumeDown:                 L_RET_CONST_STRING(L"Volume Down")
    case LKey::VolumeMute:                 L_RET_CONST_STRING(L"Volume Mute")
    case LKey::VolumeUp:                   L_RET_CONST_STRING(L"Volume Up")
    case LKey::BassBoost:                  L_RET_CONST_STRING(L"Bass Boost")
    case LKey::BassUp:                     L_RET_CONST_STRING(L"Bass Up")
    case LKey::BassDown:                   L_RET_CONST_STRING(L"Bass Down")
    case LKey::TrebleUp:                   L_RET_CONST_STRING(L"Treble Up")
    case LKey::TrebleDown:                 L_RET_CONST_STRING(L"Treble Down")
    case LKey::MediaPlay:                  L_RET_CONST_STRING(L"Media Play")
    case LKey::MediaStop:                  L_RET_CONST_STRING(L"Media Stop")
    case LKey::MediaPrevious:              L_RET_CONST_STRING(L"Media Previous")
    case LKey::MediaNext:                  L_RET_CONST_STRING(L"Media Next")
    case LKey::MediaRecord:                L_RET_CONST_STRING(L"Media Record")
    case LKey::MediaPause:                 L_RET_CONST_STRING(L"Media Pause")
    case LKey::MediaTogglePlayPause:       L_RET_CONST_STRING(L"Toggle Media Play/Pause")
    case LKey::HomePage:                   L_RET_CONST_STRING(L"Home Page")
    case LKey::Favorites:                  L_RET_CONST_STRING(L"Favorites")
    case LKey::Search:                     L_RET_CONST_STRING(L"Search")
    case LKey::Standby:                    L_RET_CONST_STRING(L"Standby")
    case LKey::OpenUrl:                    L_RET_CONST_STRING(L"Open UR_RET_CONST_STRING(L")
    case LKey::LaunchMail:                 L_RET_CONST_STRING(L"Launch Mail")
    case LKey::LaunchMedia:                L_RET_CONST_STRING(L"Launch Media")
    case LKey::Launch0:                    L_RET_CONST_STRING(L"Launch (0)")
    case LKey::Launch1:                    L_RET_CONST_STRING(L"Launch (1)")
    case LKey::Launch2:                    L_RET_CONST_STRING(L"Launch (2)")
    case LKey::Launch3:                    L_RET_CONST_STRING(L"Launch (3)")
    case LKey::Launch4:                    L_RET_CONST_STRING(L"Launch (4)")
    case LKey::Launch5:                    L_RET_CONST_STRING(L"Launch (5)")
    case LKey::Launch6:                    L_RET_CONST_STRING(L"Launch (6)")
    case LKey::Launch7:                    L_RET_CONST_STRING(L"Launch (7)")
    case LKey::Launch8:                    L_RET_CONST_STRING(L"Launch (8)")
    case LKey::Launch9:                    L_RET_CONST_STRING(L"Launch (9)")
    case LKey::LaunchA:                    L_RET_CONST_STRING(L"Launch (A)")
    case LKey::LaunchB:                    L_RET_CONST_STRING(L"Launch (B)")
    case LKey::LaunchC:                    L_RET_CONST_STRING(L"Launch (C)")
    case LKey::LaunchD:                    L_RET_CONST_STRING(L"Launch (D)")
    case LKey::LaunchE:                    L_RET_CONST_STRING(L"Launch (E)")
    case LKey::LaunchF:                    L_RET_CONST_STRING(L"Launch (F)")
    case LKey::LaunchG:                    L_RET_CONST_STRING(L"Launch (G)")
    case LKey::LaunchH:                    L_RET_CONST_STRING(L"Launch (H)")
    case LKey::MonBrightnessUp:            L_RET_CONST_STRING(L"Monitor Brightness Up")
    case LKey::MonBrightnessDown:          L_RET_CONST_STRING(L"Monitor Brightness Down")
    case LKey::KeyboardLightOnOff:         L_RET_CONST_STRING(L"Keyboard Light On/Off")
    case LKey::KeyboardBrightnessUp:       L_RET_CONST_STRING(L"Keyboard Brightness Up")
    case LKey::KeyboardBrightnessDown:     L_RET_CONST_STRING(L"Keyboard Brightness Down")
    case LKey::PowerOff:                   L_RET_CONST_STRING(L"Power Off")
    case LKey::WakeUp:                     L_RET_CONST_STRING(L"Wake Up")
    case LKey::Eject:                      L_RET_CONST_STRING(L"Eject")
    case LKey::ScreenSaver:                L_RET_CONST_STRING(L"Screensaver")
    case LKey::WWW:                        L_RET_CONST_STRING(L"WWW")
    case LKey::Sleep:                      L_RET_CONST_STRING(L"Sleep")
    case LKey::LightBulb:                  L_RET_CONST_STRING(L"LightBulb")
    case LKey::Shop:                       L_RET_CONST_STRING(L"Shop")
    case LKey::History:                    L_RET_CONST_STRING(L"History")
    case LKey::AddFavorite:                L_RET_CONST_STRING(L"Add Favorite")
    case LKey::HotLinks:                   L_RET_CONST_STRING(L"Hot Links")
    case LKey::BrightnessAdjust:           L_RET_CONST_STRING(L"Adjust Brightness")
    case LKey::Finance:                    L_RET_CONST_STRING(L"Finance")
    case LKey::Community:                  L_RET_CONST_STRING(L"Community")
    case LKey::AudioRewind:                L_RET_CONST_STRING(L"Media Rewind")
    case LKey::BackForward:                L_RET_CONST_STRING(L"Back Forward")
    case LKey::ApplicationLeft:            L_RET_CONST_STRING(L"Application Left")
    case LKey::ApplicationRight:           L_RET_CONST_STRING(L"Application Right")
    case LKey::Book:                       L_RET_CONST_STRING(L"Book")
    case LKey::CD:                         L_RET_CONST_STRING(L"CD")
    case LKey::Calculator:                 L_RET_CONST_STRING(L"Calculator")
    case LKey::Calendar:                   L_RET_CONST_STRING(L"Calendar")
    case LKey::Clear:                      L_RET_CONST_STRING(L"Clear")
    case LKey::ClearGrab:                  L_RET_CONST_STRING(L"Clear Grab")
    case LKey::Close:                      L_RET_CONST_STRING(L"Close")
    case LKey::ContrastAdjust:             L_RET_CONST_STRING(L"Adjust contrast")
    case LKey::Copy:                       L_RET_CONST_STRING(L"Copy")
    case LKey::Cut:                        L_RET_CONST_STRING(L"Cut")
    case LKey::Display:                    L_RET_CONST_STRING(L"Display")
    case LKey::DOS:                        L_RET_CONST_STRING(L"DOS")
    case LKey::Documents:                  L_RET_CONST_STRING(L"Documents")
    case LKey::Excel:                      L_RET_CONST_STRING(L"Spreadsheet")
    case LKey::Explorer:                   L_RET_CONST_STRING(L"Browser")
    case LKey::Game:                       L_RET_CONST_STRING(L"Game")
    case LKey::Go:                         L_RET_CONST_STRING(L"Go")
    case LKey::iTouch:                     L_RET_CONST_STRING(L"iTouch")
    case LKey::LogOff:                     L_RET_CONST_STRING(L"Logoff")
    case LKey::Market:                     L_RET_CONST_STRING(L"Market")
    case LKey::Meeting:                    L_RET_CONST_STRING(L"Meeting")
    case LKey::Memo:                       L_RET_CONST_STRING(L"Memo")
    case LKey::MenuKB:                     L_RET_CONST_STRING(L"Keyboard Menu")
    case LKey::MenuPB:                     L_RET_CONST_STRING(L"Menu PB")
    case LKey::MySites:                    L_RET_CONST_STRING(L"My Sites")
    case LKey::News:                       L_RET_CONST_STRING(L"News")
    case LKey::OfficeHome:                 L_RET_CONST_STRING(L"Home Office")
    case LKey::Option:                     L_RET_CONST_STRING(L"Option")
    case LKey::Paste:                      L_RET_CONST_STRING(L"Paste")
    case LKey::Phone:                      L_RET_CONST_STRING(L"Phone")
    case LKey::Reply:                      L_RET_CONST_STRING(L"Reply")
    case LKey::Reload:                     L_RET_CONST_STRING(L"Reload")
    case LKey::RotateWindows:              L_RET_CONST_STRING(L"Rotate Windows")
    case LKey::RotationPB:                 L_RET_CONST_STRING(L"Rotation PB")
    case LKey::RotationKB:                 L_RET_CONST_STRING(L"Rotation KB")
    case LKey::Save:                       L_RET_CONST_STRING(L"Save")
    case LKey::Send:                       L_RET_CONST_STRING(L"Send")
    case LKey::Spell:                      L_RET_CONST_STRING(L"Spellchecker")
    case LKey::SplitScreen:                L_RET_CONST_STRING(L"Split Screen")
    case LKey::Support:                    L_RET_CONST_STRING(L"Support")
    case LKey::TaskPane:                   L_RET_CONST_STRING(L"Task Panel")
    case LKey::Terminal:                   L_RET_CONST_STRING(L"Terminal")
    case LKey::ToDoList:                   L_RET_CONST_STRING(L"To-do list")
    case LKey::Tools:                      L_RET_CONST_STRING(L"Tools")
    case LKey::Travel:                     L_RET_CONST_STRING(L"Travel")
    case LKey::Video:                      L_RET_CONST_STRING(L"Video")
    case LKey::Word:                       L_RET_CONST_STRING(L"Word Processor")
    case LKey::Xfer:                       L_RET_CONST_STRING(L"XFer")
    case LKey::ZoomIn:                     L_RET_CONST_STRING(L"Zoom In")
    case LKey::ZoomOut:                    L_RET_CONST_STRING(L"Zoom Out")
    case LKey::Away:                       L_RET_CONST_STRING(L"Away")
    case LKey::Messenger:                  L_RET_CONST_STRING(L"Messenger")
    case LKey::WebCam:                     L_RET_CONST_STRING(L"WebCam")
    case LKey::MailForward:                L_RET_CONST_STRING(L"Mail Forward")
    case LKey::Pictures:                   L_RET_CONST_STRING(L"Pictures")
    case LKey::Music:                      L_RET_CONST_STRING(L"Music")
    case LKey::Battery:                    L_RET_CONST_STRING(L"Battery")
    case LKey::Bluetooth:                  L_RET_CONST_STRING(L"Bluetooth")
    case LKey::WLAN:                       L_RET_CONST_STRING(L"Wireless")
    case LKey::UWB:                        L_RET_CONST_STRING(L"Ultra Wide Band")
    case LKey::AudioForward:               L_RET_CONST_STRING(L"Media Fast Forward")
    case LKey::AudioRepeat:                L_RET_CONST_STRING(L"Audio Repeat")
    case LKey::AudioRandomPlay:            L_RET_CONST_STRING(L"Audio Random Play")
    case LKey::Subtitle:                   L_RET_CONST_STRING(L"Subtitle")
    case LKey::AudioCycleTrack:            L_RET_CONST_STRING(L"Audio Cycle Track")
    case LKey::Time:                       L_RET_CONST_STRING(L"Time")
    case LKey::Hibernate:                  L_RET_CONST_STRING(L"Hibernate")
    case LKey::View:                       L_RET_CONST_STRING(L"View")
    case LKey::TopMenu:                    L_RET_CONST_STRING(L"Top Menu")
    case LKey::PowerDown:                  L_RET_CONST_STRING(L"Power Down")
    case LKey::Suspend:                    L_RET_CONST_STRING(L"Suspend")

    case LKey::MicMute:                    L_RET_CONST_STRING(L"Microphone Mute")

    case LKey::Red:                        L_RET_CONST_STRING(L"Red")
    case LKey::Green:                      L_RET_CONST_STRING(L"Green")
    case LKey::Yellow:                     L_RET_CONST_STRING(L"Yellow")
    case LKey::Blue:                       L_RET_CONST_STRING(L"Blue")

    case LKey::ChannelUp:                  L_RET_CONST_STRING(L"Channel Up")
    case LKey::ChannelDown:                L_RET_CONST_STRING(L"Channel Down")

    case LKey::Guide:                      L_RET_CONST_STRING(L"Guide")
    case LKey::Info:                       L_RET_CONST_STRING(L"Info")
    case LKey::Settings:                   L_RET_CONST_STRING(L"Settings")

    case LKey::MicVolumeUp:                L_RET_CONST_STRING(L"Microphone Volume Up")
    case LKey::MicVolumeDown:              L_RET_CONST_STRING(L"Microphone Volume Down")

    case LKey::New:                        L_RET_CONST_STRING(L"New")
    case LKey::Open:                       L_RET_CONST_STRING(L"Open")
    case LKey::Find:                       L_RET_CONST_STRING(L"Find")
    case LKey::Undo:                       L_RET_CONST_STRING(L"Undo")
    case LKey::Redo:                       L_RET_CONST_STRING(L"Redo")
    // --------------------------------------------------------------
    // Keypad navigation keys
    case LKey::Select:       L_RET_CONST_STRING(L"Select")
    case LKey::Yes:          L_RET_CONST_STRING(L"Yes")
    case LKey::No:           L_RET_CONST_STRING(L"No")

    // --------------------------------------------------------------
    // Device keys
    case LKey::Context1:         L_RET_CONST_STRING(L"Context1")
    case LKey::Context2:         L_RET_CONST_STRING(L"Context2")
    case LKey::Context3:         L_RET_CONST_STRING(L"Context3")
    case LKey::Context4:         L_RET_CONST_STRING(L"Context4")
    //: Button to start a call (note: a separate button is used to end the call;
    case LKey::Call:             L_RET_CONST_STRING(L"Call")
    //: Button to end a call (note: a separate button is used to start the call;
    case LKey::Hangup:           L_RET_CONST_STRING(L"Hangup")
    //: Button that will hang up if we're in call: or make a call if we're not.
    case LKey::ToggleCallHangup: L_RET_CONST_STRING(L"Toggle Call/Hangup")
    case LKey::Flip:             L_RET_CONST_STRING(L"Flip")
    //: Button to trigger voice dialing
    case LKey::VoiceDial:        L_RET_CONST_STRING(L"Voice Dial")
    //: Button to redial the last number called
    case LKey::LastNumberRedial: L_RET_CONST_STRING(L"Last Number Redial")
    //: Button to trigger the camera shutter (take a picture;
    case LKey::Camera:           L_RET_CONST_STRING(L"Camera Shutter")
    //: Button to focus the camera
    case LKey::CameraFocus:      L_RET_CONST_STRING(L"Camera Focus")

    // --------------------------------------------------------------
    // Japanese keyboard support
    case LKey::Kanji:            L_RET_CONST_STRING(L"Kanji")
    case LKey::Muhenkan:         L_RET_CONST_STRING(L"Muhenkan")
    case LKey::Henkan:           L_RET_CONST_STRING(L"Henkan")
    case LKey::Romaji:           L_RET_CONST_STRING(L"Romaji")
    case LKey::Hiragana:         L_RET_CONST_STRING(L"Hiragana")
    case LKey::Katakana:         L_RET_CONST_STRING(L"Katakana")
    case LKey::Hiragana_Katakana:L_RET_CONST_STRING(L"Hiragana Katakana")
    case LKey::Zenkaku:          L_RET_CONST_STRING(L"Zenkaku")
    case LKey::Hankaku:          L_RET_CONST_STRING(L"Hankaku")
    case LKey::Zenkaku_Hankaku:  L_RET_CONST_STRING(L"Zenkaku Hankaku")
    case LKey::Touroku:          L_RET_CONST_STRING(L"Touroku")
    case LKey::Massyo:           L_RET_CONST_STRING(L"Massyo")
    case LKey::Kana_Lock:        L_RET_CONST_STRING(L"Kana Lock")
    case LKey::Kana_Shift:       L_RET_CONST_STRING(L"Kana Shift")
    case LKey::Eisu_Shift:       L_RET_CONST_STRING(L"Eisu Shift")
    case LKey::Eisu_toggle:      L_RET_CONST_STRING(L"Eisu toggle")
    case LKey::Codeinput:        L_RET_CONST_STRING(L"Code input")
    case LKey::MultipleCandidate:L_RET_CONST_STRING(L"Multiple Candidate")
    case LKey::PreviousCandidate:L_RET_CONST_STRING(L"Previous Candidate")

    // --------------------------------------------------------------
    // Korean keyboard support
    case LKey::Hangul:          L_RET_CONST_STRING(L"Hangul")
    case LKey::Hangul_Start:    L_RET_CONST_STRING(L"Hangul Start")
    case LKey::Hangul_End:      L_RET_CONST_STRING(L"Hangul End")
    case LKey::Hangul_Hanja:    L_RET_CONST_STRING(L"Hangul Hanja")
    case LKey::Hangul_Jamo:     L_RET_CONST_STRING(L"Hangul Jamo")
    case LKey::Hangul_Romaja:   L_RET_CONST_STRING(L"Hangul Romaja")
    case LKey::Hangul_Jeonja:   L_RET_CONST_STRING(L"Hangul Jeonja")
    case LKey::Hangul_Banja:    L_RET_CONST_STRING(L"Hangul Banja")
    case LKey::Hangul_PreHanja: L_RET_CONST_STRING(L"Hangul PreHanja")
    case LKey::Hangul_PostHanja:L_RET_CONST_STRING(L"Hangul PostHanja")
    case LKey::Hangul_Special:  L_RET_CONST_STRING(L"Hangul Special")

    // --------------------------------------------------------------
    // Miscellaneous keys
    case LKey::Cancel:  L_RET_CONST_STRING(L"Cancel")
    case LKey::Printer:  L_RET_CONST_STRING(L"Printer")
    case LKey::Execute:  L_RET_CONST_STRING(L"Execute")
    case LKey::Play:  L_RET_CONST_STRING(L"Play")
    case LKey::Zoom:  L_RET_CONST_STRING(L"Zoom")
    case LKey::Exit:  L_RET_CONST_STRING(L"Exit")
    case LKey::TouchpadToggle:  L_RET_CONST_STRING(L"Touchpad Toggle")
    case LKey::TouchpadOn:  L_RET_CONST_STRING(L"Touchpad On")
    case LKey::TouchpadOff:  L_RET_CONST_STRING(L"Touchpad Off")
    default:    L_RET_CONST_STRING(L"")
    }
}


LK_NAMESPACE_END
