#ifndef LKEYSEQUENCE_H
#define LKEYSEQUENCE_H
#include "lkcore/lookiconfig.h"
#include "lkcore/lstringutil.h"
#include "lkcore/lcoreenums.h"
#include <array>
#include <map>

LK_NAMESPACE_BEGIN
class LKGUIDEF LKeySequence : public std::array<uint32_t, 4>
{
public:
    enum StandardKey {
        UnknownKey,
        HelpContents,
        WhatsThis,
        Open,
        Close,
        Save,
        New,
        Delete,
        Cut,
        Copy,
        Paste,
        Undo,
        Redo,
        Back,
        Forward,
        Refresh,
        ZoomIn,
        ZoomOut,
        Print,
        AddTab,
        NextChild,
        PreviousChild,
        Find,
        FindNext,
        FindPrevious,
        Replace,
        SelectAll,
        Bold,
        Italic,
        Underline,
        MoveToNextChar,
        MoveToPreviousChar,
        MoveToNextWord,
        MoveToPreviousWord,
        MoveToNextLine,
        MoveToPreviousLine,
        MoveToNextPage,
        MoveToPreviousPage,
        MoveToStartOfLine,
        MoveToEndOfLine,
        MoveToStartOfBlock,
        MoveToEndOfBlock,
        MoveToStartOfDocument,
        MoveToEndOfDocument,
        SelectNextChar,
        SelectPreviousChar,
        SelectNextWord,
        SelectPreviousWord,
        SelectNextLine,
        SelectPreviousLine,
        SelectNextPage,
        SelectPreviousPage,
        SelectStartOfLine,
        SelectEndOfLine,
        SelectStartOfBlock,
        SelectEndOfBlock,
        SelectStartOfDocument,
        SelectEndOfDocument,
        DeleteStartOfWord,
        DeleteEndOfWord,
        DeleteEndOfLine,
        InsertParagraphSeparator,
        InsertLineSeparator,
        SaveAs,
        Preferences,
        Quit,
        FullScreen,
        Deselect,
        DeleteCompleteLine,
        Backspace,
        Cancel
    };

    enum SequenceMatch {
        NoMatch,
        PartialMatch,
        ExactMatch
    };


    LKeySequence();
    LKeySequence(uint32_t v0, uint32_t v1=0, uint32_t v2=0, uint32_t v3=0);
    LKeySequence(const LKeySequence &ks);
    LKeySequence(const StandardKey &key);

    static constexpr uint32_t maxSize(){
        return sizeof(LKeySequence)/sizeof(uint32_t);
    }


    uint32_t count() const;
    bool isValid() const;
    bool isEmpty() const;

    SequenceMatch matches(const LKeySequence &seq) const;

    std::wstring toString() const;
    bool operator< (const LKeySequence &ks) const;
    bool operator==(const LKeySequence &other) const;
    inline bool operator!= (const LKeySequence &other) const
    { return !(*this == other); }
    inline bool operator> (const LKeySequence &other) const
    { return other < *this; }
    inline bool operator<= (const LKeySequence &other) const
    { return !(other < *this); }
    inline bool operator>= (const LKeySequence &other) const
    { return !(*this < other); }

    inline LKeySequence& operator=(const LKeySequence &ks){
        for(uint32_t i=0 ; i<size(); ++i){
            _Elems[i] = ks._Elems[i];
        }
        return *this;
    }

    inline LKeySequence&operator=(LKeySequence &&other) noexcept { swap(other); return *this; }

    static const std::list<LKeySequence> keyBindings(StandardKey key);
    static const std::wstring encodeString(uint32_t key);

    static const std::map<uint32_t, StandardKey>& innnerBindings();
    static void setInnerBindings(const std::map<uint32_t, StandardKey>& bindings);
    static const std::wstring keyName(uint32_t key);
protected:    
    static const std::wstring& keyNameHelper(const uint32_t& key);
    static void initBindings();
    static bool m_bindingInit;
    static std::map<uint32_t, StandardKey> m_innerBindings;
    static void buildKeyBindings();
};




LK_NAMESPACE_END

#endif // LKEYSEQUENCE_H
