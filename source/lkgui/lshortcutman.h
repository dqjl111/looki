#ifndef LSHORTCUTMAN_H
#define LSHORTCUTMAN_H
#include "lkeysequence.h"
#include "lguievent.h"
#include "lkcore/lobject.h"

LK_NAMESPACE_BEGIN
class LKGUIDEF LShortcutEntry
{
public:
    typedef bool (*ContextMatcher)(std::shared_ptr<LObject> object, LShortcutCtxType context);

    LShortcutEntry()
        : m_keyseq(0), m_context(LShortcutCtxType::Window), m_enabled(false), m_autorepeat(1), m_id(0), m_contextMatcher(nullptr)
    {}

    LShortcutEntry(const LKeySequence &k)
        : m_keyseq(k), m_context(LShortcutCtxType::Window), m_enabled(false), m_autorepeat(1), m_id(0), m_contextMatcher(nullptr)
    {}

    LShortcutEntry(std::shared_ptr<LObject> o, const LKeySequence &k, LShortcutCtxType c,
                   uint32_t i, bool a, ContextMatcher m)
        : m_keyseq(k), m_context(c), m_enabled(true), m_id(i), m_autorepeat(a), m_contextMatcher(m)
    {
        m_owner = o;
    }

    bool correctContext() const;

    LKeySequence m_keyseq;
    LShortcutCtxType m_context;
    bool m_enabled : 1;
    bool m_autorepeat : 1;
    uint32_t m_id; //快捷键功能ID
    std::weak_ptr<LObject> m_owner;
    ContextMatcher m_contextMatcher;
};


class LKGUIDEF LShortcutMan
{
public:
    LShortcutMan();

    LKeySequence::SequenceMatch state();

    bool tryShortcut(std::shared_ptr<LKeyEvent> e);

    std::shared_ptr<LShortcutEntry> shortcutForKeySequence(const LKeySequence &seq) const;
    std::shared_ptr<LShortcutEntry> shortcutForKeySequence(std::shared_ptr<LObject> owner,
                                                           const LKeySequence &seq) const;

    inline bool hasShortcutForKeySequence(const LKeySequence &seq) const{
        const_cast<LShortcutMan*>(this)->sortSequences();
        return shortcutForKeySequence(seq) != nullptr;
    }

    inline bool hasShortcutForKeySequence(std::shared_ptr<LObject> owner,
                                          const LKeySequence &seq) const{
        return shortcutForKeySequence(owner, seq) != nullptr;
    }


    std::shared_ptr<LShortcutEntry> addShortcut(std::shared_ptr<LObject> owner,
                                                const LKeySequence &key,
                                                LShortcutCtxType ctxType,
                                                uint32_t funID,
                                                LShortcutEntry::ContextMatcher matcher);

    bool addShortcut(std::shared_ptr<LShortcutEntry> entry);
    void removeShortcut(const std::shared_ptr<LShortcutEntry> entry);
    void removeShortcut(const std::shared_ptr<LObject> owner, uint32_t funID);
    void removeShortcut(const std::shared_ptr<LObject> owner = nullptr);

    std::shared_ptr<LShortcutEntry> shortCut(const std::shared_ptr<LObject> owner, uint32_t funID);
    void sortSequences(); //由小到大排列
    inline void setSorted(bool isSort){
        m_isSorted = isSort;
    }
protected:
    std::vector<std::shared_ptr<LShortcutEntry> > m_sequences;          // All sequences!

    int m_currentId;                              // Global shortcut ID number
    int m_ambigCount;                             // Index of last enabled ambiguous dispatch
    LKeySequence::SequenceMatch m_currentState;
    std::vector<LKeySequence> m_currentSequences;     // Sequence for the current state
    std::vector<LKeySequence> m_newEntries;
    LKeySequence m_prevSequence;                  // Sequence for the previous identical match
    std::vector<std::shared_ptr<LShortcutEntry> > m_identicals;  // Last identical matches
    bool m_isSorted = false;

    void resetState();
    LKeySequence::SequenceMatch nextState(std::shared_ptr<LKeyEvent> e);
    void dispatchEvent(std::shared_ptr<LKeyEvent> e);

    void clearSequence(std::vector<LKeySequence> &ksl);
    LKeySequence::SequenceMatch find(std::shared_ptr<LKeyEvent> e, uint32_t ignoredModifiers = 0);
    void createNewSequences(std::shared_ptr<LKeyEvent> e,
                            std::vector<LKeySequence> &ksl,
                            uint32_t ignoredModifiers);



};
LK_NAMESPACE_END

#endif // LSHORTCUTMAN_H
