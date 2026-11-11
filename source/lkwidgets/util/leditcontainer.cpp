#include "leditcontainer.h"
#include "leditop.h"
#include "leditrow.h"
#include "leditdoc.h"

LK_NAMESPACE_BEGIN

LEditContainer::LEditContainer()
{

}

bool LEditContainer::isDynChild() const
{
    return false;
}

bool LEditContainer::isContainer() const
{
    return true;
}

uint32_t LEditContainer::type() const
{
    return uint32_t(LEditType::Container);
}

LCbor::map LEditContainer::toCbor(bool filterSelect) const
{
    LCbor::map ret = LEditBase::toCbor(filterSelect);
    LCbor::array arr;

    if(filterSelect){
        if(isAllSelect()){
            for(const std::shared_ptr<LEditBase>& b: m_children){
                arr.push_back(b->toCbor(filterSelect));
            }
        }else{
            if(isDynChild()){
                for(const std::shared_ptr<LEditBase>& b: m_children){
                    if(b->m_cursorState != NoSelect){
                        arr.push_back(b->toCbor(filterSelect));
                    }
                }
            }else{
                for(const std::shared_ptr<LEditBase>& b: m_children){
                    arr.push_back(b->toCbor(filterSelect));
                }
            }
        }
    }else{
        for(const  std::shared_ptr<LEditBase>& b: m_children){
            arr.push_back(b->toCbor(filterSelect));
        }
    }

    ret[uint32_t(CborId::Children)] = arr;
    return ret;
}

LCbor::array LEditContainer::childrenCbor(int pos, int len)
{
    LCbor::array arr;
    if(pos >= (int)m_children.size()){
        return arr;
    }

    std::list<std::shared_ptr<LEditBase> >::const_iterator iter =  std::next(m_children.cbegin(), pos);
    for(int i=0; i<len; ++i, ++iter){
        if(iter == m_children.cend()){
            break;
        }else{
            arr.push_back((*iter)->toCbor(false));
        }
    }
    return arr;
}

bool LEditContainer::fromCbor(const LCbor::map &cborMap,
                              bool checkType,
                              bool newStyle,
                              std::shared_ptr<LEditDoc> pDoc)
{
    if(LEditBase::fromCbor(cborMap, checkType, newStyle, pDoc)){
        clear();

        LCbor::map::const_iterator mapIter = cborMap.find(uint32_t(CborId::Children));
        if(mapIter != cborMap.end()){
            std::shared_ptr<LEditContainer> tThis = getEditT<LEditContainer>();

            const LCbor& ccbor = mapIter->second;
            const LCbor::array cborArr = ccbor.toArray();
            LCbor::array::const_iterator iter = cborArr.begin();
            for(; iter != cborArr.end(); ++iter){
                std::shared_ptr<LEditBase> b = LEditDoc::createBase(*iter, true, pDoc);
                if(b != nullptr){
                    b->m_parent = tThis;
                    m_children.push_back(b);
                }
            }
        }
        return true;
    }
    return false;
}

std::shared_ptr<LEditBase> LEditContainer::firstBlock() const
{
    if(m_children.empty()){
        return nullptr;
    }
    return m_children.front();
}

std::shared_ptr<LEditBase> LEditContainer::nextBlock() const
{
    if(isAtStart()){
        return firstBlock();
    }else if(isAtEnd()){
        return nullptr;
    }else{
        int i= lItemIndex(m_children, m_activeChild);
        if(i>=0 && (i+1) < (int)m_children.size()){
            return *std::next(m_children.cbegin(),i+1);
        }else{
            return nullptr;
        }
    }
}

std::shared_ptr<LEditBase> LEditContainer::previousBlock() const
{
    if(isAtStart()){
        return nullptr;
    }else if(isAtEnd()){
        return lastBlock();
    }else{
        int i= lItemIndex(m_children, m_activeChild);;
        if((i-1)>=0){
            return *std::next(m_children.cbegin(),i-1);
        }else{
            return nullptr;
        }
    }
}

std::shared_ptr<LEditBase> LEditContainer::lastBlock() const
{
    if(m_children.empty()){
        return nullptr;
    }
    return m_children.back();
}

bool LEditContainer::isAncestor(std::shared_ptr<LEditBase> t) const
{
    if(t== nullptr){
        return false;
    }

    std::shared_ptr<LEditContainer> p = m_parent.lock();
    while(p!= nullptr){
        if(p == t){
            return true;
        }
        p = p->m_parent.lock();
    }
    return false;
}

bool LEditContainer::isAcceptableChild(std::shared_ptr<LEditBase> t) const
{
    if(t.get() == this){
        return false;
    }

    if(!isValid(t)){
        return false;
    }

    if(lItemIndex(m_children, t) >=0){
        return false;
    }

    std::shared_ptr<LEditContainer> p = m_parent.lock();
    while(p != nullptr){
        if(p == t){
            return false;
        }
        p = p->m_parent.lock();
    }

    return true;
}

void LEditContainer::setActiveChild(std::shared_ptr<LEditBase> child)
{
    if(child == nullptr){
        m_activeChild = nullptr;
        return;
    }

    if(child == startBase()){
        m_activeChild = child;
        return;
    }

    if(lItemIndex(m_children, child)>=0){
        m_activeChild = child;
    }
}

void LEditContainer::draw(LPainter *painter,
                          const LPoint<double> &pos,
                          int flags,
                          const LRect<double> &clip) const
{
    if(m_isHiding){
        return;
    }

    if(m_rect.translated(pos).isIntersects(clip)){
        if(isAllSelect()){
            std::shared_ptr<LPen> oldPen = nullptr;
            if(flags & (int)DrawFlags::Selections){
                LRect<double> f = m_rect.translated(pos).intersected(clip);
                std::shared_ptr<LPalette> pal = palette();
                painter->fillRect(f, pal->brush(LPalette::Highlight));
                oldPen = painter->pen();
                painter->setPen(pal->brush(LPalette::HighlightedText)->m_color);
            }

            flags = flags & ~((int)DrawFlags::Selections);
            for (const std::shared_ptr<LEditBase>& eb : m_children)
            {
                eb->draw(painter, pos, flags, clip);
            }

            if(oldPen != nullptr){
                painter->setPen(oldPen, false);
            }
        }else{
            for (const std::shared_ptr<LEditBase>& eb : m_children)
            {
                eb->draw(painter, pos, flags, clip);
            }
        }
    }
}

void LEditContainer::drawCursor(LPainter *p,
                                const LPoint<double> &pos,
                                int width,
                                const LRect<double> &clip) const
{
    if(width ==0){
        return;
    }

    if(isValid(m_activeChild)){
        m_activeChild->drawCursor(p, pos, width, clip);
    }else{
        LRect<double> cursorRect(marginLeft(), m_rect.m_y, width, m_rect.height(), false);
        cursorRect.translate(pos);

        if(cursorRect.isIntersects(clip)){
            std::shared_ptr<LPalette> pal = palette();
            std::shared_ptr<LPen> oldPen = p->pen();
            p->setPen(pal->brush(LPalette::Text)->m_color);

            bool toggleAntialiasing = !(p->renderHints() & LRenderHint::Antialiasing)
                                      && (p->transform().type() > LTransform2D::TxTranslate);
            if (toggleAntialiasing){
                p->setRenderHints(LRenderHint::Antialiasing);
            }
            LCompositionMode origCompositionMode = p->compositionMode();
            if (p->engine()->hasFeature(LPaintEngineFeature::RasterOpModes)){
                p->setCompositionMode(LCompositionMode::RasterOpNotDestination);
            }
            p->fillRect(cursorRect, p->pen()->brush());
            p->setCompositionMode(origCompositionMode);
            p->setPen(oldPen, false);
        }
    }
}


void LEditContainer::calculateRect()
{
    calculateSize();
    stretchSize();
    calculatePos();
}

void LEditContainer::calculatePos()
{

}

bool LEditContainer::hasSelect() const
{
    if(m_cursorState == NoSelect){
        return false;
    }

    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        if(eb->hasSelect()){
            return true;
        }
    }
    return false;
}

bool LEditContainer::isAllSelect() const
{
    if(m_cursorState == NoSelect){
        return false;
    }

    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        if(!eb->isAllSelect()){
            return false;
        }
    }
    return true;
}

void LEditContainer::deSelect()
{
    m_cursorState = NoSelect;
    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        eb->deSelect();
    }
}

void LEditContainer::selectAll(bool isLeftToRight)
{
    m_cursorState = PartialSelect;
    if(isLeftToRight){
        m_partSelectStartId = 0;
        m_partSelectEndId = (int)m_children.size();
    }else{
        m_partSelectStartId = (int)m_children.size();
        m_partSelectEndId = 0;
    }
    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        eb->selectAll(isLeftToRight);
    }
}

std::list<std::shared_ptr<LEditOp> > LEditContainer::removeSelection()
{
    std::list<std::shared_ptr<LEditOp>> ret;
    if(m_cursorState == NoSelect){
        return ret;
    }

    std::list<std::shared_ptr<LEditBase> > partialSelectList;
    std::list<std::shared_ptr<LEditBase> > allSelectList;

    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        if(eb->m_cursorState != NoSelect){
            if(eb->isAllSelect()){
                allSelectList.push_back(eb);
            }else{
                partialSelectList.push_back(eb);
            }
        }
    }

    if(isDynChild()){
        for(const std::shared_ptr<LEditBase>& b: allSelectList){
            if(b == m_activeChild){
                m_activeChild = nullptr;
            }

            std::shared_ptr<LItemInputDelete> op
                = std::make_shared<LItemInputDelete>(false);
            buildIndexes(op->m_editContainerIndex);
            op->m_pos = lItemIndex(m_children, b);
            op->m_validNum = 1;
            op->m_inputCbor.push_back(b->toCbor(false));
            ret.push_back(op);

            m_children.remove(b);
        }
    }else{
        for(std::shared_ptr<LEditBase> b: allSelectList){
            std::list<std::shared_ptr<LEditOp> > t = b->removeSelection();
            ret.insert(ret.end(), t.begin(), t.end());
        }
    }

    for(std::shared_ptr<LEditBase> b: partialSelectList){
        std::list<std::shared_ptr<LEditOp> > t = b->removeSelection();
        ret.insert(ret.end(), t.begin(), t.end());
    }

    m_partSelectEndId = 0;
    m_partSelectStartId = 0;
    m_cursorState = NoSelect;
    return ret;
}

bool LEditContainer::moveCursor(LMoveOper op)
{
    if(isValid(m_activeChild)){
        if(m_activeChild->moveCursor(op)){
            return true;
        }
    }

    switch(op){
    case LMoveOper::NoMove:{
        return true;
    }
    case LMoveOper::Start:
    case LMoveOper::StartOfLine:
    case LMoveOper::StartOfWord:{
        moveToStart();
        return true;
    }
    case LMoveOper::StartOfBlock:{
        std::shared_ptr<LEditBase> t = firstBlock();
        if(isValid(t)){
            m_activeChild = t;
            return m_activeChild->moveCursor(LMoveOper::StartOfBlock);
        }else{
            moveToStart();
            return true;
        }
    }
    case LMoveOper::PreviousBlock:
    case LMoveOper::PreviousCharacter:
    case LMoveOper::PreviousWord:
    case LMoveOper::Left:
    case LMoveOper::WordLeft:
    case LMoveOper::PreviousCell:{
        if(isAtStart()){
            return false;
        }
        std::shared_ptr<LEditBase> t  = previousBlock();
        if(isValid(t)){
            m_activeChild = t;
            return m_activeChild->moveCursor(LMoveOper::EndOfBlock);
        }else{
            moveToStart();
            return true;
        }
    }
    case LMoveOper::End:
    case LMoveOper::EndOfLine:
    case LMoveOper::EndOfWord:{
        moveToEnd();
        return true;
    }
    case LMoveOper::EndOfBlock:{
        std::shared_ptr<LEditBase> t = lastBlock();
        if(isValid(t)){
            m_activeChild = t;
            return m_activeChild->moveCursor(LMoveOper::EndOfBlock);
        }else{
            moveToEnd();
            return true;
        }
    }
    case LMoveOper::NextBlock:
    case LMoveOper::NextCharacter:
    case LMoveOper::NextWord:
    case LMoveOper::Right:
    case LMoveOper::WordRight:
    case LMoveOper::NextCell:{
        if(isAtEnd()){
            return false;
        }
        std::shared_ptr<LEditBase> t = nextBlock();
        if(isValid(t)){
            m_activeChild = t;
            return m_activeChild->moveCursor(LMoveOper::StartOfBlock);
        }else{
            moveToEnd();
            return true;
        }
    }
    default:{
        break;
    }
    }
    return false;

}

int LEditContainer::maxIndex() const
{
    return (int)m_children.size();
}

int LEditContainer::minIndex() const
{
    return -1;
}

int LEditContainer::activePos() const
{
    if(m_activeChild != nullptr){
        if(isAtStart()){
            return -1;
        }

        return lItemIndex(m_children, m_activeChild);
    }else{
        return (int)m_children.size();
    }
}

void LEditContainer::setActivePos(int pos)
{
    if(pos <0){
        moveToStart();
        return;
    }

    if(pos <= (int)m_children.size()){
        m_activeChild =  *std::next(m_children.cbegin(),pos);
    }else{
        moveToEnd();
    }
}

bool LEditContainer::setToRowChild(std::shared_ptr<LEditBase> t, int index)
{
    if(index>=0 && index<= (int)m_children.size()){
        std::list<std::shared_ptr<LEditBase> >::iterator iter =  std::next(m_children.begin(), index);
        std::shared_ptr<LEditBase> child = *iter;
        if(child->isRow()){
            if(!isAcceptableChild(t)){
                return false;
            }

            if(t->isRow()){
                if(child != t){
                    t->m_parent = getEditT<LEditContainer>();
                    *iter = t;
                }
            }else{
                child->clear();
                std::shared_ptr<LEditRow> row = std::dynamic_pointer_cast<LEditRow>(child);
                row->append(t);
            }
            return true;
        }
    }
    return false;
}

void LEditContainer::calculateSize()
{
    if(m_isHiding){
        m_rect.reset();
        return;
    }

    for (std::shared_ptr<LEditBase> eb : m_children)
    {
        eb->calculateSize();
    }
}

void LEditContainer::stretchSize()
{
    if(m_isHiding){
        return;
    }

    LEditBase::stretchSize();
    for (std::shared_ptr<LEditBase> eb : m_children)
    {
        eb->stretchSize();
    }
}

bool LEditContainer::deleteChild(std::shared_ptr<LEditBase> child)
{
    if(isDynChild()){
        if(isValid(child)){
            if(child->m_parent.lock().get() == this){
                m_children.remove(child);
                child->m_parent.reset();
                if(child == m_activeChild){
                    m_activeChild = nullptr;
                }
                return true;
            }
        }
    }
    return false;
}

std::shared_ptr<LEditBase> LEditContainer::childAtPoint(const LPoint<double> &p) const
{
    if(m_isHiding){
        return nullptr;
    }

    if(bounds().contains(p) == false){
        return nullptr;
    }

    for (const std::shared_ptr<LEditBase>& eb : m_children){
        if(eb->bounds().contains(p)){
            return eb;
        }
    }
    return nullptr;
}

std::shared_ptr<LEditBase> LEditContainer::leafAtPoint(const LPoint<double> &p) const
{
    if(m_isHiding){
        return nullptr;
    }

    if(bounds().contains(p) == false){
        return nullptr;
    }

    std::shared_ptr<LEditContainer> ret = getEditT<LEditContainer>();
    std::shared_ptr<LEditBase> b = nullptr;
    while(true){
        b = nullptr;
        for (const std::shared_ptr<LEditBase>& eb : ret->m_children){
            if(eb->bounds().contains(p)){
                b = eb;
                break;
            }
        }

        if(b == nullptr){
            break;
        }
        if(b->isContainer()){
            ret = std::dynamic_pointer_cast<LEditContainer>(b);
        }else{
            return b;
        }
    }
    return ret;
}

LPoint<double> LEditContainer::activePoint() const
{
    if(isAtStart()){
        return LPoint<double>(m_rect.m_x, midY());
    }

    if(m_activeChild != nullptr){
        return m_activeChild->activePoint();
    }
    return LPoint<double>(m_rect.m_x + m_rect.width(), midY());
}

bool LEditContainer::setToActivePoint(const LPoint<double> &p)
{
    m_activeChild = nullptr;
    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        if(eb->bounds().contains(p)){
            m_activeChild = eb;
            break;
        }
    }

    if(isValid(m_activeChild)){
        m_activeChild->setToActivePoint(p);
        return true;
    }else{
        if(p.m_x > midX()){
            moveToEnd();
        }else{
            moveToStart();
        }
        return true;
    }
}

int LEditContainer::del(int pos, int length)
{
    if(pos <0){
        return 0;
    }

    if(pos >(int)m_children.size()){
        return 0;
    }


    if(isDynChild()){
        int delNum =0;
        for(int i=0; i<length; ++i){
            if(pos<(int)m_children.size()){
                std::list<std::shared_ptr<LEditBase> >::const_iterator iter =  std::next(m_children.cbegin(), pos);
                std::shared_ptr<LEditBase> child = *iter;
                m_children.erase(iter);
                if(child == m_activeChild){
                    m_activeChild = nullptr;
                }
                child->m_parent.reset();
                delNum ++;
            }else{
                break;
            }
        }

        if(pos >=0 && pos< (int)m_children.size()){
            m_activeChild = *std::next(m_children.begin(), pos);
            m_activeChild->moveToStart();
        }
        return delNum;
    }else{
        return 0;
    }
}

bool LEditContainer::insert(std::shared_ptr<LEditBase> child, int pos)
{
    if(isDynChild()){
        if(isAcceptableChild(child)){
            std::shared_ptr<LEditContainer> p = child->m_parent.lock();
            if(p != nullptr){
                p->deleteChild(child);
            }
            std::list<std::shared_ptr<LEditBase> >::const_iterator iter =  std::next(m_children.cbegin(), pos);
            m_children.insert(iter, child);
            child->m_parent = getEditT<LEditContainer>();
            return true;
        }
        return false;
    }
    return false;
}

void LEditContainer::clear()
{
    for(const std::shared_ptr<LEditBase>& c : m_children){
        c->m_parent.reset();
    }
    m_children.clear();
    m_activeChild = nullptr;
    m_partSelectStartId = 0;
    m_partSelectEndId = 0;
}

void LEditContainer::markSelectStart()
{
    if((m_cursorState & PartialSelect) == NoSelect){
        m_cursorState = PartialSelect;
        if(isAtStart()){
            m_partSelectStartId = -1;
        }else if(isAtEnd()){
            m_partSelectStartId = (int)m_children.size();
        }else{
            int index = lItemIndex(m_children, m_activeChild);
            if(index >=0){
                m_partSelectStartId = index;
                m_activeChild->markSelectStart();
            }
        }
    }
}

void LEditContainer::selectFrom(const std::vector<int> &indexes, int level, uint8_t type)
{
    if(level>= (int)indexes.size()){
        switch (type) {
        case SelectToLeft:{
            selectAll(true);
            break;
        }
        case SelectToRight:{
            deSelect();
            break;
        }
        default:{
            deSelect();
            break;
        }
        }
        return;
    }


    m_cursorState = PartialSelect;
    m_partSelectStartId = indexes[level];

    switch (type) {
    case SelectToLeft:{
        m_partSelectEndId = -1;
        int i=0;
        for (std::shared_ptr<LEditBase> eb : m_children)
        {
            if(i< m_partSelectStartId){
                eb->selectAll(false);
            }else if(i == m_partSelectStartId){
                eb->selectFrom(indexes, level+1, type);
            }else{
                eb->deSelect();
            }
            ++i;
        }
        break;
    }
    case SelectToRight:{
        m_partSelectEndId = (int)m_children.size();
        int i=0;
        for (std::shared_ptr<LEditBase> eb : m_children)
        {
            if(i> m_partSelectStartId){
                eb->selectAll(true);
            }else if(i == m_partSelectStartId){
                eb->selectFrom(indexes, level+1, type);
            }else{
                eb->deSelect();
            }
            ++i;
        }
        break;
    }
    default:{
        m_partSelectEndId = m_partSelectStartId;
        if(m_partSelectStartId>=0 && m_partSelectStartId< (int)m_children.size()){
            std::shared_ptr<LEditBase> eb = *std::next(m_children.cbegin(),m_partSelectStartId);
            eb->selectFrom(indexes, level+1, type);
        }
        break;
    }
    }
}

void LEditContainer::selectTo(const std::vector<int> &indexes, int level, uint8_t type)
{
    if(level>= (int)indexes.size()){
        switch (type) {
        case SelectToRight:{
            selectAll(false);
            break;
        }
        case SelectToLeft:{
            deSelect();
            break;
        }
        default:{
            deSelect();
            break;
        }
        }
        return;
    }


    m_cursorState = PartialSelect;
    m_partSelectEndId = indexes[level];

    switch (type) {
    case SelectToRight:{
        m_partSelectStartId = -1;
        int i=0;
        for (std::shared_ptr<LEditBase> eb : m_children)
        {
            if(i< m_partSelectEndId){
                eb->selectAll(true);
            }else if(i == m_partSelectEndId){
                eb->selectTo(indexes, level+1, type);
            }else{
                eb->deSelect();
            }
            ++i;
        }
        break;
    }
    case SelectToLeft:{
        m_partSelectStartId = (int)m_children.size();
        int i=0;
        for (std::shared_ptr<LEditBase> eb : m_children)
        {
            if(i> m_partSelectEndId){
                eb->selectAll(false);
            }else if(i == m_partSelectEndId){
                eb->selectTo(indexes, level+1, type);
            }else{
                eb->deSelect();
            }
            ++i;
        }
        break;
    }
    default:{
        m_partSelectStartId = m_partSelectEndId;
        if(m_partSelectStartId>=0 && m_partSelectStartId< (int)m_children.size()){
            std::shared_ptr<LEditBase> eb = *std::next(m_children.cbegin(),m_partSelectStartId);
            eb->selectTo(indexes, level+1, type);
        }
        break;
    }
    }
}

void LEditContainer::selectRange(const std::vector<int> &start, const std::vector<int> &end, int level, uint8_t type)
{
    if((int)start.size() >level ){
        if((int)end.size()>level){
            int startId = start[level];
            int endId = end[level];

            if(startId == endId){
                m_cursorState = PartialSelect;
                m_partSelectStartId = startId;
                m_partSelectEndId = m_partSelectStartId;
                if(m_partSelectStartId >= 0 && m_partSelectStartId < (int)m_children.size()){
                    std::shared_ptr<LEditBase> eb = *std::next(m_children.cbegin(),m_partSelectStartId);
                    eb->selectRange(start, end, level+1, type);
                }
                return;
            }else{
                m_cursorState = PartialSelect;
                m_partSelectStartId = startId;
                m_partSelectEndId = endId;

                switch (type) {
                case SelectToLeft:{
                    int i=0;
                    for(std::shared_ptr<LEditBase> ch: m_children){
                        if(i >endId && i < startId){
                            ch->selectAll(false);
                        }

                        if(i == startId){
                            ch->selectFrom(start, level+1, type);
                        }else if(i == endId){
                            ch->selectTo(end, level+1, type);
                        }
                        i++;
                    }
                    break;
                }
                case SelectToRight:{
                    int i=0;
                    for(std::shared_ptr<LEditBase> ch: m_children){
                        if(i >startId && i < endId){
                            ch->selectAll(true);
                        }

                        if(i == startId){
                            ch->selectFrom(start, level+1, type);
                        }else if(i == endId){
                            ch->selectTo(end, level+1, type);
                        }
                        i++;
                    }
                    break;
                }
                default:{
                    break;
                }
                }
            }
        }else{
            selectFrom(start, level, type);
        }
    }else{
        if((int)end.size()>level){
            selectTo(end, level, type);
        }
    }
}

bool LEditContainer::canStrechChild(std::shared_ptr<LEditBase> ch) const
{
    if(ch->isMathMLDisplayStyle()){
        return false;
    }
    if(isRow()){
        if(ch->isRow() || ch->isText()){
            return ch->m_levelOffset == 0;
        }
    }
    return false;
}

bool LEditContainer::isStretchX() const
{
    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        if(!eb->isMathMLDisplayStyle()){
            if(eb->m_levelOffset == 0){
                if(eb->isStretchX()){
                    return true;
                }
            }
        }
    }
    return false;
}

bool LEditContainer::isStretchY() const
{
    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        if(!eb->isMathMLDisplayStyle()){
            if(eb->m_levelOffset == 0){
                if(eb->isStretchY()){
                    return true;
                }
            }
        }
    }
    return false;
}

bool LEditContainer::isAtStart() const
{
    return m_activeChild == startBase();
}

bool LEditContainer::isAtEnd() const
{
    return m_activeChild == nullptr;
}

void LEditContainer::moveToStart()
{
    m_activeChild = startBase();
}

void LEditContainer::moveToEnd()
{
    m_activeChild = nullptr;
}

bool LEditContainer::isTextEmpty()
{
    for (const std::shared_ptr<LEditBase>& child : m_children)
    {
        if(child->isText()){
            std::shared_ptr<LEditText> textRow =  std::dynamic_pointer_cast<LEditText>(child);
            if(!textRow->text().empty()){
                return false;
            }
        }else if(child->isContainer()){
            std::shared_ptr<LEditContainer> cc =  std::dynamic_pointer_cast<LEditContainer>(child);
            if(!cc->isTextEmpty()){
                return false;
            }
        }
    }
    return true;
}

LK_NAMESPACE_END
