#include "lshortcutman.h"
#include "lguiapplication.h"
#include <algorithm>

LK_NAMESPACE_BEGIN

bool LShortcutEntry::correctContext() const
{
    std::shared_ptr<LObject> t= m_owner.lock();
    if(m_contextMatcher == nullptr){
        return t != nullptr;
    }
    return m_contextMatcher(t, m_context);
}



LShortcutMan::LShortcutMan()
{

}

LKeySequence::SequenceMatch LShortcutMan::state()
{
    return m_currentState;
}

bool LShortcutMan::tryShortcut(std::shared_ptr<LKeyEvent> e)
{
    if (e->key() == (uint32_t)LKey::Unknown){
        return false;
    }

    sortSequences();
    LKeySequence::SequenceMatch previousState = m_currentState;

    switch (nextState(e)) {
    case LKeySequence::NoMatch:{
        // In the case of going from a partial match to no match we handled the
        // event, since we already stated that we did for the partial match. But
        // in the normal case of directly going to no match we say we didn't.
        return previousState == LKeySequence::PartialMatch;
    }
    case LKeySequence::PartialMatch:{
        // For a partial match we don't know yet if we will handle the shortcut
        // but we need to say we did, so that we get the follow-up key-presses.
        return true;
    }
    case LKeySequence::ExactMatch: {
        // Save number of identical matches before dispatching
        // to keep QShortcutMap and tryShortcut reentrant.
        const int32_t identicalMatches = m_identicals.size();
        resetState();
        dispatchEvent(e);
        // If there are no identicals we've only found disabled shortcuts, and
        // shouldn't say that we handled the event.
        return identicalMatches > 0;
    }
    default:{
        return false;
    }
    }
}

std::shared_ptr<LShortcutEntry> LShortcutMan::shortcutForKeySequence(const LKeySequence &seq) const
{
    std::vector<std::shared_ptr<LShortcutEntry> >::const_iterator itEnd = m_sequences.cend();
    std::vector<std::shared_ptr<LShortcutEntry> >::const_iterator it;
    if(m_isSorted){
        auto cmpz = [](std::shared_ptr<LShortcutEntry> x, LKeySequence y) { return x->m_keyseq < y; };
        it = std::lower_bound(m_sequences.cbegin(), itEnd, seq, cmpz);
    }else{
        it = m_sequences.cbegin();
    }

    for(; it != itEnd; ++it){
        std::shared_ptr<LShortcutEntry> entry = *it;
        if(seq.matches(entry->m_keyseq) == LKeySequence::ExactMatch
            && entry->correctContext()
            && entry->m_enabled){
            return entry;
        }
    }
    return nullptr;
}

std::shared_ptr<LShortcutEntry> LShortcutMan::shortcutForKeySequence(std::shared_ptr<LObject> owner,
                                                                     const LKeySequence &seq) const
{
    std::vector<std::shared_ptr<LShortcutEntry> >::const_iterator itEnd = m_sequences.cend();
    std::vector<std::shared_ptr<LShortcutEntry> >::const_iterator it;
    if(m_isSorted){
        auto cmpz = [](std::shared_ptr<LShortcutEntry> x, LKeySequence y) { return x->m_keyseq < y; };
        it = std::lower_bound(m_sequences.cbegin(), itEnd, seq, cmpz);
    }else{
        it = m_sequences.cbegin();
    }

    for(; it != itEnd; ++it){
        std::shared_ptr<LShortcutEntry> entry = *it;
        if(entry->m_owner.lock() == owner
            && seq.matches(entry->m_keyseq) == LKeySequence::ExactMatch
            && entry->correctContext()
            && entry->m_enabled){
            return entry;
        }
    }
    return nullptr;
}

std::shared_ptr<LShortcutEntry> LShortcutMan::addShortcut(std::shared_ptr<LObject> owner,
                                                          const LKeySequence &key,
                                                          LShortcutCtxType ctxType,
                                                          uint32_t funID,
                                                          LShortcutEntry::ContextMatcher matcher)
{
    if(owner != nullptr){
        std::shared_ptr<LShortcutEntry> t = shortcutForKeySequence(owner, key);
        if(t != nullptr){
            if(t->m_id == funID){
                t->m_context = ctxType;
                t->m_contextMatcher = matcher;
                return t;
            }else{
                lWarn() << "Cannot add short cut sequence which already used by another function!";
                return nullptr;
            }
        }


        t = shortCut(owner, funID);
        if(t != nullptr){
            t->m_keyseq = key;
            t->m_context = ctxType;
            t->m_contextMatcher = matcher;
            m_isSorted = false;
            return t;
        }else{
            t = std::make_shared<LShortcutEntry>(owner, key, ctxType, funID, true, matcher);
            m_sequences.push_back(t);
            m_isSorted = false;
            return t;
        }
    }else{
        lWarn() << "Cannot add short cut sequence without owner!";
    }
    return nullptr;
}

bool LShortcutMan::addShortcut(std::shared_ptr<LShortcutEntry> in)
{
    if(in != nullptr){
        std::shared_ptr<LObject> owner = in->m_owner.lock();
        if(owner != nullptr){
            std::shared_ptr<LShortcutEntry> t = shortcutForKeySequence(owner, in->m_keyseq);
            if(t == in){
                return true;
            }

            if(t != nullptr){
                if(t->m_id != in->m_id){
                    lWarn() << "Cannot add short cut sequence which already used by another function!";
                    return false;
                }
            }

            for(uint32_t i=0; i<m_sequences.size(); ++i){
                const std::shared_ptr<LShortcutEntry> entry = m_sequences[i];
                if(entry->m_owner.lock() == owner && entry->m_id == in->m_id){
                    m_sequences[i] = in;
                    m_isSorted = false;
                    return true;
                }
            }

            m_sequences.push_back(in);
            m_isSorted = false;
            return true;
        }
    }
    return false;
}

void LShortcutMan::removeShortcut(const std::shared_ptr<LShortcutEntry> entry)
{
    m_sequences.erase(remove(m_sequences.begin(), m_sequences.end(), entry), m_sequences.end());
}

void LShortcutMan::removeShortcut(const std::shared_ptr<LObject> owner, uint32_t funID)
{
    auto isShortCut = [owner, funID](std::shared_ptr<LShortcutEntry> entry) {
        return owner == entry->m_owner.lock() && funID == entry->m_id;
    };

    m_sequences.erase(std::remove_if(m_sequences.begin(),
                                     m_sequences.end(),
                                     isShortCut), m_sequences.end());

}

void LShortcutMan::removeShortcut(const std::shared_ptr<LObject> owner)
{
    auto isShortCut = [owner](std::shared_ptr<LShortcutEntry> entry) {
        return owner == entry->m_owner.lock();
    };

    m_sequences.erase(std::remove_if(m_sequences.begin(),
                                     m_sequences.end(),
                                     isShortCut), m_sequences.end());
}

std::shared_ptr<LShortcutEntry> LShortcutMan::shortCut(const std::shared_ptr<LObject> owner, uint32_t funID)
{
    for(const std::shared_ptr<LShortcutEntry> entry : m_sequences){
        if(entry->m_owner.lock() == owner && entry->m_id == funID){
            return entry;
        }
    }
    return nullptr;
}

void LShortcutMan::resetState()
{
    m_currentState = LKeySequence::NoMatch;
    clearSequence(m_currentSequences);
}

LKeySequence::SequenceMatch LShortcutMan::nextState(std::shared_ptr<LKeyEvent> e)
{
    if (e->key() >= (uint32_t)LKey::Shift && e->key() <= (uint32_t)LKey::Alt){
        return m_currentState;
    }

    LKeySequence::SequenceMatch result = LKeySequence::NoMatch;

    // We start fresh each time..
    m_identicals.clear();

    result = find(e);
    if (result == LKeySequence::NoMatch && (e->modifiers() & LKeyModifier::Keypad)) {
        // Try to find a match without keypad modifier
        result = find(e, (uint32_t)LKeyModifier::Keypad);
    }
    if (result == LKeySequence::NoMatch && e->modifiers() & LKeyModifier::Shift) {
        // If Shift + Key_Backtab, also try Shift + LKey::Tab
        if (e->key() == (uint32_t)LKey::Backtab) {
            std::shared_ptr<LKeyEvent>  pe =
                std::make_shared<LKeyEvent>(e->type(), (uint32_t)LKey::Tab, e->modifiers(), e->text());
            result = find(pe);
        }
    }

    // Does the new state require us to clean up?
    if (result == LKeySequence::NoMatch){
        clearSequence(m_currentSequences);
    }
    m_currentState = result;
    return result;
}

void LShortcutMan::dispatchEvent(std::shared_ptr<LKeyEvent> e)
{
    if (!m_identicals.size()){
        return;
    }

    const LKeySequence &curKey = m_identicals.at(0)->m_keyseq;
    if (m_prevSequence != curKey) {
        m_ambigCount = 0;
        m_prevSequence = curKey;
    }
    // Find next
    std::shared_ptr<LShortcutEntry> current = nullptr, next = nullptr;
    uint32_t i = 0, enabledShortcuts = 0;

    while(i < m_identicals.size()) {
        current = m_identicals.at(i);
        if (current->m_enabled || next == nullptr){
            ++enabledShortcuts;
            if (enabledShortcuts > m_ambigCount + 1){
                break;
            }
            next = current;
        }
        ++i;
    }
    m_ambigCount = (m_identicals.size() == i ? 0 : m_ambigCount + 1);
    // Don't trigger shortcut if we're autorepeating and the shortcut is
    // grabbed with not accepting autorepeats.
    if (!next || (e->isAutoRepeat() && !next->m_autorepeat)){
        return;
    }

    std::shared_ptr<LShortcutEvent> se
        = std::make_shared<LShortcutEvent>(next->m_keyseq, next->m_id, enabledShortcuts>1);
    LCoreApplication::sendEvent(next->m_owner.lock(), se);
}

void LShortcutMan::clearSequence(std::vector<LKeySequence> &ksl)
{
    ksl.clear();
    m_newEntries.clear();
}

LKeySequence::SequenceMatch LShortcutMan::find(std::shared_ptr<LKeyEvent> e, uint32_t ignoredModifiers)
{
    if (m_sequences.empty()){
        return LKeySequence::NoMatch;
    }

    createNewSequences(e, m_newEntries, ignoredModifiers);
    // Looking for new identicals, scrap old
    m_identicals.clear();

    bool partialFound = false;
    bool identicalDisabledFound = false;
    std::vector<LKeySequence> okEntries;
    uint32_t result = (uint32_t)LKeySequence::NoMatch;
    auto cmpz = [](std::shared_ptr<LShortcutEntry> x, LKeySequence y) { return x->m_keyseq < y; };

    for (int i = m_newEntries.size()-1; i >= 0 ; --i) {
        const LKeySequence& entry = m_newEntries[i];
        std::vector<std::shared_ptr<LShortcutEntry> >::const_iterator itEnd = m_sequences.cend();
        std::vector<std::shared_ptr<LShortcutEntry> >::const_iterator it =
            m_isSorted? std::lower_bound(m_sequences.cbegin(), itEnd, entry, cmpz) : m_sequences.cbegin();

        uint32_t oneKSResult = (uint32_t)LKeySequence::NoMatch;
        uint32_t tempRes = (uint32_t)LKeySequence::NoMatch;
        do {
            if (it == itEnd){
                break;
            }
            std::shared_ptr<LShortcutEntry> p = *it;

            tempRes = entry.matches(p->m_keyseq);
            oneKSResult = std::max<uint32_t>(oneKSResult, tempRes);
            if (tempRes != LKeySequence::NoMatch && p->correctContext()) {
                if (tempRes == LKeySequence::ExactMatch) {
                    if (p->m_enabled){
                        m_identicals.push_back(p);
                    }
                    else{
                        identicalDisabledFound = true;
                    }
                } else if (tempRes == LKeySequence::PartialMatch) {
                    // We don't need partials, if we have identicals
                    if (m_identicals.size()){
                        break;
                    }
                    // We only care about enabled partials, so we don't consume
                    // key events when all partials are disabled!
                    partialFound |= p->m_enabled;
                }
            }
            ++it;
            // If we got a valid match on this run, there might still be more keys to check against,
            // so we'll loop once more. If we get NoMatch, there's guaranteed no more possible
            // matches in the shortcutmap.
        } while (tempRes != LKeySequence::NoMatch);

        // If the type of match improves (ergo, NoMatch->Partial, or Partial->Exact), clear the
        // previous list. If this match is equal or better than the last match, append to the list
        if (oneKSResult > result) {
            okEntries.clear();
        }
        if (oneKSResult && oneKSResult >= result) {
            okEntries.push_back(m_newEntries.at(i));
        }
    }

    if (m_identicals.size()) {
        result = LKeySequence::ExactMatch;
    } else if (partialFound) {
        result = LKeySequence::PartialMatch;
    } else if (identicalDisabledFound) {
        result = LKeySequence::ExactMatch;
    } else {
        clearSequence(m_currentSequences);
        result = LKeySequence::NoMatch;
    }
    if (result != LKeySequence::NoMatch){
        m_currentSequences = okEntries;
    }

    return LKeySequence::SequenceMatch(result);
}

void LShortcutMan::createNewSequences(std::shared_ptr<LKeyEvent> e,
                                      std::vector<LKeySequence> &ksl,
                                      uint32_t ignoredModifiers)
{
    std::list<uint32_t> possibleKeys = LGuiApplication::possibleKeys(e);
    uint32_t pkTotal = possibleKeys.size();
    if (pkTotal == 0){
        return;
    }

    uint32_t ssActual = m_currentSequences.size();
    uint32_t ssTotal = std::max<uint32_t>(1, ssActual);
    // Resize to possible permutations of the current sequence(s).
    ksl.resize(pkTotal * ssTotal);

    uint32_t index = ssActual ? m_currentSequences.at(0).size() : 0;
    uint32_t k = 0;
    for (const uint32_t pkKey : possibleKeys ) {
        for (uint32_t ssNum = 0; ssNum < ssTotal; ++ssNum) {
            LKeySequence &curKsl = ksl[k+ssNum];
            if (ssActual) {
                curKsl = m_currentSequences.at(ssNum);
            } else {
                curKsl.fill(0);
            }
            curKsl[index] = pkKey & ~ignoredModifiers;
        }
        k += ssTotal;
    }
}

void LShortcutMan::sortSequences()
{
    if(m_isSorted){
        std::sort(m_sequences.begin(), m_sequences.end(),
                  [](const std::shared_ptr<LShortcutEntry>& a, const std::shared_ptr<LShortcutEntry>& b)
                  {
                      return a->m_keyseq < b->m_keyseq;
                  });

        m_isSorted = true;
    }
}


LK_NAMESPACE_END
