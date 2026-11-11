#include "ltextengine.h"
#include "lkgui/paint/lpainter.h"
#include "lkcore/text/ltextboundaryfinder.h"
#include "lkcore/lstringutil.h"
#include "lkgui/paint/lpen.h"
#include "lkgui/text/lfontengine.h"
#include "lkgui/lguiapplication.h"
#include <iterator>
#include <algorithm>

LK_NAMESPACE_BEGIN
namespace {
static inline bool isRetainableControlCode(wchar_t c)
{
    return (c >= 0x202a && c <= 0x202e) // LRE, RLE, PDF, LRO, RLO
           || (c >= 0x200e && c <= 0x200f) // LRM, RLM
           || (c >= 0x2066 && c <= 0x2069); // LRI, RLI, FSI, PDI
}

static std::wstring stringMidRetainingBidiCC(const std::wstring &string,
                                        const std::wstring &ellidePrefix,
                                        const std::wstring &ellideSuffix,
                                        int subStringFrom,
                                        int subStringTo,
                                        int midStart,
                                        int midLength)
{
    std::wstring prefix;
    for (int i=subStringFrom; i<midStart; ++i) {
        wchar_t c = string.at(i);
        if (isRetainableControlCode(c)){
            prefix += c;
        }
    }

    std::wstring suffix;
    for (int i=midStart + midLength; i<subStringTo; ++i) {
        wchar_t c = string.at(i);
        if (isRetainableControlCode(c)){
            suffix += c;
        }
    }

    return prefix + ellidePrefix + string.substr(midStart, midLength) + ellideSuffix + suffix;
}

/* These two helper functions are used to determine whether we need to insert a ZWJ character
   between the text that gets truncated and the ellipsis. This is important to get
   correctly shaped results for arabic text.
*/
static inline bool nextCharJoins(const std::wstring &string, int pos)
{
    while (pos < string.length() && LStringUtil::category(string.at(pos)) == lchar::Mark_NonSpacing){
        ++pos;
    }
    if (pos == string.length()){
        return false;
    }
    lchar::JoiningType joining = LStringUtil::joiningType(string.at(pos));
    return joining != lchar::Joining_None && joining != lchar::Joining_Transparent;
}

static inline bool prevCharJoins(const std::wstring &string, int pos)
{
    while (pos > 0 && LStringUtil::category(string.at(pos - 1)) == lchar::Mark_NonSpacing){
        --pos;
    }
    if (pos == 0){
        return false;
    }
    lchar::JoiningType joining = LStringUtil::joiningType(string.at(pos - 1));
    return joining == lchar::Joining_Dual || joining == lchar::Joining_Causing;
}


enum JustificationClass {
    Justification_Prohibited      = 0,   // Justification can not be applied after this glyph
    Justification_Arabic_Space    = 1,   // This glyph represents a space inside arabic text
    Justification_Character       = 2,   // Inter-character justification point follows this glyph
    Justification_Space           = 4,   // This glyph represents a blank outside an Arabic run
    Justification_Arabic_Normal   = 7,   // Normal Middle-Of-Word glyph that connects to the right (begin)
    Justification_Arabic_Waw      = 8,   // Next character is final form of Waw/Ain/Qaf/Feh
    Justification_Arabic_BaRa     = 9,   // Next two characters are Ba + Ra/Ya/AlefMaksura
    Justification_Arabic_Alef     = 10,  // Next character is final form of Alef/Tah/Lam/Kaf/Gaf
    Justification_Arabic_HahDal   = 11,  // Next character is final form of Hah/Dal/Teh Marbuta
    Justification_Arabic_Seen     = 12,  // Initial or medial form of Seen/Sad
    Justification_Arabic_Kashida  = 13   // User-inserted Kashida(U+0640)
};

// Helper class used in LTextEngine::itemize
// keep it out here to allow us to keep supporting various compilers.
class Itemizer {
public:
    Itemizer(const std::wstring &string, const LScriptAnalysis *analysis, std::vector<LScriptItem> &items)
        : m_string(string),
        m_analysis(analysis),
        m_items(items)
    {
    }

    /// generate the script items
    /// The caps parameter is used to choose the algoritm of splitting text and assiging roles to the textitems
    void generate(int start, int length, LFont::Capitalization caps)
    {
        if (caps == LFont::SmallCaps){
            generateScriptItemsSmallCaps(m_string.data(), start, length);
        }else if(caps == LFont::Capitalize){
            generateScriptItemsCapitalize(start, length);
        }else if(caps != LFont::MixedCase) {
            generateScriptItemsAndChangeCase(start, length,
                                             caps == LFont::AllLowercase ? LScriptAnalysis::Lowercase : LScriptAnalysis::Uppercase);
        }
        else{
            generateScriptItems(start, length);
        }
    }

private:
    enum { MaxItemLength = 4096 };

    void generateScriptItemsAndChangeCase(int start, int length, LScriptAnalysis::Flags flags)
    {
        generateScriptItems(start, length);
        if (m_items.empty()){ // the next loop won't work in that case
            return;
        }

        int pos = m_items.size();
        do{
            pos--;
            LScriptItem& item = m_items[pos];
            if(item.analysis.flags < LScriptAnalysis::LineOrParagraphSeparator){
                item.analysis.flags = flags;
            }
        }while(pos > start);
    }

    void generateScriptItems(int start, int length)
    {
        if (!length){
            return;
        }
        const int end = start + length;
        for (int i = start + 1; i < end; ++i) {
            if (m_analysis[i].bidiLevel == m_analysis[start].bidiLevel
                && m_analysis[i].flags == m_analysis[start].flags
                && (m_analysis[i].script == m_analysis[start].script || m_string[i] == wchar_t('.'))
                && m_analysis[i].flags < LScriptAnalysis::SpaceTabOrObject
                && i - start < MaxItemLength)
                continue;
            m_items.push_back(LScriptItem(start, m_analysis[start]));
            start = i;
        }
        m_items.push_back(LScriptItem(start, m_analysis[start]));
    }

    void generateScriptItemsCapitalize(int start, int length)
    {
        if (!length){
            return;
        }

        if (m_splitter == nullptr){
            m_splitter = std::make_shared<LTextBoundaryFinder>(LTextBoundaryFinder::Word, m_string);
        }

        m_splitter->setPosition(start);
        LScriptAnalysis itemAnalysis = m_analysis[start];

        if (m_splitter->boundaryReasons() & LTextBoundaryReason::StartOfItem)
            itemAnalysis.flags = LScriptAnalysis::Uppercase;

        m_splitter->toNextBoundary();

        const int end = start + length;
        for (int i = start + 1; i < end; ++i) {
            bool atWordStart = false;

            if (i == m_splitter->position()) {
                if (m_splitter->boundaryReasons() & LTextBoundaryReason::StartOfItem) {
                    atWordStart = true;
                }

                m_splitter->toNextBoundary();
            }

            if (m_analysis[i] == itemAnalysis
                && m_analysis[i].flags < LScriptAnalysis::TabOrObject
                && !atWordStart
                && i - start < MaxItemLength){
                continue;
            }

            m_items.push_back(LScriptItem(start, itemAnalysis));
            start = i;
            itemAnalysis = m_analysis[start];

            if (atWordStart){
                itemAnalysis.flags = LScriptAnalysis::Uppercase;
            }
        }
        m_items.push_back(LScriptItem(start, itemAnalysis));
    }

    void generateScriptItemsSmallCaps(const wchar_t *uc, int start, int length)
    {
        if (!length){
            return;
        }
        bool lower = (LStringUtil::category(uc[start]) == lchar::Letter_Lowercase);
        const int end = start + length;
        // split text into parts that are already uppercase and parts that are lowercase, and mark the latter to be uppercased later.
        for (int i = start + 1; i < end; ++i) {
            bool l = (LStringUtil::category(uc[i]) == lchar::Letter_Lowercase);
            if ((m_analysis[i] == m_analysis[start])
                && m_analysis[i].flags < LScriptAnalysis::TabOrObject
                && l == lower
                && i - start < MaxItemLength){
                continue;
            }
            m_items.push_back(LScriptItem(start, m_analysis[start]));
            if (lower){
                m_items[m_items.size()-1].analysis.flags = LScriptAnalysis::SmallCaps;
            }
            start = i;
            lower = l;
        }
        m_items.push_back(LScriptItem(start, m_analysis[start]));
        if (lower){
            m_items[m_items.size()-1].analysis.flags = LScriptAnalysis::SmallCaps;
        }
    }

    const std::wstring &m_string;
    const LScriptAnalysis * const m_analysis;
    std::vector<LScriptItem> &m_items;
    std::shared_ptr<LTextBoundaryFinder> m_splitter;
};

// -----------------------------------------------------------------------------------------------------
//
// The Unicode Bidi algorithm.
// See http://www.unicode.org/reports/tr9/tr9-37.html
//
// -----------------------------------------------------------------------------------------------------
enum { BidiDebugEnabled = false };

struct LBidiAlgorithm {

    LBidiAlgorithm(const wchar_t *text, LScriptAnalysis *analysis, int length, bool baseDirectionIsRtl)
        : text(text),
        analysis(analysis),
        length(length),
        baseLevel(baseDirectionIsRtl ? 1 : 0)
    {

    }

    struct IsolatePair {
        int start;
        int end;
    };

    void initScriptAnalysisAndIsolatePairs(std::vector<IsolatePair> &isolatePairs)
    {
        int isolateStack[128];
        int isolateLevel = 0;
        // load directions of string, and determine isolate pairs
        for (int i = 0; i < length; ++i) {
            int pos = i;
            uint32_t uc = text[i];
            if (LStringUtil::isHighSurrogate(uc) && i < length - 1) {
                ++i;
                analysis[i].bidiDirection = lchar::DirNSM;
                uc = LStringUtil::surrogateToUcs4(uint16_t(uc), text[i]);
            }
            const lchar::Properties *p = lchar::Util::properties(uc);
            analysis[pos].bidiDirection = lchar::Direction(p->direction);
            switch (lchar::Direction(p->direction)) {
            case lchar::DirON:{
                // all mirrored chars are DirON
                if (p->mirrorDiff){
                    analysis[pos].bidiFlags = LScriptAnalysis::BidiMirrored;
                }
                break;
            }
            case lchar::DirLRE:
            case lchar::DirRLE:
            case lchar::DirLRO:
            case lchar::DirRLO:
            case lchar::DirPDF:
            case lchar::DirBN:{
                analysis[pos].bidiFlags = LScriptAnalysis::BidiMaybeResetToParagraphLevel|LScriptAnalysis::BidiBN;
                break;
            }
            case lchar::DirLRI:
            case lchar::DirRLI:
            case lchar::DirFSI:{
                if (isolateLevel < 128) {
                    isolateStack[isolateLevel] = isolatePairs.size();
                    isolatePairs.push_back({ pos, length });
                }
                ++isolateLevel;
                analysis[pos].bidiFlags = LScriptAnalysis::BidiMaybeResetToParagraphLevel;
                break;
            }
            case lchar::DirPDI:{
                if (isolateLevel > 0) {
                    --isolateLevel;
                    if (isolateLevel < 128){
                        isolatePairs[isolateStack[isolateLevel]].end = pos;
                    }
                }
                //No break;
            }
            case lchar::DirWS:{
                analysis[pos].bidiFlags = LScriptAnalysis::BidiMaybeResetToParagraphLevel;
                break;
            }
            case lchar::DirS:
            case lchar::DirB:{
                analysis[pos].bidiFlags = LScriptAnalysis::BidiResetToParagraphLevel;
                if (uc == lchar::ParagraphSeparator) {
                    // close all open isolates as we start a new paragraph
                    while (isolateLevel > 0) {
                        --isolateLevel;
                        if (isolateLevel < 128){
                            isolatePairs[isolateStack[isolateLevel]].end = pos;
                        }
                    }
                }
                break;
            }
            default:{
                break;
            }
            }
        }
    }

    struct DirectionalRun {
        int start;
        int end;
        int continuation;
        uint16_t level;
        bool isContinuation;
        bool hasContent;
    };

    void generateDirectionalRuns(const std::vector<IsolatePair> &isolatePairs, std::vector<DirectionalRun> &runs)
    {
        struct DirectionalStack {
            enum { MaxDepth = 125 };
            struct Item {
                uint16_t level;
                bool isOverride;
                bool isIsolate;
                int runBeforeIsolate;
            };
            Item items[128];
            int counter = 0;

            void push(Item i) {
                items[counter] = i;
                ++counter;
            }
            void pop() {
                --counter;
            }
            int depth() const {
                return counter;
            }
            const Item &top() const {
                return items[counter - 1];
            }
        } stack;
        int overflowIsolateCount = 0;
        int overflowEmbeddingCount = 0;
        int validIsolateCount = 0;

        uint16_t level = baseLevel;
        bool override = false;
        stack.push({ level, false, false, -1 });

        int runStart = 0;
        int continuationFrom = -1;
        int lastRunWithContent = -1;
        bool runHasContent = false;

        auto appendRun = [&](int runEnd) {
            if (runEnd < runStart){
                return;
            }
            bool isContinuation = false;
            if (continuationFrom != -1) {
                runs[continuationFrom].continuation = runs.size();
                isContinuation = true;
            } else if (lastRunWithContent != -1 && level == runs.at(lastRunWithContent).level) {
                runs[lastRunWithContent].continuation = runs.size();
                isContinuation = true;
            }
            if (runHasContent){
                lastRunWithContent = runs.size();
            }

            runs.push_back({ runStart, runEnd, -1, level, isContinuation, runHasContent });
            runHasContent = false;
            runStart = runEnd + 1;
            continuationFrom = -1;
        };

        int isolatePairPosition = 0;

        for (int i = 0; i < length; ++i) {
            lchar::Direction dir = analysis[i].bidiDirection;

            auto doEmbed = [&](bool isRtl, bool isOverride, bool isIsolate) {
                if (isIsolate) {
                    if (override){
                        analysis[i].bidiDirection = (level & 1) ? lchar::DirR : lchar::DirL;
                    }
                    runHasContent = true;
                    lastRunWithContent = -1;
                    ++isolatePairPosition;
                }
                int runBeforeIsolate = runs.size();
                uint16_t newLevel = isRtl ? ((stack.top().level + 1) | 1) : ((stack.top().level + 2) & ~1);
                if (newLevel <= DirectionalStack::MaxDepth && !overflowEmbeddingCount && !overflowIsolateCount) {
                    if (isIsolate){
                        ++validIsolateCount;
                    }else{
                        runBeforeIsolate = -1;
                    }
                    appendRun(isIsolate ? i : i - 1);
                    stack.push({ newLevel, isOverride, isIsolate, runBeforeIsolate });
                    override = isOverride;
                    level = newLevel;
                } else {
                    if (isIsolate){
                        ++overflowIsolateCount;
                    }else if (!overflowIsolateCount){
                        ++overflowEmbeddingCount;
                    }
                }
                if (!isIsolate) {
                    if (override){
                        analysis[i].bidiDirection = (level & 1) ? lchar::DirR : lchar::DirL;
                    }else{
                        analysis[i].bidiDirection = lchar::DirBN;
                    }
                }
            };

            switch (dir) {
            case lchar::DirLRE:{
                doEmbed(false, false, false);
                break;
            }
            case lchar::DirRLE:{
                doEmbed(true, false, false);
                break;
            }
            case lchar::DirLRO:{
                doEmbed(false, true, false);
                break;
            }
            case lchar::DirRLO:{
                doEmbed(true, true, false);
                break;
            }
            case lchar::DirLRI:{
                doEmbed(false, false, true);
                break;
            }
            case lchar::DirRLI:{
                doEmbed(true, false, true);
                break;
            }
            case lchar::DirFSI: {
                bool isRtl = false;
                if (isolatePairPosition < (int)isolatePairs.size()) {
                    const auto &pair = isolatePairs.at(isolatePairPosition);
                    isRtl = LStringUtil::isRightToLeft(text + pair.start + 1, pair.end - pair.start - 1);
                }
                doEmbed(isRtl, false, true);
                break;
            }
            case lchar::DirPDF:{
                if (override){
                    analysis[i].bidiDirection = (level & 1) ? lchar::DirR : lchar::DirL;
                }else{
                    analysis[i].bidiDirection = lchar::DirBN;
                }
                if (overflowIsolateCount) {
                    ; // do nothing
                } else if (overflowEmbeddingCount) {
                    --overflowEmbeddingCount;
                } else if (!stack.top().isIsolate && stack.depth() >= 2) {
                    appendRun(i);
                    stack.pop();
                    override = stack.top().isOverride;
                    level = stack.top().level;
                }
                break;
            }
            case lchar::DirPDI:{
                runHasContent = true;
                if (overflowIsolateCount) {
                    --overflowIsolateCount;
                } else if (validIsolateCount == 0) {
                    ; // do nothing
                } else {
                    appendRun(i - 1);
                    overflowEmbeddingCount = 0;
                    while (!stack.top().isIsolate)
                        stack.pop();
                    continuationFrom = stack.top().runBeforeIsolate;
                    stack.pop();
                    override = stack.top().isOverride;
                    level = stack.top().level;
                    lastRunWithContent = -1;
                    --validIsolateCount;
                }
                if (override){
                    analysis[i].bidiDirection = (level & 1) ? lchar::DirR : lchar::DirL;
                }
                break;
            }
            case lchar::DirB:{
                // paragraph separator, go down to base direction, reset all state
                if (text[i] == lchar::ParagraphSeparator) {
                    appendRun(i - 1);
                    while (stack.counter > 1) {
                        // there might be remaining isolates on the stack that are missing a PDI. Those need to get
                        // a continuation indicating to take the eos from the end of the string (ie. the paragraph level)
                        const auto &t = stack.top();
                        if (t.isIsolate) {
                            runs[t.runBeforeIsolate].continuation = -2;
                        }
                        --stack.counter;
                    }
                    continuationFrom = -1;
                    lastRunWithContent = -1;
                    validIsolateCount = 0;
                    overflowIsolateCount = 0;
                    overflowEmbeddingCount = 0;
                    level = baseLevel;
                }
                break;
            }
            default:{
                runHasContent = true;
                //No break;
            }
            case lchar::DirBN:{
                if (override){
                    analysis[i].bidiDirection = (level & 1) ? lchar::DirR : lchar::DirL;
                }
                break;
            }
            }
        }
        appendRun(length - 1);
        while (stack.counter > 1) {
            // there might be remaining isolates on the stack that are missing a PDI. Those need to get
            // a continuation indicating to take the eos from the end of the string (ie. the paragraph level)
            const auto &t = stack.top();
            if (t.isIsolate) {
                runs[t.runBeforeIsolate].continuation = -2;
            }
            --stack.counter;
        }
    }

    void resolveExplicitLevels(std::vector<DirectionalRun> &runs)
    {
        std::vector<IsolatePair> isolatePairs;
        isolatePairs.reserve(64);

        initScriptAnalysisAndIsolatePairs(isolatePairs);
        generateDirectionalRuns(isolatePairs, runs);
    }

    struct IsolatedRunSequenceIterator {
        struct Position {
            int current = -1;
            int pos = -1;

            Position() = default;
            Position(int current, int pos) : current(current), pos(pos) {}

            bool isValid() const { return pos != -1; }
            void clear() { pos = -1; }
        };
        IsolatedRunSequenceIterator(const std::vector<DirectionalRun> &runs, int i)
            : runs(runs),
            current(i)
        {
            pos = runs.at(current).start;
        }
        int operator *() const { return pos; }
        bool atEnd() const { return pos < 0; }
        void operator++() {
            ++pos;
            if (pos > runs.at(current).end) {
                current = runs.at(current).continuation;
                if (current > -1)
                    pos = runs.at(current).start;
                else
                    pos = -1;
            }
        }
        void setPosition(Position p) {
            current = p.current;
            pos = p.pos;
        }
        Position position() const {
            return Position(current, pos);
        }
        bool operator !=(int position) const {
            return pos != position;
        }

        const std::vector<DirectionalRun> &runs;
        int current;
        int pos;
    };


    void resolveW1W2W3(const std::vector<DirectionalRun> &runs, int i, lchar::Direction sos)
    {
        lchar::Direction last = sos;
        lchar::Direction lastStrong = sos;
        IsolatedRunSequenceIterator it(runs, i);
        while (!it.atEnd()) {
            int pos = *it;

            // Rule W1: Resolve NSM
            lchar::Direction current = analysis[pos].bidiDirection;
            if (current == lchar::DirNSM) {
                current = last;
                analysis[pos].bidiDirection = current;
            } else if (current >= lchar::DirLRI) {
                last = lchar::DirON;
            } else if (current == lchar::DirBN) {
                current = last;
            } else {
                last = current;
            }

            // Rule W2
            if (current == lchar::DirEN && lastStrong == lchar::DirAL) {
                current = lchar::DirAN;
                analysis[pos].bidiDirection = current;
            }

            // remember last strong char for rule W2
            if (current == lchar::DirL || current == lchar::DirR) {
                lastStrong = current;
            } else if (current == lchar::DirAL) {
                // Rule W3
                lastStrong = current;
                analysis[pos].bidiDirection = lchar::DirR;
            }
            last = current;
            ++it;
        }
    }


    void resolveW4(const std::vector<DirectionalRun> &runs, int i, lchar::Direction sos)
    {
        // Rule W4
        lchar::Direction secondLast = sos;

        IsolatedRunSequenceIterator it(runs, i);
        int lastPos = *it;
        lchar::Direction last = analysis[lastPos].bidiDirection;

        //            BIDI_DEBUG() << "Applying rule W4/W5";
        ++it;
        while (!it.atEnd()) {
            int pos = *it;
            lchar::Direction current = analysis[pos].bidiDirection;
            if (current == lchar::DirBN) {
                ++it;
                continue;
            }
            //BIDI_DEBUG() << pos << secondLast << last << current;
            if (last == lchar::DirES && current == lchar::DirEN && secondLast == lchar::DirEN) {
                last = lchar::DirEN;
                analysis[lastPos].bidiDirection = last;
            } else if (last == lchar::DirCS) {
                if (current == lchar::DirEN && secondLast == lchar::DirEN) {
                    last = lchar::DirEN;
                    analysis[lastPos].bidiDirection = last;
                } else if (current == lchar::DirAN && secondLast == lchar::DirAN) {
                    last = lchar::DirAN;
                    analysis[lastPos].bidiDirection = last;
                }
            }
            secondLast = last;
            last = current;
            lastPos = pos;
            ++it;
        }
    }

    void resolveW5(const std::vector<DirectionalRun> &runs, int i)
    {
        // Rule W5
        IsolatedRunSequenceIterator::Position lastETPosition;
        IsolatedRunSequenceIterator it(runs, i);
        int lastPos = *it;
        lchar::Direction last = analysis[lastPos].bidiDirection;
        if (last == lchar::DirET || last == lchar::DirBN){
            lastETPosition = it.position();
        }

        ++it;
        while (!it.atEnd()) {
            int pos = *it;
            lchar::Direction current = analysis[pos].bidiDirection;
            if (current == lchar::DirBN) {
                ++it;
                continue;
            }
            if (current == lchar::DirET) {
                if (last == lchar::DirEN) {
                    current = lchar::DirEN;
                    analysis[pos].bidiDirection = current;
                } else if (!lastETPosition.isValid()) {
                    lastETPosition = it.position();
                }
            } else if (lastETPosition.isValid()) {
                if (current == lchar::DirEN) {
                    it.setPosition(lastETPosition);
                    while (it != pos) {
                        int pos = *it;
                        analysis[pos].bidiDirection = lchar::DirEN;
                        ++it;
                    }
                }
                lastETPosition.clear();
            }
            last = current;
            ++it;
        }
    }

    void resolveW6W7(const std::vector<DirectionalRun> &runs, int i, lchar::Direction sos)
    {
        lchar::Direction lastStrong = sos;
        IsolatedRunSequenceIterator it(runs, i);
        while (!it.atEnd()) {
            int pos = *it;
            // Rule W6
            lchar::Direction current = analysis[pos].bidiDirection;
            if (current == lchar::DirBN) {
                ++it;
                continue;
            }
            if (current == lchar::DirET || current == lchar::DirES || current == lchar::DirCS) {
                analysis[pos].bidiDirection = lchar::DirON;
            }

            // Rule W7
            else if (current == lchar::DirL || current == lchar::DirR) {
                lastStrong = current;
            } else if (current == lchar::DirEN && lastStrong == lchar::DirL) {
                analysis[pos].bidiDirection = lastStrong;
            }
            ++it;
        }
    }

    struct BracketPair {
        int first;
        int second;

        bool isValid() const { return second > 0; }

        lchar::Direction containedDirection(const LScriptAnalysis *analysis, lchar::Direction embeddingDir) const {
            int isolateCounter = 0;
            lchar::Direction containedDir = lchar::DirON;
            for (int i = first + 1; i < second; ++i) {
                lchar::Direction dir = analysis[i].bidiDirection;
                if (isolateCounter) {
                    if (dir == lchar::DirPDI){
                        --isolateCounter;
                    }
                    continue;
                }
                if (dir == lchar::DirL) {
                    containedDir = dir;
                    if (embeddingDir == dir){
                        break;
                    }
                } else if (dir == lchar::DirR || dir == lchar::DirAN || dir == lchar::DirEN) {
                    containedDir = lchar::DirR;
                    if (embeddingDir == lchar::DirR){
                        break;
                    }
                } else if (dir == lchar::DirLRI || dir == lchar::DirRLI || dir == lchar::DirFSI){
                    ++isolateCounter;
                }
            }
            return containedDir;
        }
    };

    struct BracketStack {
        struct Item {
            Item() = default;
            Item(uint32_t pairedBracked, int position) : pairedBracked(pairedBracked), position(position) {}
            uint32_t pairedBracked = 0;
            int position = 0;
        };

        void push(uint32_t closingUnicode, int pos) {
            if (position < MaxDepth){
                stack[position] = Item(closingUnicode, pos);
            }
            ++position;
        }
        int match(uint32_t unicode) {
            int p = position;
            while (--p >= 0) {
                if (stack[p].pairedBracked == unicode ||
                    // U+3009 and U+2329 are canonical equivalents of each other. Fortunately it's the only pair in Unicode 10
                    (stack[p].pairedBracked == 0x3009 && unicode == 0x232a) ||
                    (stack[p].pairedBracked == 0x232a && unicode == 0x3009)) {
                    position = p;
                    return stack[p].position;
                }

            }
            return -1;
        }

        enum { MaxDepth = 63 };
        Item stack[MaxDepth];
        int position = 0;

        bool overflowed() const { return position > MaxDepth; }
    };

    void resolveN0(const std::vector<DirectionalRun> &runs, int i, lchar::Direction sos)
    {
        uint16_t level = runs.at(i).level;

        std::vector<BracketPair> bracketPairs;
        {
            BracketStack bracketStack;
            IsolatedRunSequenceIterator it(runs, i);
            while (!it.atEnd()) {
                int pos = *it;
                lchar::Direction dir = analysis[pos].bidiDirection;
                if (dir == lchar::DirON) {
                    const lchar::Properties *p = lchar::Util::properties((uint16_t)text[pos]);
                    if (p->mirrorDiff) {
                        // either opening or closing bracket
                        if (p->category == lchar::Punctuation_Open) {
                            // opening bracked
                            uint32_t closingBracked = text[pos] + p->mirrorDiff;
                            bracketStack.push(closingBracked, bracketPairs.size());
                            if (bracketStack.overflowed()) {
                                bracketPairs.clear();
                                break;
                            }
                            bracketPairs.push_back({ pos, -1 });
                        } else if (p->category == lchar::Punctuation_Close) {
                            int pairPos = bracketStack.match(text[pos]);
                            if (pairPos != -1){
                                bracketPairs[pairPos].second = pos;
                            }
                        }
                    }
                }
                ++it;
            }
        }

        lchar::Direction lastStrong = sos;
        IsolatedRunSequenceIterator it(runs, i);
        lchar::Direction embeddingDir = (level & 1) ? lchar::DirR : lchar::DirL;
        for (uint32_t i = 0; i < bracketPairs.size(); ++i) {
            const auto &pair = bracketPairs.at(i);
            if (!pair.isValid())
                continue;
            lchar::Direction containedDir = pair.containedDirection(analysis, embeddingDir);
            if (containedDir == lchar::DirON) {
                continue;
            } else if (containedDir == embeddingDir) {
                analysis[pair.first].bidiDirection = embeddingDir;
                analysis[pair.second].bidiDirection = embeddingDir;
            } else {
                // case c.
                while (it.pos < pair.first) {
                    int pos = *it;
                    switch (analysis[pos].bidiDirection) {
                    case lchar::DirR:
                    case lchar::DirEN:
                    case lchar::DirAN:
                        lastStrong = lchar::DirR;
                        break;
                    case lchar::DirL:
                        lastStrong = lchar::DirL;
                        break;
                    default:
                        break;
                    }
                    ++it;
                }
                analysis[pair.first].bidiDirection = lastStrong;
                analysis[pair.second].bidiDirection = lastStrong;
            }
            for (int i = pair.second + 1; i < length; ++i) {
                if (LStringUtil::direction(text[i]) == lchar::DirNSM){
                    analysis[i].bidiDirection = analysis[pair.second].bidiDirection;
                }
                else{
                    break;
                }
            }
        }
    }

    void resolveN1N2(const std::vector<DirectionalRun> &runs, int i, lchar::Direction sos, lchar::Direction eos)
    {
        // Rule N1 & N2
        lchar::Direction lastStrong = sos;
        IsolatedRunSequenceIterator::Position niPos;
        IsolatedRunSequenceIterator it(runs, i);
        //            lchar::Direction last = lchar::DirON;
        while (1) {
            int pos = *it;

            lchar::Direction current = pos >= 0 ? analysis[pos].bidiDirection : eos;
            lchar::Direction currentStrong = current;
            switch (current) {
            case lchar::DirEN:
            case lchar::DirAN:{
                currentStrong = lchar::DirR;
                //No break;
            }
            case lchar::DirL:
            case lchar::DirR:{
                if (niPos.isValid()) {
                    lchar::Direction dir = currentStrong;
                    if (lastStrong != currentStrong)
                        dir = (runs.at(i).level) & 1 ? lchar::DirR : lchar::DirL;
                    it.setPosition(niPos);
                    while (*it != pos) {
                        if (analysis[*it].bidiDirection != lchar::DirBN)
                            analysis[*it].bidiDirection = dir;
                        ++it;
                    }
                    niPos.clear();
                }
                lastStrong = currentStrong;
                break;
            }
            case lchar::DirBN:
            case lchar::DirS:
            case lchar::DirWS:
            case lchar::DirON:
            case lchar::DirFSI:
            case lchar::DirLRI:
            case lchar::DirRLI:
            case lchar::DirPDI:
            case lchar::DirB:{
                if (!niPos.isValid()){
                    niPos = it.position();
                }
                break;
            }
            default:{
                break;
            }
            }
            if (it.atEnd()){
                break;
            }
            ++it;
        }
    }

    void resolveImplicitLevelsForIsolatedRun(const std::vector<DirectionalRun> &runs, int i)
    {
        // Rule X10
        int level = runs.at(i).level;
        int before = i - 1;
        while (before >= 0 && !runs.at(before).hasContent){
            --before;
        }
        int level_before = (before >= 0) ? runs.at(before).level : baseLevel;
        int after = i;
        while (runs.at(after).continuation >= 0)
            after = runs.at(after).continuation;
        if (runs.at(after).continuation == -2) {
            after = runs.size();
        } else {
            ++after;
            while (after < (int)runs.size() && !runs.at(after).hasContent){
                ++after;
            }
        }
        int level_after = (after == runs.size()) ? baseLevel : runs.at(after).level;
        lchar::Direction sos = (std::max(level_before, level) & 1) ? lchar::DirR : lchar::DirL;
        lchar::Direction eos = (std::max(level_after, level) & 1) ? lchar::DirR : lchar::DirL;

        resolveW1W2W3(runs, i, sos);
        resolveW4(runs, i, sos);
        resolveW5(runs, i);


        resolveW6W7(runs, i, sos);

        // Resolve neutral types

        // Rule N0
        resolveN0(runs, i, sos);
        resolveN1N2(runs, i, sos, eos);

        // Rules I1 & I2: set correct levels
        {
            uint16_t level = runs.at(i).level;
            IsolatedRunSequenceIterator it(runs, i);
            while (!it.atEnd()) {
                int pos = *it;

                lchar::Direction current = analysis[pos].bidiDirection;
                switch (current) {
                case lchar::DirBN:
                    break;
                case lchar::DirL:
                    analysis[pos].bidiLevel = (level + 1) & ~1;
                    break;
                case lchar::DirR:
                    analysis[pos].bidiLevel = level | 1;
                    break;
                case lchar::DirAN:
                case lchar::DirEN:
                    analysis[pos].bidiLevel = (level + 2) & ~1;
                    break;
                default:{
                    break;
                }
                }
                ++it;
            }
        }
    }

    void resolveImplicitLevels(const std::vector<DirectionalRun> &runs)
    {
        for (uint32_t i = 0; i < runs.size(); ++i) {
            if (runs.at(i).isContinuation){
                continue;
            }
            resolveImplicitLevelsForIsolatedRun(runs, i);
        }
    }

    bool checkForBidi() const
    {
        if (baseLevel != 0)
            return true;
        for (int i = 0; i < length; ++i) {
            if (text[i] >= 0x590) {
                switch (LStringUtil::direction(text[i])) {
                case lchar::DirR: case lchar::DirAN:
                case lchar::DirLRE: case lchar::DirLRO: case lchar::DirAL:
                case lchar::DirRLE: case lchar::DirRLO: case lchar::DirPDF:
                case lchar::DirLRI: case lchar::DirRLI: case lchar::DirFSI: case lchar::DirPDI:{
                    return true;
                }
                default:{
                    break;
                }
                }
            }
        }
        return false;
    }

    bool process()
    {
        memset(analysis, 0, length * sizeof(LScriptAnalysis));

        bool hasBidi = checkForBidi();        
        if (!hasBidi){
            return false;
        }

        {
            std::vector<DirectionalRun> runs;
            resolveExplicitLevels(runs);
            // now we have a list of isolated run sequences inside the vector of runs, that can be fed
            // through the implicit level resolving

            resolveImplicitLevels(runs);
        }

        // Rule L1:
        bool resetLevel = true;
        for (int i = length - 1; i >= 0; --i) {
            if (analysis[i].bidiFlags & LScriptAnalysis::BidiResetToParagraphLevel) {
                analysis[i].bidiLevel = baseLevel;
                resetLevel = true;
            } else if (resetLevel && analysis[i].bidiFlags & LScriptAnalysis::BidiMaybeResetToParagraphLevel) {
                analysis[i].bidiLevel = baseLevel;
            } else {
                resetLevel = false;
            }
        }

        // set directions for BN to the minimum of adjacent chars
        // This makes is possible to be conformant with the Bidi algorithm even though we don't
        // remove BN and explicit embedding chars from the stream of characters to reorder
        int lastLevel = baseLevel;
        int lastBNPos = -1;
        for (int i = 0; i < length; ++i) {
            if (analysis[i].bidiFlags & LScriptAnalysis::BidiBN) {
                if (lastBNPos < 0){
                    lastBNPos = i;
                }
                analysis[i].bidiLevel = lastLevel;
            } else {
                int l = analysis[i].bidiLevel;
                if (lastBNPos >= 0) {
                    if (l < lastLevel) {
                        while (lastBNPos < i) {
                            analysis[lastBNPos].bidiLevel = l;
                            ++lastBNPos;
                        }
                    }
                    lastBNPos = -1;
                }
                lastLevel = l;
            }
        }
        if (lastBNPos >= 0 && baseLevel < lastLevel) {
            while (lastBNPos < length) {
                analysis[lastBNPos].bidiLevel = baseLevel;
                ++lastBNPos;
            }
        }

        return true;
    }


    const wchar_t *text;
    LScriptAnalysis *analysis;
    int length;
    char baseLevel;
};

} // namespace


void LScriptLine::setDefaultHeight(std::shared_ptr<LTextEngine> eng)
{
    std::shared_ptr<LFontEngine> e = eng->font()->engine();
    double otherAscent = e->ascent();
    double otherDescent = e->descent();
    double otherLeading = e->leading();
    leading = std::max(leading + ascent, otherLeading + otherAscent) - std::max(ascent, otherAscent);
    ascent = std::max(ascent, otherAscent);
    descent = std::max(descent, otherDescent);
}

void LScriptLine::operator+=(const LScriptLine &other)
{
    leading= std::max(leading + ascent, other.leading + other.ascent) - std::max(ascent, other.ascent);
    descent = std::max(descent, other.descent);
    ascent = std::max(ascent, other.ascent);
    textWidth += other.textWidth;
    length += other.length;
}


LTextEngine::LayoutData::LayoutData(const std::wstring &str, void **stackMemory, int _allocated)
    : m_string(str)
{
    m_allocated = _allocated;

    int spaceCharAttributes = sizeof(LCharAttributes)*m_string.length()/sizeof(void*) + 1;
    int spaceLogClusters = sizeof(unsigned short)*m_string.length()/sizeof(void*) + 1;
    m_availableGlyphs = ((int)m_allocated - spaceCharAttributes - spaceLogClusters)*(int)sizeof(void*)/(int)LGlyphLayout::SpaceNeeded;

    if (m_availableGlyphs < (int)str.length()) {
        // need to allocate on the heap
        m_allocated = 0;
        m_memoryOnStack = false;
        m_memory = nullptr;
        m_logClustersPtr = nullptr;
    } else {
        m_memoryOnStack = true;
        m_memory = stackMemory;
        m_logClustersPtr = (unsigned short *)(m_memory + spaceCharAttributes);

        void *m = m_memory + spaceCharAttributes + spaceLogClusters;
        m_glyphLayout = LGlyphLayout(reinterpret_cast<char *>(m), str.length());
        m_glyphLayout.clear();
        memset(m_memory, 0, spaceCharAttributes*sizeof(void *));
    }
    m_used = 0;
    m_hasBidi = false;
    m_layoutState = LayoutEmpty;
    m_haveCharAttributes = false;
}

LTextEngine::LayoutData::LayoutData()
{
    m_memory = nullptr;
    m_allocated = 0;
    m_memoryOnStack = false;
    m_used = 0;
    m_hasBidi = false;
    m_layoutState = LayoutEmpty;
    m_haveCharAttributes = false;
    m_logClustersPtr = nullptr;
    m_availableGlyphs = 0;
}

LTextEngine::LayoutData::~LayoutData()
{
    if (!m_memoryOnStack){
        free(m_memory);
    }
    m_memory = nullptr;
}

bool LTextEngine::LayoutData::reallocate(int totalGlyphs)
{
    if (m_memoryOnStack && m_availableGlyphs >= totalGlyphs) {
        m_glyphLayout.grow(m_glyphLayout.data(), totalGlyphs);
        return true;
    }

    int spaceCharAttributes = sizeof(LCharAttributes)*m_string.length()/sizeof(void*) + 1;
    int spaceLogClusters = sizeof(unsigned short)*m_string.length()/sizeof(void*) + 1;
    int spaceGlyphs = (totalGlyphs * LGlyphLayout::SpaceNeeded) / sizeof(void *) + 2;

    int newAllocated = spaceCharAttributes + spaceGlyphs + spaceLogClusters;
    // These values can be negative if the length of string/glyphs causes overflow,
    // we can't layout such a long string all at once, so return false here to
    // indicate there is a failure
    if (spaceCharAttributes < 0 || spaceLogClusters < 0 || spaceGlyphs < 0 || newAllocated < m_allocated) {
        m_layoutState = LayoutFailed;
        return false;
    }

    void **newMem = (void **)::realloc(m_memoryOnStack ? nullptr : m_memory, newAllocated*sizeof(void *));
    if (!newMem) {
        m_layoutState = LayoutFailed;
        return false;
    }
    if (m_memoryOnStack){
        memcpy(newMem, m_memory, m_allocated*sizeof(void *));
    }
    m_memory = newMem;
    m_memoryOnStack = false;

    void **m = m_memory;
    m += spaceCharAttributes;
    m_logClustersPtr = (unsigned short *) m;
    m += spaceLogClusters;

    const int spacePreGlyphLayout = spaceCharAttributes + spaceLogClusters;
    if (m_allocated < spacePreGlyphLayout){
        memset(m_memory + m_allocated, 0, (spacePreGlyphLayout - m_allocated)*sizeof(void *));
    }

    m_glyphLayout.grow(reinterpret_cast<char *>(m), totalGlyphs);
    m_allocated = newAllocated;
    return true;
}


LTextEngine::LTextEngine()
{
    init(this);
}

LTextEngine::LTextEngine(const std::wstring &str, std::shared_ptr<LFont> f)
    : m_text(str)
{
    m_font = f;
    init(this);
}

void LTextEngine::invalidate()
{
    freeMemory();
    m_minWidth = 0;
    m_maxWidth = 0;
}

void LTextEngine::clearLineData()
{
    m_lines.clear();
}

void LTextEngine::init(LTextEngine *e)
{
    e->m_ignoreBidi = false;
    e->m_cacheGlyphs = false;
    e->m_forceJustification = false;
    e->m_visualMovement = false;
    e->m_delayDecorations = false;

    e->m_layoutData = nullptr;

    e->m_minWidth = 0;
    e->m_maxWidth = 0;

    //e->m_specialData = nullptr;
    e->m_stackEngine = false;
}

void LTextEngine::addItemDecoration(LPainter *painter,
                                    const LLine<double> &line,
                                    std::vector<ItemDecoration> &decorations)
{
    if (m_delayDecorations) {
        decorations.push_back(ItemDecoration(line.m_pt1.m_x, line.m_pt2.m_x, line.m_pt1.m_y, painter->pen()));
    } else {
        painter->drawLine(line);
    }
}

void LTextEngine::indexFormats()
{
    std::shared_ptr<LTextFormatCollection> collection = formatCollection();
    if (collection == nullptr) {
        m_specialData->m_formatCollection  = std::make_shared<LTextFormatCollection>();
        collection = m_specialData->m_formatCollection;
    }

    // replace with shared copies
    for (int i = 0; i < (int)m_specialData->m_formats.size(); ++i) {
        LTextCharFormat &format = m_specialData->m_formats[i].format;
        format = collection->charFormat(collection->indexForFormat(format));
    }
}

namespace {
class FormatRangeComparatorByStart {
    const std::vector<LTextLayout::FormatRange> &list;
public:
    FormatRangeComparatorByStart(const std::vector<LTextLayout::FormatRange> &list) : list(list) { }
    bool operator()(int a, int b) {
        return list.at(a).start < list.at(b).start;
    }
};
class FormatRangeComparatorByEnd {
    const std::vector<LTextLayout::FormatRange> &list;
public:
    FormatRangeComparatorByEnd(const std::vector<LTextLayout::FormatRange> &list) : list(list) { }
    bool operator()(int a, int b) {
        return list.at(a).start + list.at(a).length < list.at(b).start + list.at(b).length;
    }
};
}

void LTextEngine::resolveFormats() const
{
    if(m_specialData == nullptr){
        return;
    }

    if (m_specialData->m_formats.empty()){
        return;
    }

    std::shared_ptr<LTextFormatCollection> collection = formatCollection();

    std::vector<LTextCharFormat> resolvedFormats(m_layoutData->m_items.size());

    std::vector<int> formatsSortedByStart;
    formatsSortedByStart.reserve(m_specialData->m_formats.size());
    for (int i = 0; i < (int)m_specialData->m_formats.size(); ++i) {
        if (m_specialData->m_formats.at(i).length >= 0){
            formatsSortedByStart.push_back(i);
        }
    }
    std::vector<int> formatsSortedByEnd = formatsSortedByStart;
    sort(formatsSortedByStart.begin(), formatsSortedByStart.end(),
              FormatRangeComparatorByStart(m_specialData->m_formats));
    sort(formatsSortedByEnd.begin(), formatsSortedByEnd.end(),
              FormatRangeComparatorByEnd(m_specialData->m_formats));

    std::vector<int>  currentFormats;
    std::vector<int>::iterator startIt = formatsSortedByStart.begin();
    std::vector<int>::iterator endIt = formatsSortedByEnd.begin();

    for (int i = 0; i < (int)m_layoutData->m_items.size(); ++i) {
        const LScriptItem *si = &m_layoutData->m_items.at(i);
        int end = si->position + length(si);

        while (startIt != formatsSortedByStart.end() &&
               m_specialData->m_formats.at(*startIt).start <= si->position) {
            currentFormats.insert(std::upper_bound(currentFormats.begin(), currentFormats.end(), *startIt),
                                  *startIt);
            ++startIt;
        }
        while (endIt != formatsSortedByEnd.end() &&
               m_specialData->m_formats.at(*endIt).start + m_specialData->m_formats.at(*endIt).length < end) {
            std::vector<int>::iterator currentFormatIterator = std::lower_bound(currentFormats.begin(), currentFormats.end(), *endIt);
            if (*endIt < *currentFormatIterator){
                currentFormatIterator = currentFormats.end();
            }
            endIt = currentFormats.erase(currentFormatIterator);
        }

        LTextCharFormat &format = resolvedFormats[i];
        if (!currentFormats.empty()) {
            for (int cur : currentFormats) {
                const LTextLayout::FormatRange &range = m_specialData->m_formats.at(cur);
                format.merge(range.format);
            }
            format = collection->charFormat(collection->indexForFormat(format)); // get shared copy
        }
    }

    m_specialData->m_resolvedFormats = resolvedFormats;
}

void LTextEngine::drawItemDecorations(LPainter *painter, const std::vector<ItemDecoration> &decorations)
{
    if (decorations.empty()){
        return;
    }

    for (const ItemDecoration &decoration : decorations) {
        painter->setPen(decoration.pen, false);
        painter->drawLine(LLine<double>(decoration.x1, decoration.y, decoration.x2, decoration.y));
    }
}


void LTextEngine::addUnderline(LPainter *painter, const LLine<double> &line)
{
    addItemDecoration(painter, line, m_underlines);
}

void LTextEngine::addStrikeOut(LPainter *painter, const LLine<double> &line)
{
    addItemDecoration(painter, line, m_strikeOuts);
}

void LTextEngine::addOverline(LPainter *painter, const LLine<double> &line)
{
    addItemDecoration(painter, line, m_overlines);
}

void LTextEngine::validate() const
{
    if (m_layoutData != nullptr){
        return;
    }
    m_layoutData = std::make_shared<LayoutData>();
    m_layoutData->m_string = m_text;
    if(m_specialData != nullptr){
        if (m_specialData->m_preeditPosition != -1){
            m_layoutData->m_string.insert(m_specialData->m_preeditPosition, m_specialData->m_preeditText);
        }
    }
}

void LTextEngine::addRequiredBoundaries() const
{
    if (m_specialData) {
        for(const LTextLayout::FormatRange& r : m_specialData->m_formats) {
            setBoundary(r.start);
            setBoundary(r.start + r.length);
        }
    }
}

void LTextEngine::setBoundary(int strPos) const
{
    const int item = findItem(strPos);
    if (item < 0){
        return;
    }

    LScriptItem newItem = m_layoutData->m_items.at(item);
    if (newItem.position != strPos) {
        newItem.position = strPos;
        m_layoutData->m_items.insert(next(m_layoutData->m_items.begin(), item + 1), newItem);
    }
}

int LTextEngine::getClusterLength(uint16_t *logClusters, const LCharAttributes *attributes,
                                  int from, int to, int glyphPos, int *start) const
{
    int clusterLength = 0;
    for (int i = from; i < to; i++) {
        if (logClusters[i] == glyphPos && attributes[i].graphemeBoundary) {
            if (*start < 0){
                *start = i;
            }
            clusterLength++;
        }
        else if (clusterLength){
            break;
        }
    }
    return clusterLength;
}


void LTextEngine::itemize() const
{
    validate();
    if (m_layoutData->m_items.empty()==false){
        return;
    }

    int length = m_layoutData->m_string.length();
    if (!length){
        return;
    }

    const uint16_t *string = reinterpret_cast<const uint16_t *>(m_layoutData->m_string.data());
    bool rtl = isRightToLeft();

    std::vector<LScriptAnalysis> scriptAnalysis(length);
    //scriptAnalysis.reserve(4096);
    LScriptAnalysis *analysis = scriptAnalysis.data();

    LBidiAlgorithm bidi(m_layoutData->m_string.data(), analysis, length, rtl);
    m_layoutData->m_hasBidi = bidi.process();

    {
        std::vector<uint8_t> scripts(length);
        //scripts.reserve(256);
        lchar::Util::initScripts(string, length, scripts.data());
        for (int i = 0; i < length; ++i){
            analysis[i].script = scripts.at(i);
        }
    }

    const uint16_t *uc = string;
    const uint16_t *e = uc + length;
    while (uc < e) {
        switch (*uc) {
        case lchar::ObjectReplacementCharacter:{
            analysis->flags = LScriptAnalysis::Object;
            break;
        }
        case lchar::LineSeparator:{
            analysis->flags = LScriptAnalysis::LineOrParagraphSeparator;
            if (m_option.m_flags & LTextOption::ShowLineAndParagraphSeparators) {
                *const_cast<uint16_t*>(uc) = 0x21B5; //替换为可见换行
            }
            break;
        }
        case lchar::Tabulation:
            analysis->flags = LScriptAnalysis::Tab;
            analysis->bidiLevel = bidi.baseLevel;
            break;
        case lchar::Space:
        case lchar::Nbsp:
            if (m_option.m_flags & LTextOption::ShowTabsAndSpaces) {
                analysis->flags = (*uc == lchar::Space) ? LScriptAnalysis::Space : LScriptAnalysis::Nbsp;
                break;
            }
            //No break;
        default:
            analysis->flags = LScriptAnalysis::None;
            break;
        }

        ++uc;
        ++analysis;
    }
    if (m_option.m_flags & LTextOption::ShowLineAndParagraphSeparators) {
        (analysis-1)->flags = LScriptAnalysis::LineOrParagraphSeparator; // to exclude it from width
    }

    Itemizer itemizer(m_layoutData->m_string, scriptAnalysis.data(), m_layoutData->m_items);
    itemizer.generate(0, length, static_cast<LFont::Capitalization> (font()->m_capital));

    addRequiredBoundaries();
    resolveFormats();
}

struct LJustificationPoint {
    int type;
    double kashidaWidth;
    LGlyphLayout glyph;
};


static void set(LJustificationPoint *point, int type, const LGlyphLayout &glyph, std::shared_ptr<LFontEngine> fe)
{
    point->type = type;
    point->glyph = glyph;

    if (type >= Justification_Arabic_Normal) {
        wchar_t ch(0x640); // Kashida character

        uint32_t kashidaGlyph = fe->glyphIndex(ch);
        if (kashidaGlyph != 0) {
            LGlyphLayout g;
            g.numGlyphs = 1;
            g.glyphs = &kashidaGlyph;
            g.advances = &point->kashidaWidth;
            fe->recalcAdvances(&g, 0);

            if (point->kashidaWidth == 0){
                point->type = Justification_Prohibited;
            }
        } else {
            point->type = Justification_Prohibited;
            point->kashidaWidth = 0;
        }
    }
}


void LTextEngine::justify(const LScriptLine &line)
{
    if (line.gridfitted && line.justified)
        return;

    if (!line.gridfitted) {
        // redo layout in device metrics, then adjust
        const_cast<LScriptLine &>(line).gridfitted = true;
    }

    if ((m_option.m_align & (uint32_t)LAlignment::HorizontalMask) != (uint32_t)LAlignment::Justify){
        return;
    }

    itemize();

    if (!m_forceJustification) {
        int end = line.from + (int)line.length + line.trailingSpaces;
        if (end == m_layoutData->m_string.length()){
            return; // no justification at end of paragraph
        }
        if (end && m_layoutData->m_items.at(findItem(end - 1)).analysis.flags == LScriptAnalysis::LineOrParagraphSeparator){
            return; // no justification at the end of an explicitly separated line
        }
    }

    // justify line
    int maxJustify = 0;

    // don't include trailing white spaces when doing justification
    int line_length = line.length;
    const LCharAttributes *a = attributes();
    if ( a== nullptr){
        return;
    }
    a += line.from;
    while (line_length && a[line_length-1].whiteSpace){
        --line_length;
    }
    // subtract one char more, as we can't justfy after the last character
    --line_length;

    if (line_length <= 0){
        return;
    }

    int firstItem = findItem(line.from);
    int lastItem = findItem(line.from + line_length - 1, firstItem);
    int nItems = (firstItem >= 0 && lastItem >= firstItem)? (lastItem-firstItem+1) : 0;

    std::vector<LJustificationPoint> justificationPoints;
    int nPoints = 0;
    //     qDebug("justifying from %d len %d, firstItem=%d, nItems=%d (%s)", line.from, line_length, firstItem, nItems, layoutData->string.mid(line.from, line_length).toUtf8().constData());
    double minKashida = 0x100000;

    // we need to do all shaping before we go into the next loop, as we there
    // store pointers to the glyph data that could get reallocated by the shaping
    // process.
    for (int i = 0; i < nItems; ++i) {
        const LScriptItem &si = m_layoutData->m_items.at(firstItem + i);
        if (!si.numGlyphs){
            shape(firstItem + i);
        }
    }

    for (int i = 0; i < nItems; ++i) {
        const LScriptItem &si = m_layoutData->m_items.at(firstItem + i);

        int kashida_type = Justification_Arabic_Normal;
        int kashida_pos = -1;

        int start = std::max<int>(line.from - si.position, 0);
        int end = std::min<int>(line.from + line_length - (int)si.position, length(firstItem+i));

        const uint16_t *log_clusters = logClusters(&si);

        int gs = log_clusters[start];
        int ge = (end == length(firstItem+i) ? si.numGlyphs : log_clusters[end]);


        const LGlyphLayout g = shapedGlyphs(&si);

        for (int i = gs; i < ge; ++i) {
            g.justifications[i].type = LGlyphJustification::JustifyNone;
            g.justifications[i].nKashidas = 0;
            g.justifications[i].space = 0;

            justificationPoints.resize(nPoints+3);
            int justification = g.attributes[i].justification;

            switch(justification) {
            case Justification_Prohibited:{
                break;
            }
            case Justification_Space:
            case Justification_Arabic_Space:{
                if (kashida_pos >= 0) {
                    //                     qDebug("kashida position at %d in word", kashida_pos);
                    set(&justificationPoints[nPoints], kashida_type, g.mid(kashida_pos), fontEngine());
                    if (justificationPoints[nPoints].kashidaWidth > 0) {
                        minKashida = std::min<double>(minKashida, justificationPoints[nPoints].kashidaWidth);
                        maxJustify = std::max<int>(maxJustify, justificationPoints[nPoints].type);
                        ++nPoints;
                    }
                }
                kashida_pos = -1;
                kashida_type = Justification_Arabic_Normal;
                _FALLTHROUGH;
            }
            case Justification_Character:{
                set(&justificationPoints[nPoints++], justification, g.mid(i), fontEngine());
                maxJustify = std::max<int>(maxJustify, justification);
                break;
            }
            case Justification_Arabic_Normal:
            case Justification_Arabic_Waw:
            case Justification_Arabic_BaRa:
            case Justification_Arabic_Alef:
            case Justification_Arabic_HahDal:
            case Justification_Arabic_Seen:
            case Justification_Arabic_Kashida:{
                if (justification >= kashida_type) {
                    kashida_pos = i;
                    kashida_type = justification;
                }
                break;
            }
            }
        }
        if (kashida_pos >= 0) {
            set(&justificationPoints[nPoints], kashida_type, g.mid(kashida_pos), fontEngine());
            if (justificationPoints[nPoints].kashidaWidth > 0) {
                minKashida = std::min<double>(minKashida, justificationPoints[nPoints].kashidaWidth);
                maxJustify = std::max<int>(maxJustify, justificationPoints[nPoints].type);
                ++nPoints;
            }
        }
    }

    double leading = leadingSpaceWidth(line);
    double need = line.width - line.textWidth - leading;
    if (need < 0) {
        // line overflows already!
        const_cast<LScriptLine &>(line).justified = true;
        return;
    }

    // distribute in priority order
    if (maxJustify >= Justification_Arabic_Normal) {
        while (need >= minKashida) {
            for (int type = maxJustify; need >= minKashida && type >= Justification_Arabic_Normal; --type) {
                for (int i = 0; need >= minKashida && i < nPoints; ++i) {
                    if (justificationPoints[i].type == type && justificationPoints[i].kashidaWidth <= need) {
                        justificationPoints[i].glyph.justifications->nKashidas++;
                        // ############
                        justificationPoints[i].glyph.justifications->space += justificationPoints[i].kashidaWidth;
                        need -= justificationPoints[i].kashidaWidth;
                        //                         qDebug("adding kashida type %d with width %x, neednow %x", type, justificationPoints[i].kashidaWidth, need.value());
                    }
                }
            }
        }
    }

    if (!need){
        goto end;
    }

    maxJustify = std::min<int>(maxJustify, int(Justification_Space));
    for (int type = maxJustify; need != 0 && type > 0; --type) {
        int n = 0;
        for (int i = 0; i < nPoints; ++i) {
            if (justificationPoints[i].type == type)
                ++n;
        }
        //          qDebug("number of points for justification type %d: %d", type, n);


        if (!n){
            continue;
        }

        for (int i = 0; i < nPoints; ++i) {
            if (justificationPoints[i].type == type) {
                double add = need/n;
                //                  qDebug("adding %x to glyph %x", add.value(), justificationPoints[i].glyph->glyph);
                justificationPoints[i].glyph.justifications[0].space = add;
                need -= add;
                --n;
            }
        }
    }
end:
    const_cast<LScriptLine &>(line).justified = true;

}

void LTextEngine::shapeLine(const LScriptLine &line)
{
    double x = 0;
    bool first = true;
    int item = findItem(line.from);
    if (item == -1){
        return;
    }

    const int end = findItem(line.from + line.length + line.trailingSpaces - 1, item);
    for ( ; item <= end; ++item) {
        LScriptItem &si = m_layoutData->m_items[item];
        if (si.analysis.flags == LScriptAnalysis::Tab) {
            ensureSpace(1);
            si.width = calculateTabWidth(item, x);
        } else {
            shape(item);
        }
        if (first && si.position != line.from) { // that means our x position has to be offset
            LGlyphLayout glyphs = shapedGlyphs(&si);
            for (int i = line.from - si.position - 1; i >= 0; i--) {
                x -= glyphs.effectiveAdvance(i);
            }
        }
        first = false;

        x += si.width;
    }
}

double LTextEngine::leadingSpaceWidth(const LScriptLine &line)
{
    if (!line.hasTrailingSpaces
        || (m_option.m_flags & LTextOption::IncludeTrailingSpaces)
        || !isRightToLeft()){
        return 0;
    }
    return width(line.from + line.length, line.trailingSpaces);
}

int LTextEngine::findItem(int strPos, int firstItem) const
{
    itemize();
    if (strPos < 0 || strPos >= (int)m_layoutData->m_string.size() || firstItem < 0){
        return -1;
    }

    int left = firstItem + 1;
    int right = m_layoutData->m_items.size()-1;
    while(left <= right) {
        int middle = ((right-left)/2)+left;
        if (strPos > m_layoutData->m_items.at(middle).position){
            left = middle+1;
        }
        else if (strPos < m_layoutData->m_items.at(middle).position){
            right = middle-1;
        }
        else {
            return middle;
        }
    }
    return right;
}

bool LTextEngine::isRightToLeft() const
{
    switch (m_option.m_direction) {
    case (uint32_t)LLayoutDirection::LeftToRight:{
        return false;
    }
    case (uint32_t)LLayoutDirection::RightToLeft:
        return true;
    default:
        break;
    }
    if (m_layoutData == nullptr){
        itemize();
    }
//    // this places the cursor in the right position depending on the keyboard layout
    if (m_layoutData->m_string.empty()){
        //Todo
        //return LGuiApplication::inputMethod()->inputDirection() == LLayoutDirection::RightToLeft;
        return false;
    }
    return LStringUtil::isRightToLeft(m_layoutData->m_string);
}


namespace {
template<typename InnerFunc>
void textIterator(const LTextEngine *textEngine, int from, int len, double &width, InnerFunc &&innerFunc)
{
    for (int i = 0; i < textEngine->m_layoutData->m_items.size(); i++) {
        const LScriptItem *si = textEngine->m_layoutData->m_items.data() + i;
        int pos = si->position;
        int ilen = textEngine->length(i);
        if (pos >= from + len){
            break;
        }
        if (pos + ilen > from) {
            if (!si->numGlyphs){
                textEngine->shape(i);
            }

            if (si->analysis.flags == LScriptAnalysis::Object) {
                width += si->width;
                continue;
            } else if (si->analysis.flags == LScriptAnalysis::Tab) {
                width += textEngine->calculateTabWidth(i, width);
                continue;
            }

            unsigned short *logClusters = textEngine->logClusters(si);
            int charFrom = from - pos;
            if (charFrom < 0){
                charFrom = 0;
            }
            int glyphStart = logClusters[charFrom];
            if (charFrom > 0 && logClusters[charFrom-1] == glyphStart)
                while (charFrom < ilen && logClusters[charFrom] == glyphStart){
                    charFrom++;
                }
            if (charFrom < ilen) {
                glyphStart = logClusters[charFrom];
                int charEnd = from + len - 1 - pos;
                if (charEnd >= ilen){
                    charEnd = ilen-1;
                }
                int glyphEnd = logClusters[charEnd];
                while (charEnd < ilen && logClusters[charEnd] == glyphEnd){
                    charEnd++;
                }
                glyphEnd = (charEnd == ilen) ? si->numGlyphs : logClusters[charEnd];
                innerFunc(glyphStart, glyphEnd, si);
            }
        }
    }
}
} // namespace


double LTextEngine::width(int from, int len) const
{
    itemize();

    double w = 0;
    textIterator(this, from, len, w, [this, &w](int glyphStart, int glyphEnd, const LScriptItem *si) {
        LGlyphLayout glyphs = this->shapedGlyphs(si);
        for (int j = glyphStart; j < glyphEnd; j++){
            w += glyphs.advances[j] * !glyphs.attributes[j].dontPrint;
        }
    });
    return w;
}

void LTextEngine::bidiReorder(int numItems, const uint8_t *levels, int *visualOrder)
{
    // first find highest and lowest levels
    uint8_t levelLow = 128;
    uint8_t levelHigh = 0;
    int i = 0;
    while (i < numItems) {
        //printf("level = %d\n", r->level);
        if (levels[i] > levelHigh){
            levelHigh = levels[i];
        }
        if (levels[i] < levelLow){
            levelLow = levels[i];
        }
        i++;
    }

    // implements reordering of the line (L2 according to BiDi spec):
    // L2. From the highest level found in the text to the lowest odd level on each line,
    // reverse any contiguous sequence of characters that are at that level or higher.

    // reversing is only done up to the lowest odd level
    if(!(levelLow%2)){
        levelLow++;
    }

    int count = numItems - 1;
    for (i = 0; i < numItems; i++){
        visualOrder[i] = i;
    }

    while(levelHigh >= levelLow) {
        int i = 0;
        while (i < count) {
            while(i < count && levels[i] < levelHigh){
                i++;
            }
            int start = i;
            while(i <= count && levels[i] >= levelHigh){
                i++;
            }
            int end = i-1;

            if(start != end) {
                for(int j = 0; j < (end-start+1)/2; j++) {
                    int tmp = visualOrder[start+j];
                    visualOrder[start+j] = visualOrder[end-j];
                    visualOrder[end-j] = tmp;
                }
            }
            i++;
        }
        levelHigh--;
    }
}

std::shared_ptr<LFont> LTextEngine::font(const LScriptItem &si) const
{
    LTextCharFormat f = format(&si);
    std::shared_ptr<LFont> ft = f.font();
    if(ft != nullptr){
        return ft;
    }
    return font();
}

int LTextEngine::length(uint32_t item) const
{
    if(m_layoutData != nullptr){
        if(item < m_layoutData->m_items.size()){
            const LScriptItem &si = m_layoutData->m_items[item];
            int from = si.position;
            item++;
            return (item < m_layoutData->m_items.size() ? m_layoutData->m_items[item].position : m_layoutData->m_string.length()) - from;
        }
    }
    return 0;
}

int LTextEngine::length(const LScriptItem *si) const
{
    if(m_layoutData != nullptr){
        int end;
        if ((si + 1) < m_layoutData->m_items.data()+ m_layoutData->m_items.size()){
            end = (si+1)->position;
        }else{
            end = m_layoutData->m_string.length();
        }
        return end - si->position;
    }
    return 0;
}

void LTextEngine::setFormats(const std::vector<LTextLayout::FormatRange> &formats)
{
    if (formats.empty()) {
        if (m_specialData == nullptr){
            return;
        }
        if (m_specialData->m_preeditText.empty()) {
            m_specialData = nullptr;
        } else {
            m_specialData->m_formats.clear();
        }
    } else {
        if (m_specialData == nullptr){
            m_specialData = std::make_shared<SpecialData>();
            m_specialData->m_preeditPosition = -1;
        }
        m_specialData->m_formats = formats;
        indexFormats();
    }
    invalidate();
    clearLineData();
}

LTextCharFormat LTextEngine::format(const LScriptItem *si) const
{
    const std::shared_ptr<LTextFormatCollection> collection = formatCollection();
    if (collection != nullptr){
        return collection->charFormat(formatIndex(si));
    }
    return LTextCharFormat();
}

int LTextEngine::formatIndex(const LScriptItem *si) const
{
    if (m_specialData!=nullptr && m_layoutData != nullptr) {
        std::shared_ptr<LTextFormatCollection>  collection = formatCollection();
        if(collection != nullptr){
            if(!m_layoutData->m_items.empty()){
                int pos = si - &m_layoutData->m_items.at(0);
                if(pos >=0  && pos <(int)m_specialData->m_resolvedFormats.size()){
                    return collection->indexForFormat(m_specialData->m_resolvedFormats[pos]);
                }
            }
        }
    }

    return -1;
}

void LTextEngine::freeMemory()
{
    if (!m_stackEngine) {
        m_layoutData = nullptr;
    } else {
        m_layoutData->m_used = 0;
        m_layoutData->m_hasBidi = false;
        m_layoutData->m_layoutState = LayoutEmpty;
        m_layoutData->m_haveCharAttributes = false;
        m_layoutData->m_items.clear();
    }
    if (m_specialData){
        m_specialData->m_resolvedFormats.clear();
    }
    for (uint32_t i = 0; i < m_lines.size(); ++i) {
        m_lines[i].justified = 0;
        m_lines[i].gridfitted = 0;
    }
}

int LTextEngine::lineNumberForTextPosition(int pos)
{
    if (m_layoutData == nullptr){
        itemize();
    }
    if (pos == m_layoutData->m_string.length() && m_lines.size()){
        return m_lines.size() - 1;
    }
    for (uint32_t i = 0; i < m_lines.size(); ++i) {
        const LScriptLine& line = m_lines[i];
        if (line.from + line.length + line.trailingSpaces > pos){
            return i;
        }
    }
    return -1;
}

void LTextEngine::drawDecorations(LPainter *painter)
{
    std::shared_ptr<LPen> oldPen = painter->pen();

    adjustUnderlines();
    drawItemDecorations(painter, m_underlines);
    drawItemDecorations(painter, m_strikeOuts);
    drawItemDecorations(painter, m_strikeOuts);

    clearDecorations();
    painter->setPen(oldPen, false);
}

void LTextEngine::clearDecorations()
{
    m_underlines.clear();
    m_strikeOuts.clear();
    m_overlines.clear();
}

void LTextEngine::adjustUnderlines()
{
    if (m_underlines.empty()){
        return;
    }

    int start = 0;
    int end   = m_underlines.size();
    int i = start;

    ItemDecoration& item = m_underlines[0];

    double underlinePos = item.y;
    double penWidth = item.pen->m_width;
    double lastLineEnd = item.x1;

    while (i < end) {
        ItemDecoration& item = m_underlines[i];
        if (lFuzzyCompare(lastLineEnd, item.x1)) { // no gap between underlines
            underlinePos = std::max<double>(underlinePos, item.y);
            penWidth = std::max<double>(penWidth, item.pen->m_width);
        } else { // gap between this and the last underline
            adjustUnderlines(start, i, underlinePos, penWidth);
            start = i;
            underlinePos = item.y;
            penWidth = item.pen->m_width;
        }
        lastLineEnd = item.x2;
        ++i;
    }

    adjustUnderlines(start, end, underlinePos, penWidth);
}

const LCharAttributes *LTextEngine::attributes() const
{
    if (m_layoutData!= nullptr && m_layoutData->m_haveCharAttributes){
        return (LCharAttributes *) m_layoutData->m_memory;
    }

    itemize();
    if (! ensureSpace(m_layoutData->m_string.length())){
        return nullptr;
    }

    lchar::Util::initCharAttributes(reinterpret_cast<const uint16_t *>(m_layoutData->m_string.data()),
                                  m_layoutData->m_string.length(),
                                  (LCharAttributes *)m_layoutData->m_memory,
                                  LCharAttributeOption::DefaultOptionsCompat|LCharAttributeOption::HangulLineBreakTailoring);


    m_layoutData->m_haveCharAttributes = true;
    return (LCharAttributes *) m_layoutData->m_memory;
}

void LTextEngine::shape(int item) const
{
    auto &li = m_layoutData->m_items[item];
    if (li.analysis.flags == LScriptAnalysis::Object) {
        ensureSpace(1);
        // fix log clusters to point to the previous glyph, as the object doesn't have a glyph of it's own.
        // This is required so that all entries in the array get initialized and are ordered correctly.
        if (m_layoutData->m_logClustersPtr) {
            uint16_t *lc = logClusters(&li);
            *lc = (lc != m_layoutData->m_logClustersPtr) ? lc[-1] : 0;
        }
    } else if (li.analysis.flags == LScriptAnalysis::Tab) {
        // set up at least the ascent/descent/leading of the script item for the tab
        std::shared_ptr<LFontEngine> fe = fontEngine();
        li.ascent = fe->ascent();
        li.descent = fe->descent();
        li.leading = fe->leading();

        // see the comment above
        if (m_layoutData->m_logClustersPtr) {
            uint16_t *lc = logClusters(&li);
            *lc = (lc != m_layoutData->m_logClustersPtr) ? lc[-1] : 0;
        }
    } else {
        shapeText(item);
    }
}

double LTextEngine::calculateTabWidth(int item, double x) const
{
    const LScriptItem &si = m_layoutData->m_items[item];

    double dpiScale = font()->m_dpi / LFont::defaultDpiY();

    std::list<LTextOption::Tab> tabs = m_option.m_tabStops;
    if (!tabs.empty()) {
        if (isRightToLeft()) { // rebase the tabArray positions.
            auto isLeftOrRightTab = [](const LTextOption::Tab &tab) {
                return tab.m_type == LTextOption::LeftTab || tab.m_type == LTextOption::RightTab;
            };
            std::list<LTextOption::Tab>::iterator begin = tabs.begin();
            std::list<LTextOption::Tab>::iterator end = tabs.end();
            std::list<LTextOption::Tab>::iterator cit = std::find_if(begin, end, isLeftOrRightTab);
            if (cit != end) {
                std::list<LTextOption::Tab>::iterator iter = cit;
                while (iter != end) {
                    LTextOption::Tab &tab = *iter;
                    if (tab.m_type == LTextOption::LeftTab){
                        tab.m_type = LTextOption::RightTab;
                    }
                    else if (tab.m_type == LTextOption::RightTab){
                        tab.m_type = LTextOption::LeftTab;
                    }
                    ++iter;
                }
            }
        }
        for (const LTextOption::Tab &tabSpec : tabs) {
            double tab = tabSpec.m_position * dpiScale;
            if (tab > x) {  // this is the tab we need.
                int tabSectionEnd = m_layoutData->m_string.size();
                if (tabSpec.m_type == LTextOption::RightTab || tabSpec.m_type == LTextOption::CenterTab) {
                    // find next tab to calculate the width required.
                    tab = tabSpec.m_position;
                    for (int i=item + 1; i < (int)m_layoutData->m_items.size(); i++) {
                        const LScriptItem &sItem = m_layoutData->m_items[i];
                        if (sItem.analysis.flags == LScriptAnalysis::TabOrObject) { // found it.
                            tabSectionEnd = sItem.position;
                            break;
                        }
                    }
                }
                else if (tabSpec.m_type == LTextOption::DelimiterTab)
                    // find delimitor character to calculate the width required
                    tabSectionEnd = std::max<int>(si.position,
                                             m_layoutData->m_string.find_first_of(tabSpec.m_delimiter, si.position) + 1);

                if (tabSectionEnd > si.position) {
                    double length;
                    // Calculate the length of text between this tab and the tabSectionEnd
                    for (int i=item; i < (int)m_layoutData->m_items.size(); i++) {
                        const LScriptItem &sitem = m_layoutData->m_items.at(i);
                        if (sitem.position > tabSectionEnd || sitem.position <= si.position){
                            continue;
                        }
                        shape(i); // first, lets make sure relevant text is already shaped
                        if (sitem.analysis.flags == LScriptAnalysis::Object) {
                            length += sitem.width;
                            continue;
                        }
                        LGlyphLayout glyphs = this->shapedGlyphs(&sitem);
                        const int end = std::min<int>(sitem.position + sitem.numGlyphs, tabSectionEnd) - sitem.position;
                        for (int i=0; i < end; i++){
                            length += glyphs.advances[i] * !glyphs.attributes[i].dontPrint;
                        }
                        if (end + sitem.position == tabSectionEnd && tabSpec.m_type == LTextOption::DelimiterTab){ // remove half of matching char
                            length -= glyphs.advances[end] / 2 * !glyphs.attributes[end].dontPrint;
                        }
                    }

                    switch (tabSpec.m_type) {
                    case LTextOption::CenterTab:{
                        length /= 2;
                        //No break
                    }
                    case LTextOption::DelimiterTab:
                    case LTextOption::RightTab:{
                        tab = tabSpec.m_position * dpiScale - length;
                        if (tab < x){ // default to tab taking no space
                            return 0;
                        }
                        break;
                    }
                    case LTextOption::LeftTab:
                        break;
                    }
                }
                return tab - x;
            }
        }
    }
    double tab = m_option.m_tabStopDistance;
    if (tab <= 0){
        tab = 80; // default
    }
    tab *= dpiScale;
    double nextTabPos = (std::floor(x / tab) + 1) * tab;
    double tabWidth = nextTabPos - x;

    return tabWidth;
}

double LTextEngine::alignLine(const LScriptLine &line)
{
    double x = 0;
    justify(line);

    if (!line.justified && line.width == std::numeric_limits<double>::max()) {
        LFlags<LAlignment> align = m_option.m_align;
        if ((align & LAlignment::Justify) && isRightToLeft()){
            align = LAlignment::Right;
        }
        if (align & LAlignment::Right){
            x = line.width - (line.textAdvance);
        }
        else if (align & LAlignment::HCenter){
            x = (line.width - line.textAdvance)/2;
        }
    }
    return x;
}

double LTextEngine::offsetInLigature(const LScriptItem *si, int pos, int max, int glyph_pos) const
{
    const uint16_t *logClusters = this->logClusters(si);
    const LGlyphLayout &glyphs = shapedGlyphs(si);

    int offsetInCluster = 0;
    for (int i = pos - 1; i >= 0; i--) {
        if (logClusters[i] == glyph_pos){
            offsetInCluster++;
        }else{
            break;
        }
    }

    // in the case that the offset is inside a (multi-character) glyph,
    // interpolate the position.
    if (offsetInCluster > 0) {
        int clusterLength = 0;
        for (int i = pos - offsetInCluster; i < max; i++) {
            if (logClusters[i] == glyph_pos){
                clusterLength++;
            }else{
                break;
            }
        }
        if (clusterLength){
            return glyphs.advances[glyph_pos] * offsetInCluster / clusterLength;
        }
    }

    return 0;
}

std::vector<int> LTextEngine::insertionPointsForLine(int lineNum) const
{
    LTextLineItemIterator iterator(getPtrT<LTextEngine>(), lineNum);

    std::vector<int> insertionPoints;
    insertionPoints.reserve(size_t(iterator.line.length));

    bool lastLine = lineNum >= ((int)m_lines.size() - 1);

    while (!iterator.atEnd()) {
        const LScriptItem &si = iterator.next();

        int end = iterator.itemEnd;
        if (lastLine && iterator.item == iterator.lastItem){
            ++end; // the last item in the last line -> insert eol position
        }
        if (si.analysis.bidiLevel % 2) {
            for (int i = end - 1; i >= iterator.itemStart; --i){
                insertionPoints.push_back(i);
            }
        } else {
            for (int i = iterator.itemStart; i < end; ++i){
                insertionPoints.push_back(i);
            }
        }
    }
    return insertionPoints;
}

int LTextEngine::endOfLine(int lineNum) const
{
    LTextLineItemIterator iterator(getPtrT<LTextEngine>(), lineNum);

    bool lastLine = lineNum >= ((int)m_lines.size() - 1);
    int endV  = 0;
    while (!iterator.atEnd()) {
        const LScriptItem &si = iterator.next();

        int end = iterator.itemEnd;
        if (lastLine && iterator.item == iterator.lastItem){
            ++end; // the last item in the last line -> insert eol position
        }
        if (si.analysis.bidiLevel % 2) {
            int i = iterator.itemStart;
            if(i <= end-1){
                endV = i;
            }
        } else {
            int i = end-1;
            if(i >= iterator.itemStart){
                endV = i;
            }
        }
    }
    return endV;
}

int LTextEngine::beginningOfLine(int lineNum) const
{
    LTextLineItemIterator iterator(getPtrT<LTextEngine>(), lineNum);
    bool lastLine = lineNum >= ((int)m_lines.size() - 1);

    while (!iterator.atEnd()) {
        const LScriptItem &si = iterator.next();

        int end = iterator.itemEnd;
        if (lastLine && iterator.item == iterator.lastItem){
            ++end; // the last item in the last line -> insert eol position
        }
        if (si.analysis.bidiLevel % 2) {
            int i = end - 1;
            if(i >= iterator.itemStart){
                return i;
            }
        } else {
            int i = iterator.itemStart;
            if(i < end){
                return i;
            }
        }
    }
    return 0;
}

int LTextEngine::positionInLigature(const LScriptItem *si, int end,
                                    double x, double edge,
                                    int glyphPos, bool cursorOnCharacter) const
{
    uint16_t *logClusters = this->logClusters(si);
    int clusterStart = -1;
    int clusterLength = 0;

    if (si->analysis.script != lchar::Script_Common &&
        si->analysis.script != lchar::Script_Greek &&
        si->analysis.script != lchar::Script_Latin &&
        si->analysis.script != lchar::Script_Hiragana &&
        si->analysis.script != lchar::Script_Katakana &&
        si->analysis.script != lchar::Script_Bopomofo &&
        si->analysis.script != lchar::Script_Han) {
        if (glyphPos == -1){
            return si->position + end;
        }
        else {
            int i;
            for (i = 0; i < end; i++){
                if (logClusters[i] == glyphPos){
                    break;
                }
            }
            return si->position + i;
        }
    }

    if (glyphPos == -1 && end > 0)
        glyphPos = logClusters[end - 1];
    else {
        if (x <= edge){
            glyphPos--;
        }
    }

    const LCharAttributes *attrs = attributes() + si->position;
    logClusters = this->logClusters(si);
    clusterLength = getClusterLength(logClusters, attrs, 0, end, glyphPos, &clusterStart);

    if (clusterLength) {
        const LGlyphLayout &glyphs = shapedGlyphs(si);
        double glyphWidth = glyphs.effectiveAdvance(glyphPos);
        // the approximate width of each individual element of the ligature
        double perItemWidth = glyphWidth / clusterLength;
        if (perItemWidth <= 0){
            return si->position + clusterStart;
        }
        double left = x > edge ? edge : edge - glyphWidth;
        int n = static_cast<int>(std::floor((x - left) / perItemWidth));
        double dist = x - left - n * perItemWidth;
        int closestItem = dist > (perItemWidth / 2) ? n + 1 : n;
        if (cursorOnCharacter && closestItem > 0){
            closestItem--;
        }
        int pos = clusterStart + closestItem;
        // Jump to the next grapheme boundary
        while (pos < end && !attrs[pos].graphemeBoundary){
            pos++;
        }
        return si->position + pos;
    }
    return si->position + end;

}

int LTextEngine::previousLogicalPosition(int oldPos) const
{
    const LCharAttributes *attrs = attributes();
    int len = m_layoutData->m_string.length();
    if ((attrs ==nullptr) || (oldPos <= 0) || (oldPos > len)){
        return oldPos;
    }

    oldPos--;
    while (oldPos && !attrs[oldPos].graphemeBoundary){
        oldPos--;
    }
    return oldPos;
}

int LTextEngine::nextLogicalPosition(int oldPos) const
{
    const LCharAttributes *attrs = attributes();
    int len = m_layoutData->m_string.length();
    if (!attrs || oldPos < 0 || oldPos >= len){
        return oldPos;
    }

    oldPos++;
    while (oldPos < len && !attrs[oldPos].graphemeBoundary){
        oldPos++;
    }
    return oldPos;
}

bool LTextEngine::atWordSeparator(int position) const
{
    const wchar_t c = m_layoutData->m_string.at(position);
    switch (c) {
    case '.':
    case ',':
    case '?':
    case '!':
    case '@':
    case '#':
    case '$':
    case ':':
    case ';':
    case '-':
    case '<':
    case '>':
    case '[':
    case ']':
    case '(':
    case ')':
    case '{':
    case '}':
    case '=':
    case '/':
    case '+':
    case '%':
    case '&':
    case '^':
    case '*':
    case '\'':
    case '"':
    case '`':
    case '~':
    case '|':
    case '\\':
        return true;
    default:
        break;
    }
    return false;
}

int LTextEngine::positionAfterVisualMovement(int pos, LMoveOper op)
{
    itemize();

    bool moveRight = (op == LMoveOper::Right);
    bool alignRight = isRightToLeft();
    if (!m_layoutData->m_hasBidi){
        return moveRight ^ alignRight ? nextLogicalPosition(pos) : previousLogicalPosition(pos);
    }

    int lineNum = lineNumberForTextPosition(pos);
    if (lineNum < 0){
        return pos;
    }

    const auto insertionPoints = insertionPointsForLine(lineNum);
    for (size_t i = 0, max = insertionPoints.size(); i < max; ++i)
        if (pos == insertionPoints[i]) {
            if (moveRight) {
                if (i + 1 < max){
                    return insertionPoints[i + 1];
                }
            } else {
                if (i > 0){
                    return insertionPoints[i - 1];
                }
            }

            if (moveRight ^ alignRight) {
                if (lineNum + 1 < m_lines.size()){
                    return alignRight ? endOfLine(lineNum + 1) : beginningOfLine(lineNum + 1);
                }
            }
            else {
                if (lineNum > 0){
                    return alignRight ? beginningOfLine(lineNum - 1) : endOfLine(lineNum - 1);
                }
            }

            break;
        }

    return pos;
}

std::wstring LTextEngine::elidedText(LTextElide mode,
                                     const double width,
                                     int flags,
                                     int from,
                                     int count) const
{
    if (flags & (uint32_t)LTextFlag::ShowMnemonic) {
        itemize();
        LCharAttributes *attributes = const_cast<LCharAttributes *>(this->attributes());
        if (!attributes){
            return L"";
        }
        for (int i = 0; i < m_layoutData->m_items.size(); ++i) {
            const LScriptItem &si = m_layoutData->m_items.at(i);
            if (!si.numGlyphs){
                shape(i);
            }

            unsigned short *logClusters = this->logClusters(&si);
            LGlyphLayout glyphs = shapedGlyphs(&si);

            const int end = si.position + length(&si);
            for (int i = si.position; i < end - 1; ++i) {
                if (m_layoutData->m_string.at(i) == wchar_t('&')
                    && !attributes[i + 1].whiteSpace && attributes[i + 1].graphemeBoundary) {
                    const int gp = logClusters[i - si.position];
                    glyphs.attributes[gp].dontPrint = true;
                    // emulate grapheme cluster
                    attributes[i] = attributes[i + 1];
                    memset(attributes + i + 1, 0, sizeof(LCharAttributes));
                    if (m_layoutData->m_string.at(i + 1) == wchar_t('&')){
                        ++i;
                    }
                }
            }
        }
    }

    validate();

    const int to = count >= 0 && count <= m_layoutData->m_string.length() - from
                       ? from + count
                       : m_layoutData->m_string.length();

    if (mode == LTextElide::None
        || this->width(from, m_layoutData->m_string.length()) <= width
        || to - from <= 1){
        return m_layoutData->m_string.substr(from, from - to);
    }

    double ellipsisWidth = 0;
    std::wstring ellipsisText;
    {
        std::shared_ptr<LFontEngine> engine = font()->m_engine;
        wchar_t ellipsisChar(0x2026);

        uint32_t glyph = engine->glyphIndex(ellipsisChar);

        LGlyphLayout glyphs;
        glyphs.numGlyphs = 1;
        glyphs.glyphs = &glyph;
        glyphs.advances = &ellipsisWidth;

        if (glyph != 0) {
            engine->recalcAdvances(&glyphs, { });
            ellipsisText = ellipsisChar;
        } else {
            glyph = engine->glyphIndex('.');
            if (glyph != 0) {
                engine->recalcAdvances(&glyphs, { });

                ellipsisWidth *= 3;
                ellipsisText = L"...";
            }
        }
    }

    const double availableWidth = width - ellipsisWidth;
    if (availableWidth < 0){
        return L"";
    }

    const LCharAttributes *attributes = this->attributes();
    if (!attributes){
        return L"";
    }

    if (mode == LTextElide::Right) {
        double currentWidth = 0;
        int pos = 0;
        int nextBreak = from;

        do {
            pos = nextBreak;

            ++nextBreak;
            while (nextBreak < m_layoutData->m_string.length() && !attributes[nextBreak].graphemeBoundary){
                ++nextBreak;
            }

            currentWidth += this->width(pos, nextBreak - pos);
        } while (nextBreak < to
                 && currentWidth < availableWidth);

        if (nextCharJoins(m_layoutData->m_string, pos)){
            ellipsisText.insert(ellipsisText.begin(), wchar_t(0x200d) /* ZWJ */);
        }

        return stringMidRetainingBidiCC(m_layoutData->m_string,
                                        L"", ellipsisText,
                                        from, to,
                                        from, pos - from);
    } else if (mode == LTextElide::Left) {
        double currentWidth = 0;
        int pos = 0;
        int nextBreak = to;

        do {
            pos = nextBreak;

            --nextBreak;
            while (nextBreak > 0 && !attributes[nextBreak].graphemeBoundary){
                --nextBreak;
            }

            currentWidth += this->width(nextBreak, pos - nextBreak);
        } while (nextBreak > from
                 && currentWidth < availableWidth);

        if (prevCharJoins(m_layoutData->m_string, pos)){
            ellipsisText.push_back(wchar_t(0x200d) /* ZWJ */);
        }

        return stringMidRetainingBidiCC(m_layoutData->m_string,
                                        ellipsisText, std::wstring(),
                                        from, to,
                                        pos, to - pos);
    } else if (mode == LTextElide::Middle) {
        double leftWidth = 0;
        double rightWidth = 0;

        int leftPos = from;
        int nextLeftBreak = from;

        int rightPos = to;
        int nextRightBreak = to;

        do {
            leftPos = nextLeftBreak;
            rightPos = nextRightBreak;

            ++nextLeftBreak;
            while (nextLeftBreak < m_layoutData->m_string.length() && !attributes[nextLeftBreak].graphemeBoundary){
                ++nextLeftBreak;
            }

            --nextRightBreak;
            while (nextRightBreak > from && !attributes[nextRightBreak].graphemeBoundary){
                --nextRightBreak;
            }

            leftWidth += this->width(leftPos, nextLeftBreak - leftPos);
            rightWidth += this->width(nextRightBreak, rightPos - nextRightBreak);
        } while (nextLeftBreak < to
                 && nextRightBreak > from
                 && leftWidth + rightWidth < availableWidth);

        if (nextCharJoins(m_layoutData->m_string, leftPos)){
            ellipsisText.insert(ellipsisText.begin(), wchar_t(0x200d) /* ZWJ */);
        }
        if (prevCharJoins(m_layoutData->m_string, rightPos)){
            ellipsisText.push_back(wchar_t(0x200d) /* ZWJ */);
        }

        return m_layoutData->m_string.substr(from, leftPos - from) + ellipsisText + m_layoutData->m_string.substr(rightPos, to - rightPos);
    }

    return m_layoutData->m_string.substr(from, to - from);
}

const std::shared_ptr<LFont> LTextEngine::font() const
{
    if(m_font != nullptr){
        return m_font;
    }
    return LGuiApplication::font();
}


void LTextEngine::adjustUnderlines(int start,
                                   int end,
                                   double underlinePos,
                                   double penWidth)
{
    for (int i = start; i < end; ++i) {
        ItemDecoration& item = m_underlines[i];
        item.y = underlinePos;
        item.pen->m_width = penWidth;
    }
}


static void applyVisibilityRules(uint16_t ucs,
                                 LGlyphLayout *glyphs,
                                 uint32_t glyphPosition,
                                 std::shared_ptr<LFontEngine> fontEngine)
{
    // hide characters that should normally be invisible
    switch (ucs) {
    case lchar::LineFeed:
    case 0x000c: // FormFeed
    case lchar::CarriageReturn:
    case lchar::LineSeparator:
    case lchar::ParagraphSeparator:{
        glyphs->attributes[glyphPosition].dontPrint = true;
        break;
    }
    case lchar::SoftHyphen:{
        if (!fontEngine->m_symbol) {
            // U+00AD [SOFT HYPHEN] is a default ignorable codepoint,
            // so we replace its glyph and metrics with ones for
            // U+002D [HYPHEN-MINUS] and make it visible if it appears at line-break
            const uint32_t engineIndex = glyphs->glyphs[glyphPosition] & 0xff000000;
            glyphs->glyphs[glyphPosition] = fontEngine->glyphIndex('-');
            if (L_LIKELY(glyphs->glyphs[glyphPosition] != 0)) {
                glyphs->glyphs[glyphPosition] |= engineIndex;
                LGlyphLayout tmp = glyphs->mid(glyphPosition, 1);
                fontEngine->recalcAdvances(&tmp, { });
            }
            glyphs->attributes[glyphPosition].dontPrint = true;
        }
        break;
    }
    default:{
        break;
    }
    }
}


void LTextEngine::shapeText(int item) const
{
    LScriptItem &si = m_layoutData->m_items[item];

    if (si.numGlyphs){
        return;
    }

    si.width = 0;
    si.glyphDataOffset = m_layoutData->m_used;

    const wchar_t *string = m_layoutData->m_string.data() + si.position;
    const int itemLength = length(item);

    std::wstring casedString;
    if (si.analysis.flags && si.analysis.flags <= LScriptAnalysis::SmallCaps) {
        casedString.resize(itemLength);
        wchar_t *uc = casedString.data();
        for (int i = 0; i < itemLength; ++i) {
            uint32_t ucs4 = string[i];
            if (LStringUtil::isHighSurrogate(ucs4) && (i + 1) < itemLength) {
                uint32_t low = string[i + 1];
                if (LStringUtil::isLowSurrogate(low)) {
                    // high part never changes in simple casing
                    uc[i] = ucs4;
                    ++i;
                    ucs4 = LStringUtil::surrogateToUcs4(ucs4, low);
                    ucs4 = si.analysis.flags == LScriptAnalysis::Lowercase ? LStringUtil::toLower(ucs4)
                                                                           : LStringUtil::toUpper(ucs4);
                    uc[i] = LStringUtil::lowSurrogate(ucs4);
                }
            } else {
                uc[i] = si.analysis.flags == LScriptAnalysis::Lowercase ? LStringUtil::toLower(ucs4)
                                                                        : LStringUtil::toUpper(ucs4);
            }
        }
        string = casedString.data();
    }

    if (L_UNLIKELY(!ensureSpace(itemLength))) {
        return;
    }

    std::shared_ptr<LFontEngine> fe = fontEngine();
    si.ascent = fe->ascent();
    si.descent = fe->descent();
    si.leading = fe->leading();

    std::shared_ptr<LFont> font = this->font(si);
    bool letterSpacingIsAbsolute = font->m_letterSpacingIsAbsolute;
    double letterSpacing = font->m_letterSpacing;
    double wordSpacing = font->m_wordSpacing;


    if (letterSpacingIsAbsolute && (lFuzzyIsZero(letterSpacing) == false)){
        letterSpacing *= font->m_dpi / LFont::defaultDpiY();
    }

    // split up the item into parts that come from different font engines
    // k * 3 entries, array[k] == index in string, array[k + 1] == index in glyphs, array[k + 2] == engine index
    std::vector<uint32_t> itemBoundaries;
    itemBoundaries.reserve(24);

    LGlyphLayout initialGlyphs = availableGlyphs(&si);
    int nGlyphs = initialGlyphs.numGlyphs;

    // ask the font engine to find out which glyphs (as an index in the specific font)
    // to use for the text in one item.
    fe->stringToCMap(string, itemLength, &initialGlyphs, &nGlyphs, 0);

    uint16_t *pLogCluster = logClusters(&si);

    int glyphPos = 0;
    for (int i = 0; i < itemLength; ++i, ++glyphPos) {
        pLogCluster[i] = glyphPos;
        initialGlyphs.attributes[glyphPos].clusterStart = true;
        if (LStringUtil::isHighSurrogate(string[i])
            && i + 1 < itemLength
            && LStringUtil::isLowSurrogate(string[i + 1])) {
            initialGlyphs.attributes[glyphPos].dontPrint = !LStringUtil::isPrint(LStringUtil::surrogateToUcs4(string[i], string[i + 1]));
            ++i;
            pLogCluster[i] = glyphPos;

        } else {
            initialGlyphs.attributes[glyphPos].dontPrint = !LStringUtil::isPrint(string[i]);
        }

        if (L_UNLIKELY(!initialGlyphs.attributes[glyphPos].dontPrint)) {
            applyVisibilityRules(string[i], &initialGlyphs, glyphPos, fe);
        }
    }

    si.numGlyphs = glyphPos;

    if (L_UNLIKELY(si.numGlyphs == 0)) {
        return;
    }
    m_layoutData->m_used += si.numGlyphs;
    LGlyphLayout glyphs = shapedGlyphs(&si);


    if (letterSpacing != 0) {
        for (int i = 1; i < si.numGlyphs; ++i) {
            if (glyphs.attributes[i].clusterStart) {
                if (letterSpacingIsAbsolute)
                    glyphs.advances[i - 1] += letterSpacing;
                else {
                    double &advance = glyphs.advances[i - 1];
                    advance += (letterSpacing - 100) * advance / 100;
                }
            }
        }
        if (letterSpacingIsAbsolute)
            glyphs.advances[si.numGlyphs - 1] += letterSpacing;
        else {
            double &advance = glyphs.advances[si.numGlyphs - 1];
            advance += (letterSpacing - 100) * advance / 100;
        }
    }
    if (wordSpacing != 0) {
        for (int i = 0; i < si.numGlyphs; ++i) {
            if (glyphs.attributes[i].justification == Justification_Space
                || glyphs.attributes[i].justification == Justification_Arabic_Space) {
                // word spacing only gets added once to a consecutive run of spaces (see CSS spec)
                if (i + 1 == si.numGlyphs
                    ||(glyphs.attributes[i+1].justification != Justification_Space
                        && glyphs.attributes[i+1].justification != Justification_Arabic_Space)){
                    glyphs.advances[i] += wordSpacing;
                }
            }
        }
    }

    for (int i = 0; i < si.numGlyphs; ++i){
        si.width += glyphs.advances[i] * !glyphs.attributes[i].dontPrint;
    }

}

LStackTextEngine::LStackTextEngine(const std::wstring &str, const std::shared_ptr<LFont> f)
: LTextEngine(str, f)
{
    m_stackEngine = true;
    m_layoutData = std::make_shared<LayoutData>();
    m_layoutData->m_string = str;
}

LTextLineItemIterator::LTextLineItemIterator(std::shared_ptr<LTextEngine> _eng,
                                             int _lineNum,
                                             const LPoint<double> &pos,
                                             const LTextLayout::FormatRange *_selection)
    : eng(_eng),
    line(eng->m_lines[_lineNum]),
    si(nullptr),
    lineNum(_lineNum),
    lineEnd(line.from + line.length),
    firstItem(eng->findItem(line.from)),
    lastItem(eng->findItem(lineEnd - 1, firstItem)),
    nItems((firstItem >= 0 && lastItem >= firstItem)? (lastItem-firstItem+1) : 0),
    logicalItem(-1),
    item(-1),
    itemLength(0),
    glyphsStart(0),
    glyphsEnd(0),
    itemStart(0),
    itemEnd(0),
    itemWidth(0),
    visualOrder(nItems),
    selection(_selection)
{

    x = pos.m_x;
    x += line.x;
    x += eng->alignLine(line);

    std::vector<uint8_t> levels(nItems);
    for (int i = 0; i < nItems; ++i){
        levels[i] = eng->m_layoutData->m_items.at(i + firstItem).analysis.bidiLevel;
    }
    LTextEngine::bidiReorder(nItems, levels.data(), visualOrder.data());
    eng->shapeLine(line);
}

LScriptItem &LTextLineItemIterator::next()
{
    x += itemWidth;

    ++logicalItem;
    item = visualOrder[logicalItem] + firstItem;
    itemLength = eng->length(item);
    si = &eng->m_layoutData->m_items[item];
    if (!si->numGlyphs){
        eng->shape(item);
    }

    itemStart = std::max(line.from, si->position);
    itemEnd = std::min(lineEnd, si->position + itemLength);

    if (si->analysis.flags >= LScriptAnalysis::TabOrObject) {
        glyphsStart = 0;
        glyphsEnd = 1;
        itemWidth = si->width;
        return *si;
    }

    const uint16_t *logClusters = eng->logClusters(si);
    LGlyphLayout glyphs = eng->shapedGlyphs(si);

    glyphsStart = logClusters[itemStart - si->position];
    glyphsEnd = (itemEnd == si->position + itemLength) ? si->numGlyphs : logClusters[itemEnd - si->position];

    // show soft-hyphen at line-break
    if (si->position + itemLength >= lineEnd
        && eng->m_layoutData->m_string.at(lineEnd - 1) == lchar::SoftHyphen){
        glyphs.attributes[glyphsEnd - 1].dontPrint = false;
    }

    itemWidth = 0;
    for (int g = glyphsStart; g < glyphsEnd; ++g){
        itemWidth += glyphs.effectiveAdvance(g);
    }

    return *si;
}

bool LTextLineItemIterator::getSelectionBounds(double *selectionX, double *selectionWidth) const
{
    *selectionX = *selectionWidth = 0;

    if (!selection){
        return false;
    }

    if (si->analysis.flags >= LScriptAnalysis::TabOrObject) {
        if (si->position >= selection->start + selection->length
            || si->position + itemLength <= selection->start)
            return false;

        *selectionX = x;
        *selectionWidth = itemWidth;
    } else {
        const uint16_t *logClusters = eng->logClusters(si);
        LGlyphLayout glyphs = eng->shapedGlyphs(si);

        int from = std::max(itemStart, selection->start) - si->position;
        int to = std::min(itemEnd, selection->start + selection->length) - si->position;
        if (from >= to)
            return false;

        int start_glyph = logClusters[from];
        int end_glyph = (to == itemLength) ? si->numGlyphs : logClusters[to];
        double soff = 0;
        double swidth = 0;
        if (si->analysis.bidiLevel %2) {
            for (int g = glyphsEnd - 1; g >= end_glyph; --g){
                soff += glyphs.effectiveAdvance(g);
            }
            for (int g = end_glyph - 1; g >= start_glyph; --g){
                swidth += glyphs.effectiveAdvance(g);
            }
        } else {
            for (int g = glyphsStart; g < start_glyph; ++g){
                soff += glyphs.effectiveAdvance(g);
            }
            for (int g = start_glyph; g < end_glyph; ++g){
                swidth += glyphs.effectiveAdvance(g);
            }
        }

        // If the starting character is in the middle of a ligature,
        // selection should only contain the right part of that ligature
        // glyph, so we need to get the width of the left part here and
        // add it to *selectionX
        double leftOffsetInLigature = eng->offsetInLigature(si, from, to, start_glyph);
        *selectionX = x + soff + leftOffsetInLigature;
        *selectionWidth = swidth - leftOffsetInLigature;
        // If the ending character is also part of a ligature, swidth does
        // not contain that part yet, we also need to find out the width of
        // that left part
        *selectionWidth += eng->offsetInLigature(si, to, itemLength, end_glyph);
    }
    return true;
}

LK_NAMESPACE_END
