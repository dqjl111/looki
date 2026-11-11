#include "ltextboundaryfinder.h"

LK_NAMESPACE_BEGIN

static void init(LTextBoundaryFinder::BoundaryType type,
                 const wchar_t *chars,
                 int length,
                 LCharAttributes *attributes)
{
    const uint16_t *string = reinterpret_cast<const uint16_t *>(chars);

    LFlags<LCharAttributeOption> options;
    switch (type) {
    case LTextBoundaryFinder::Grapheme:{
        options |= LCharAttributeOption::GraphemeBreaks;
        break;
    }
    case LTextBoundaryFinder::Word:{
        options |= LCharAttributeOption::WordBreaks;
        break;
    }
    case LTextBoundaryFinder::Sentence:{
        options |= LCharAttributeOption::SentenceBreaks;
        break;
    }
    case LTextBoundaryFinder::Line:{
        options |= LCharAttributeOption::LineBreaks;
        break;
    }
    default:{
        break;
    }
    }
    lchar::Util::initCharAttributes(string, length, attributes, options);
}



LTextBoundaryFinder::LTextBoundaryFinder()
{

}

LTextBoundaryFinder::LTextBoundaryFinder(BoundaryType type, const std::wstring &string)
    : t(type)
    , s(string)
    , chars(string.data())
    , length(string.length())
    , pos(0)
    , freePrivate(true)
{
    attributes.resize(length+1);
    init(t, chars, length, attributes.data());
}

LTextBoundaryFinder::~LTextBoundaryFinder()
{

}


std::wstring LTextBoundaryFinder::string() const
{
    if (chars == s.data() && length == s.length()){
        return s;
    }
    return std::wstring(chars, length);
}

void LTextBoundaryFinder::toStart()
{
    pos = 0;
}

void LTextBoundaryFinder::toEnd()
{
    pos = length;
}

int LTextBoundaryFinder::position() const
{
    return pos;
}

void LTextBoundaryFinder::setPosition(int position)
{
    pos = lBound(0, position, length);
}

int LTextBoundaryFinder::toNextBoundary()
{
    if (attributes.size() != length || pos < 0 || pos >= length) {
        pos = -1;
        return pos;
    }

    ++pos;
    switch(t) {
    case Grapheme:{
        while (pos < length && !attributes[pos].graphemeBoundary){
            ++pos;
        }
        break;
    }
    case Word:{
        while (pos < length && !attributes[pos].wordBreak){
            ++pos;
        }
        break;
    }
    case Sentence:{
        while (pos < length && !attributes[pos].sentenceBoundary){
            ++pos;
        }
        break;
    }
    case Line:{
        while (pos < length && !attributes[pos].lineBreak){
            ++pos;
        }
        break;
    }
    default:{
        break;
    }
    }

    return pos;
}

int LTextBoundaryFinder::toPreviousBoundary()
{
    if (attributes.size() != length || pos <= 0 || pos > length) {
        pos = -1;
        return pos;
    }

    --pos;
    switch(t) {
    case Grapheme:{
        while (pos > 0 && !attributes[pos].graphemeBoundary){
            --pos;
        }
        break;
    }
    case Word:{
        while (pos > 0 && !attributes[pos].wordBreak){
            --pos;
        }
        break;
    }
    case Sentence:{
        while (pos > 0 && !attributes[pos].sentenceBoundary){
            --pos;
        }
        break;
    }
    case Line:{
        while (pos > 0 && !attributes[pos].lineBreak){
            --pos;
        }
        break;
    }
    default:{
        break;
    }
    }
    return pos;
}

bool LTextBoundaryFinder::isAtBoundary() const
{
    if (attributes.size() != length || pos < 0 || pos > length){
        return false;
    }

    switch(t) {
    case Grapheme:{
        return attributes[pos].graphemeBoundary;
    }
    case Word:{
        return attributes[pos].wordBreak;
    }
    case Sentence:{
        return attributes[pos].sentenceBoundary;
    }
    case Line:{
        // ### TR#14 LB2 prohibits break at sot
        return attributes[pos].lineBreak || pos == 0;
    }
    default:{
        break;
    }
    }
    return false;
}

LFlags<LTextBoundaryReason> LTextBoundaryFinder::boundaryReasons() const
{
    LFlags<LTextBoundaryReason> reasons = LTextBoundaryReason::NotAtBoundary;
    if (attributes.size() != length || pos < 0 || pos > length){
        return reasons;
    }

    const LCharAttributes attr = attributes[pos];
    switch (t) {
    case Grapheme:{
        if (attr.graphemeBoundary) {
            reasons |= LTextBoundaryReason::BreakOpportunity
                       | LTextBoundaryReason::StartOfItem
                       | LTextBoundaryReason::EndOfItem;
            if (pos == 0)
                reasons &= ~uint16_t(LTextBoundaryReason::EndOfItem);
            else if (pos == length)
                reasons &= ~uint16_t(LTextBoundaryReason::StartOfItem);
        }
        break;
    }
    case Word:{
        if (attr.wordBreak) {
            reasons |= LTextBoundaryReason::BreakOpportunity;
            if (attr.wordStart){
                reasons |= LTextBoundaryReason::StartOfItem;
            }
            if (attr.wordEnd){
                reasons |= LTextBoundaryReason::EndOfItem;
            }
        }
        break;
    }
    case Sentence:{
        if (attr.sentenceBoundary) {
            reasons |= LTextBoundaryReason::BreakOpportunity
                       | LTextBoundaryReason::StartOfItem
                       | LTextBoundaryReason::EndOfItem;
            if (pos == 0){
                reasons &= ~uint16_t(LTextBoundaryReason::EndOfItem);
            }
            else if (pos == length){
                reasons &= ~uint16_t(LTextBoundaryReason::StartOfItem);
            }
        }
        break;
    }
    case Line:{
        // ### TR#14 LB2 prohibits break at sot
        if (attr.lineBreak || pos == 0) {
            reasons |= LTextBoundaryReason::BreakOpportunity;
            if (attr.mandatoryBreak || pos == 0) {
                reasons |= LTextBoundaryReason::MandatoryBreak
                           | LTextBoundaryReason::StartOfItem
                           | LTextBoundaryReason::EndOfItem;
                if (pos == 0){
                    reasons &= ~uint16_t(LTextBoundaryReason::EndOfItem);
                }
                else if (pos == length){
                    reasons &= ~uint16_t(LTextBoundaryReason::StartOfItem);
                }
            } else if (pos > 0 && chars[pos - 1] == lchar::SoftHyphen) {
                reasons |= lchar::SoftHyphen;
            }
        }
        break;
    }
    default:{
        break;
    }
    }

    return reasons;
}

LK_NAMESPACE_END
