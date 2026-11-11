#include "leditbase.h"
#include "leditcontainer.h"
#include "lmultilineeditor.h"
#include "lkwidgets/core/lapplication.h"

LK_NAMESPACE_BEGIN

double LEditBase::m_minFontSize = 8;
double LEditBase::m_scriptSizeMultiplier = 0.5;

LEditBase::LEditBase()
{

}

int LEditBase::compareIndexes(const std::vector<int> &i1, const std::vector<int> &i2)
{
    int len = std::min(i1.size(), i2.size());
    for(int i=0; i<len; ++i){
        if(i1[i] != i2[i]){
            return i1[i] - i2[i];
        }
    }
    return -((int)i1.size() - (int)i2.size());
}

LCbor::map LEditBase::toCbor(bool filterSelect) const
{
    LCbor::map cborMap;
    cborMap[int(CborId::Type)] = type();
    if(m_style != nullptr){
        cborMap[int(CborId::Style)] = m_style->toCbor();
    }
    return cborMap;
}

bool LEditBase::fromCbor(const LCbor::map &cborMap,
                         bool checkType,
                         bool newStyle,
                         std::shared_ptr<LEditDoc>)
{
    if(checkType){
        LCbor::map::const_iterator mapIter = cborMap.find(uint32_t(CborId::Type));
        if(mapIter != cborMap.end()){
            const LCbor& cbor = mapIter->second;
            const uint32_t ttype = (uint32_t)cbor.toUInt64();
            if(type() != ttype){
                return false;
            }
        }
    }

    LCbor::map::const_iterator mapIter = cborMap.find(uint32_t(CborId::Style));
    if(mapIter != cborMap.end()){
        if(m_style != nullptr){
            m_style->fromCbor(mapIter->second);
        }else if(newStyle){
            m_style = std::make_shared<LStyle>();
            m_style->fromCbor(mapIter->second);
        }
    }
    return true;
}


bool LEditBase::isText() const
{
    return false;
}

bool LEditBase::isContainer() const
{
    return false;
}

bool LEditBase::isRow() const
{
    return false;
}

uint32_t LEditBase::type() const
{
    return uint32_t(LEditType::Unknown);
}

void LEditBase::draw(LPainter *painter,
                     const LPoint<double> &pos,
                     int flags,
                     const LRect<double> &clip) const
{

}

void LEditBase::drawCursor(LPainter *p,
                           const LPoint<double> &pos,
                           int width,
                           const LRect<double> &clip) const
{

}

bool LEditBase::isAtStart() const
{
    return true;
}

bool LEditBase::isAtEnd() const
{
    return true;
}

bool LEditBase::hasSelect() const
{
    if(m_cursorState == NoSelect){
        return false;
    }
    return true;
}

bool LEditBase::isAllSelect() const
{
    if(m_cursorState == NoSelect){
        return false;
    }

    int min = minIndex();
    int max = maxIndex();

    if((m_partSelectStartId == min)
        && (m_partSelectEndId == max)){
        return true;
    }

    if((m_partSelectStartId == max)
        && (m_partSelectEndId == min)){
        return true;
    }
    return false;
}

void LEditBase::deSelect()
{
    m_cursorState = NoSelect;
}

void LEditBase::selectAll(bool isLeftToRight)
{
    m_cursorState = PartialSelect;
    if(isLeftToRight){
        m_partSelectStartId = minIndex();
        m_partSelectEndId = maxIndex();
    }else{
        m_partSelectStartId = maxIndex();
        m_partSelectEndId = minIndex();
    }
}

std::list<std::shared_ptr<LEditOp> > LEditBase::removeSelection()
{
    std::list<std::shared_ptr<LEditOp> > ret;
    if(m_cursorState == NoSelect){
        return ret;
    }

    m_partSelectEndId = 0;
    m_partSelectStartId = 0;
    m_cursorState = NoSelect;
    return ret;

}

bool LEditBase::moveCursor(LMoveOper oper) { return false; }

bool LEditBase::mouseClick(const LPoint<double> &p)
{
    return setToActivePoint(p);
}

LPoint<double> LEditBase::activePoint() const
{
    if(isAtStart()){
        return LPoint<double>(m_rect.m_x, midY());
    }
    return LPoint<double>(m_rect.m_x + m_rect.width(), midY());
}

bool LEditBase::setToActivePoint(const LPoint<double> &p)
{
    if(p.m_x < midX()){
        moveToStart();
    }else{
        moveToEnd();
    }
    return true;
}

bool LEditBase::isStretchX() const
{
    return false;
}

bool LEditBase::isStretchY() const
{
    return false;
}

double LEditBase::upHeight() const
{
    if(lFuzzyIsZero(m_rect.height())){
        return 0;
    }
    return m_rect.height()*0.5 + marginTop();
}

double LEditBase::downHeight() const
{
    return fullHeight() - upHeight();
}

std::shared_ptr<LPalette> LEditBase::palette() const
{
    std::shared_ptr<LIEditor>  e = editor();
    if(e != nullptr){
        return e->palette();
    }
    return LGuiApplication::palette();
}

//uint8_t LEditBase::cursorWidth() const
//{
//    std::shared_ptr<LIEditor>  e = editor();
//    if(e != nullptr){
//        return e->m_cursorWidth;
//    }
//    return 1;
//}

bool LEditBase::blinkStatus() const
{
    std::shared_ptr<LIEditor>  e = editor();
    if(e != nullptr){
        return e->blinkStatus();
    }
    return false;
}

const std::shared_ptr<LFont> LEditBase::font() const
{
    std::shared_ptr<LIEditor>  e = editor();
    if(e != nullptr){
        std::shared_ptr<LFont> f =  e->font();
        if(e->hasScriptFontSize()){
            return scriptFont(f);
        }else{
            return f;
        }
    }
    return LApplication::font();
}

const std::shared_ptr<LFont> LEditBase::scriptFont(std::shared_ptr<LFont> f) const
{
    if(f == nullptr){
        f = font();
    }

    double fontSize = scriptFontSize(f->pointSizeF(), scriptLevel());
    if(lFuzzyIsZero(f->pointSizeF() - fontSize)){
        return f;
    }
    std::shared_ptr<LFont> ret = std::make_shared<LFont>(f);
    ret->setPointSizeF(fontSize);
    return ret;
}

double LEditBase::scriptFontSize(double ps, int sl)
{
    if ( sl >= 0 )
    {
        for ( int i = 0; i < sl; ++i ){
            ps = ps * m_scriptSizeMultiplier;
        }
    }
    else
    {
        for ( int i = 0; i > sl; --i ){
            ps = ps /m_scriptSizeMultiplier;
        }
    }
    if ( ps < m_minFontSize ){
        ps = m_minFontSize;
    }
    return ps;
}

double LEditBase::mmlSpacing(LMathMLSpacing spaceType, double baseSize, int level)
{
    switch (spaceType) {
    case LMathMLSpacing::Veryverythinmathspace:{
        return scriptFontSize(baseSize, level) * 0.0555556;
    }
    case LMathMLSpacing::Verythinmathspace:{
        return scriptFontSize(baseSize, level) * 0.111111;
    }
    case LMathMLSpacing::Thinmathspace:{
        return scriptFontSize(baseSize, level) * 0.166667;
    }
    case LMathMLSpacing::Mediummathspace:{
        return scriptFontSize(baseSize, level) * 0.222222;
    }
    case LMathMLSpacing::Thickmathspace:{
        return scriptFontSize(baseSize, level) * 0.277778;
    }
    case LMathMLSpacing::Verythickmathspace:{
        return scriptFontSize(baseSize, level) * 0.333333;
    }
    case LMathMLSpacing::Veryverythickmathspace:{
        return scriptFontSize(baseSize, level) * 0.388889;
    }
    case LMathMLSpacing::NegativeVeryverythinmathspace:{
        return -scriptFontSize(baseSize, level) * 0.0555556;
    }
    case LMathMLSpacing::NegativeVerythinmathspace:{
        return -scriptFontSize(baseSize, level) * 0.111111;
    }
    case LMathMLSpacing::NegativeThinmathspace:{
        return -scriptFontSize(baseSize, level) * 0.166667;
    }
    case LMathMLSpacing::NegativeMediummathspace:{
        return -scriptFontSize(baseSize, level) * 0.222222;
    }
    case LMathMLSpacing::NegativeThickmathspace:{
        return -scriptFontSize(baseSize, level) * 0.277778;
    }
    case LMathMLSpacing::NegativeVerythickmathspace:{
        return -scriptFontSize(baseSize, level) * 0.333333;
    }
    case LMathMLSpacing::NegativeVeryverythickmathspace:{
        return -scriptFontSize(baseSize, level) * 0.388889;
    }
    default:{
        return 0;
    }
    }
}

const LMargins<double> &LEditBase::margins() const
{
    return defaultMargins();
}

void LEditBase::setMargins(const LMargins<double> &v)
{

}

LMargins<double> &LEditBase::defaultMargins()
{
    static LMargins<double> t;
    return t;
}

void LEditBase::buildIndexes(std::vector<int> &indexes) const
{
    std::shared_ptr<LEditContainer> parent = m_parent.lock();
    if(parent != nullptr){
        parent->buildIndexes(indexes);
        std::shared_ptr<LEditBase> b = getEditT<LEditBase>();
        indexes.push_back(parent->childIndex(b));
        if(isContainer()){
            if(isAtStart()){
                indexes.push_back(-1);
            }
        }
    }
}

std::vector<int> LEditBase::fullIndexes() const
{
    std::vector<int> indexes;
    buildIndexes(indexes);
    return indexes;
}

std::string LEditBase::indexesString(const std::vector<int> &indexes)
{
    std::ostringstream result;
    std::vector<int>::const_iterator iter = indexes.cbegin();
    if(iter != indexes.cend()){
        result << *iter;
        ++iter;
        for(; iter != indexes.cend(); ++iter){
            result << '.' << *iter;
        }
    }
    return result.str();
}

double LEditBase::fullWidth() const
{
    if(lFuzzyIsZero(m_rect.width())){
        return 0;
    }
    return baseFullWidth();

}

double LEditBase::fullHeight() const
{
    if(lFuzzyIsZero(m_rect.height())){
        return 0;
    }
    return baseFullHeight();
}

std::shared_ptr<LIEditor> LEditBase::editor() const
{
    std::shared_ptr<LEditContainer> t = m_parent.lock();
    if(t != nullptr){
        return t->editor();
    }
    return nullptr;
}

std::shared_ptr<LStyle> LEditBase::style()
{
    if(m_style != nullptr){
        return m_style;
    }
    std::shared_ptr<LEditContainer> t = m_parent.lock();
    if(t != nullptr){
        return t->style();
    }
    return LApplication::style();
}

void LEditBase::setStyle(std::shared_ptr<LStyle> s)
{
    if(s != style()){
        m_style = s;
    }
}


bool LEditBase::isMathMLDisplayStyle() const
{
    if(m_style == nullptr){
        return false;
    }
    return m_style->get(&LMathMLStyle::displaystyle);
}

int LEditBase::stylecSriptlevel() const
{
    if(m_style == nullptr){
        return false;
    }
    return m_style->get(&LMathMLStyle::sriptLevel);
}

int LEditBase::scriptLevel() const
{
    std::shared_ptr<LEditContainer> parent = m_parent.lock();
    if(parent != nullptr){
        return parent->scriptLevel() + m_levelOffset + stylecSriptlevel();
    }
    return m_levelOffset;
}

std::shared_ptr<LEditBase> LEditBase::startBase()
{
    static std::shared_ptr<LEditBase> tStart = std::make_shared<LEditBase>();
    return tStart;
}

bool LEditBase::isValid(std::shared_ptr<LEditBase> base)
{
    return base != nullptr && base != startBase();
}

bool LEditBase::isStart(std::shared_ptr<LEditBase> base)
{
    return base == startBase();
}


void LEditBase::selectFrom(const std::vector<int> &indexes, int level, uint8_t type)
{
    m_cursorState = PartialSelect;
    m_partSelectStartId = indexes[level];

    int min = minIndex();
    int max = maxIndex();
    switch (type) {
    case SelectToLeft:{
        m_partSelectEndId = min;
        break;
    }
    case SelectToRight:{
        m_partSelectEndId = max;
        break;
    }
    default:{
        m_partSelectEndId = m_partSelectStartId;
        break;
    }
    }
}

void LEditBase::selectTo(const std::vector<int> &indexes, int level, uint8_t type)
{
    m_cursorState = PartialSelect;
    m_partSelectEndId = indexes[level];

    int min = minIndex();
    int max = maxIndex();
    switch (type) {
    case SelectToLeft:{
        m_partSelectStartId = max;
        break;
    }
    case SelectToRight:{
        m_partSelectStartId = min;
        break;
    }
    default:{
        m_partSelectStartId = m_partSelectEndId;
        break;
    }
    }
}

void LEditBase::selectRange(const std::vector<int> &start,
                            const std::vector<int> &end, int level, uint8_t type)
{
    m_cursorState = PartialSelect;
    int min = minIndex();
    int max = maxIndex();

    switch (type) {
    case SelectToLeft:{
        if(level >=0){
            if(level<(int)start.size()){
                m_partSelectStartId = start[level];
            }else{
                m_partSelectStartId = max;
            }

            if(level<(int)end.size()){
                m_partSelectEndId = end[level];
            }else{
                m_partSelectEndId = min;
            }
        }else{
            m_partSelectStartId = max;
            m_partSelectEndId = min;
        }
        break;
    }
    default:{
        if(level >=0){
            if(level<(int)start.size()){
                m_partSelectStartId = start[level];
            }else{
                m_partSelectStartId = min;
            }

            if(level<(int)end.size()){
                m_partSelectEndId = end[level];
            }else{
                m_partSelectEndId = max;
            }
        }else{
            m_partSelectStartId = min;
            m_partSelectEndId = max;
        }
        break;
    }
    }
}

void LEditBase::clear()
{

}

void LEditBase::stretchSize()
{
    std::shared_ptr<LEditContainer> parent = m_parent.lock();
    if(parent != nullptr){
        if(parent->canStrechChild(getEditT<LEditBase>())){
            bool stretchX = isStretchX();
            bool stretchY = isStretchY();
            if(stretchX || stretchY){
                LRect<double> t = m_rect;
                LRect<double> pRect = parent->m_rect;
                if(stretchY){
                    if(pRect.height() > baseFullHeight()){
                        t.setHeight(pRect.height() - marginTop() - marginBottom());
                    }
                }

                if(stretchX){
                    if(pRect.width() > baseFullWidth()){
                        t.setWidth(pRect.width() - marginLeft() - marginRight());
                    }
                }

                m_rect = t;
            }
        }
    }
}

void LEditBase::calculateSize()
{

}

void LEditBase::markSelectStart()
{
    if((m_cursorState & PartialSelect) == NoSelect){
        m_cursorState = PartialSelect;
        m_partSelectStartId = activePos();
    }
}

LK_NAMESPACE_END
