#include "leditrow.h"
#include "lkgui/text/lfontmetrics.h"
#include "ledittext.h"
#include "leditop.h"

LK_NAMESPACE_BEGIN

LEditRow::LEditRow()
{
}

bool LEditRow::isRow() const
{
    return true;
}

uint32_t LEditRow::type() const
{
    return (uint32_t)LEditType::Row;
}

bool LEditRow::isDynChild() const
{
    return true;
}

void LEditRow::calculateSize()
{
    if(m_isHiding){
        m_rect.reset();
        return;
    }

    LEditContainer::calculateSize();

    if(m_children.empty()){
        std::shared_ptr<LFont> f = font();
        if(f != nullptr){
            if(isAutoMargin()){
                initMargin(f->pixelSizeF());
            }
            LFontMetrics metrics = LFontMetrics(f);
            m_baseUpHeight = metrics.height()*0.5;
            m_baseHeight = metrics.height();
            m_rect.setWidth(0);
            m_rect.setHeight(m_baseHeight);
        }else{
            m_rect.setWidth(0);
            m_rect.setHeight(0);
            m_baseUpHeight = 0;
            m_baseHeight = 0;
        }
    }else{
        if(isAutoMargin()){
            m_margin.reset();
        }
        double width = 0;
        double maxUpHeight = 0;
        double maxDownHeight = 0;
        for (const std::shared_ptr<LEditBase>& eb : m_children)
        {
            width += eb->fullWidth();
            maxUpHeight = std::max<double>(maxUpHeight, eb->upHeight());
            maxDownHeight = std::max<double>(maxDownHeight, eb->downHeight());
        }

        m_baseUpHeight = maxUpHeight;
        m_baseHeight = maxUpHeight + maxDownHeight;
        m_rect.setWidth(width);
        m_rect.setHeight(m_baseHeight);
    }
}

void LEditRow::calculatePos()
{
    double x = m_rect.m_x;
    double centerV = m_rect.m_y - marginTop() + upHeight();

    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        x += eb->marginLeft();
        eb->m_rect.moveLeft(x);
        x += eb->m_rect.width() + eb->marginRight();
        eb->m_rect.moveTop(centerV - eb->upHeight() + eb->marginTop());
    }

    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        if(eb->isContainer()){
            std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(eb);
            c->calculatePos();
        }
    }
}

bool LEditRow::setToRowChild(std::shared_ptr<LEditBase> t, int index)
{
    if((int)m_children.size() > index){
        return LEditContainer::setToRowChild(t, index);
    }else if((int)m_children.size() == index){
        return insert(t, index);
    }else{
        return false;
    }
}

std::shared_ptr<LEditBase> LEditRow::merge(std::shared_ptr<LEditRow> row, int pos)
{
    if(isAncestor(row)){
        return nullptr;
    }

    if(row != nullptr && (row.get() != this)){
        if((pos > (int)m_children.size()) || (pos <0)){
            return nullptr;
        }

        std::shared_ptr<LEditBase> lastItem = nullptr;
        for(const std::shared_ptr<LEditBase>& eb : row->m_children){
            if(insert(eb, pos++)){
                lastItem = eb;
            }
        }

        std::shared_ptr<LEditContainer> rp = row->m_parent.lock();
        if(rp != nullptr){
            rp->deleteChild(row);
        }
        return lastItem;
    }
    return nullptr;
}

double LEditRow::upHeight() const
{
    if(lFuzzyIsZero(m_rect.height())){
        return 0;
    }

    if(m_baseHeight>0){
        return m_baseUpHeight/m_baseHeight * m_rect.height() + marginTop();
    }else{
        return m_rect.height()*0.5 + marginTop();
    }
}

bool LEditRow::moveCursor(LMoveOper op)
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
    case LMoveOper::StartOfBlock:
    case LMoveOper::StartOfWord:{
        moveToStart();
        return true;
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
        if(isValid(t) && t != m_activeChild){
            bool needMoveMore = false;
            if(isAtEnd()){
                needMoveMore = true;
            }else if(isValid(m_activeChild)){
                if(m_activeChild->isRow()){
                    needMoveMore = true;
                }else if(m_activeChild->isText()){
                    needMoveMore = true;
                }else if(m_activeChild->isAtStart()){
                    needMoveMore = true;
                }
            }

            m_activeChild = t;
            if(t->isContainer()){
                if(!t->isRow()){
                    std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(t);
                    c->setActiveChild(nullptr);
                    if(needMoveMore){
                        return m_activeChild->moveCursor(LMoveOper::EndOfBlock);
                    }else{
                        return true;
                    }
                }
            }
            if(needMoveMore){
                return m_activeChild->moveCursor(op);
            }else{
                return m_activeChild->moveCursor(LMoveOper::EndOfBlock);
            }
        }
        break;
    }
    case LMoveOper::End:
    case LMoveOper::EndOfLine:
    case LMoveOper::EndOfBlock:
    case LMoveOper::EndOfWord:{
        moveToEnd();
        return true;
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
        if(isValid(t) && t != m_activeChild){
            bool needMoveMore = false;
            if(isAtStart()){
                needMoveMore = true;
            }else if(isValid(m_activeChild)){
                if(m_activeChild->isRow()){
                    needMoveMore = true;
                }else if(m_activeChild->isText()){
                    needMoveMore = true;
                }else if(m_activeChild->isAtEnd()){
                    needMoveMore = true;
                }
            }

            m_activeChild = t;
            if(t->isContainer()){
                if(!t->isRow()){
                    std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(t);
                    c->setActiveChild(nullptr);
                    if(needMoveMore){
                        return m_activeChild->moveCursor(LMoveOper::StartOfBlock);
                    }else{
                        return true;
                    }
                }
            }
            if(needMoveMore){
                return m_activeChild->moveCursor(op);
            }else{
                return m_activeChild->moveCursor(LMoveOper::StartOfBlock);
            }
        }
        break;
    }
    default:{
        break;
    }
    }
    return false;
}

bool LEditRow::setToActivePoint(const LPoint<double> &p)
{
    m_activeChild = nullptr;
    double s = m_rect.m_x;
    double e;
    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        e = eb->m_rect.x2() + eb->marginRight();
        if ((p.m_x >= s) &&
            (p.m_x <= e))
        {
            m_activeChild = eb;
            break;
        }
        s = e;
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

std::list<std::shared_ptr<LEditOp> > LEditRow::input(const wchar_t ch)
{
    std::wstring t(&ch, 1);
    return input(t);
}

std::list<std::shared_ptr<LEditOp> > LEditRow::input(const std::wstring &ch)
{
    std::shared_ptr<LEditText> textRow = rowCurrentText();
    if(textRow == nullptr){
        textRow = createText(ch);
        return putToActive(textRow, true, NoMerge);
    }else{
        if(textRow->isScript()){
            if(textRow->isSriptEnd()){
                textRow = createText(ch, textRow);
                return putToActive(textRow, true, NoMerge);
            }else{
                return textRow->input(ch);
            }
        }else{
            if(canMergeText(ch, textRow)){
                return textRow->input(ch);
            }else{
                if(isDynChild()){
                    std::shared_ptr<LEditText> t = createText(ch, textRow);
                    return putToActive(t, true, NoMerge);
                }
            }
        }
    }
    return std::list<std::shared_ptr<LEditOp> >();
}

bool LEditRow::canMergeText(const std::wstring &ch, std::shared_ptr<LEditText> textRow)
{
    if(textRow == nullptr){
        return false;
    }
    return textRow->canMergeText(ch);
}

std::list<std::shared_ptr<LEditOp> > LEditRow::putToActive(std::shared_ptr<LEditBase> child,
                                                           bool isForward,
                                                           MergeType mergeType)
{
    std::list<std::shared_ptr<LEditOp>> ret;

    int pos = -1;
    if(!isValid(m_activeChild)){
        if(!m_children.empty()){
            if(isStart(m_activeChild)){
                if(m_activeChild->isText()){
                    m_activeChild = m_children.front();
                    m_activeChild->moveCursor(LMoveOper::StartOfBlock);
                }else{
                    pos = 0;
                }
            }else{
                m_activeChild = m_children.back();
                m_activeChild->moveCursor(LMoveOper::EndOfBlock);
            }
        }
    }

    if(isValid(m_activeChild)){
        bool isSplitPos = false;

        if(m_activeChild->isText()){
            std::shared_ptr<LEditText> textRow = std::dynamic_pointer_cast<LEditText>(m_activeChild);
            if(textRow != nullptr){
                if(!textRow->isScript()){
                    if(mergeType != NoMerge){ //合并文本
                        if(child->type() == textRow->type()){
                            std::shared_ptr<LEditText> cht = std::dynamic_pointer_cast<LEditText>(child);
                            if(textRow->canMergeText(cht->text())){
                                std::shared_ptr<LTextInputDelete> op = std::make_shared<LTextInputDelete>(true);
                                textRow->buildTextIndexes(op->m_editItemIndex);
                                op->m_text = cht->text();
                                textRow->inputText(op->m_text);
                                ret.push_back(op);
                                return ret;
                            }
                        }
                    }

                    //不能合并，需要分割处理
                    pos = lItemIndex(m_children, m_activeChild);
                    isSplitPos = true;
                    if(pos >=0){
                        std::wstring prev = textRow->text().substr(0, textRow->activePos());
                        std::wstring next = textRow->text().substr(textRow->activePos());

                        if(prev.empty()){
                        }else{
                            if(!next.empty()){
                                std::shared_ptr<LTextMergeSplit> op = std::make_shared<LTextMergeSplit>(false);
                                textRow->buildTextIndexes(op->m_mergeItemIndex);
                                op->m_text = next;
                                op->m_hasSplit = true;
                                ret.push_back(op);

                                textRow->setText(prev);
                                textRow->setActivePos(0);
                                textRow = createText(next, textRow);
                                insert(textRow, pos+1);
                            }

                            if(isForward){
                                pos +=1;
                            }
                        }
                    }
                }
            }
        }

        if(!isSplitPos){
            pos = lItemIndex(m_children, m_activeChild);
            if(isForward){
                pos +=1;
            }
        }
    }

    if(pos <0)
    {
        if(isForward){
            pos = m_children.size();
        }else{
            pos = 0;
        }
    }

    std::shared_ptr<LEditBase> mergeItem = nullptr;
    if((child->type() == type()) && (mergeType != NoMerge)){

        std::shared_ptr<LItemInputDelete> op = std::make_shared<LItemInputDelete>(true);
        buildIndexes(op->m_editContainerIndex);
        op->m_pos = pos;
        std::shared_ptr<LEditRow> cr = std::dynamic_pointer_cast<LEditRow>(child);
        op->m_inputCbor = cr->childrenCbor();

        ret.push_back(op);
        mergeItem = merge(cr, pos);
        if(mergeItem != nullptr){
            op->m_validNum = lItemIndex(m_children, mergeItem)-pos +1;
            ret.push_back(op);
        }
    }

    if(mergeItem == nullptr){
        if(insert(child, pos)){
            m_activeChild = child;
            m_activeChild->moveCursor(LMoveOper::EndOfBlock);

            std::shared_ptr<LItemInputDelete> op = std::make_shared<LItemInputDelete>(true);
            buildIndexes(op->m_editContainerIndex);
            op->m_pos = pos;
            op->m_inputCbor.push_back(child->toCbor(false));
            op->m_validNum = 1;
            ret.push_back(op);
        }
    }else{
        m_activeChild = mergeItem;
        m_activeChild->moveCursor(LMoveOper::EndOfBlock);
    }
    return ret;
}

bool LEditRow::isSingleText()
{
    if(m_children.size() == 1){
        return m_children.front()->isText();
    }
    return false;
}

void LEditRow::initMargin(double baseSize)
{
    double space = mmlSpacing(LMathMLSpacing::Thinmathspace, baseSize, scriptLevel())*0.5;
    m_margin = LMargins<double>(space, space, space, space);
}

bool LEditRow::isAutoMargin()
{
    return true;
}

bool LEditRow::append(std::shared_ptr<LEditBase> child)
{
    return insert(child, (int)m_children.size());
}

bool LEditRow::prepend(std::shared_ptr<LEditBase> child)
{
    return insert(child, 0);
}

std::shared_ptr<LEditText> LEditRow::blockCurrentText()
{
    std::shared_ptr<LEditBase> ch = m_activeChild;
    while(isValid(ch)){
        if(ch->isContainer()){
            std::shared_ptr<LEditContainer> cr = std::dynamic_pointer_cast<LEditContainer>(ch);
            ch = cr->activeChild();
        }else{
            if(ch->isText()){
                return std::dynamic_pointer_cast<LEditText>(ch);
            }
        }
    }
    return nullptr;
}

std::shared_ptr<LEditText> LEditRow::rowCurrentText()
{
    if(isValid(m_activeChild)){
        if(m_activeChild->isText()){
            return std::dynamic_pointer_cast<LEditText>(m_activeChild);
        }
    }
    return nullptr;
}

std::shared_ptr<LEditText> LEditRow::rowFirstText()
{
    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        if(eb->isText()){
            return std::dynamic_pointer_cast<LEditText>(eb);
        }
    }
    return nullptr;
}

std::shared_ptr<LEditText> LEditRow::rowNextText()
{
    if(isValid(m_activeChild)){
        if(m_activeChild->isText()){
            std::list<std::shared_ptr<LEditBase> >::const_iterator iter = m_children.cbegin();
            for(; iter != m_children.cend(); ++iter){
                if(*iter == m_activeChild){
                    ++iter;
                    if(iter != m_children.cend()){
                        if((*iter)->isText()){
                            return std::dynamic_pointer_cast<LEditText>(*iter);
                        }
                    }
                    break;
                }
            }
        }
    }
    return nullptr;
}

std::shared_ptr<LEditText> LEditRow::rowPreviousText()
{
    if(isValid(m_activeChild)){
        if(m_activeChild->isText()){
            std::list<std::shared_ptr<LEditBase> >::const_reverse_iterator iter = m_children.crbegin();
            for(; iter != m_children.crend(); ++iter){
                if(*iter == m_activeChild){
                    ++iter;
                    if(iter != m_children.crend()){
                        if((*iter)->isText()){
                            return std::dynamic_pointer_cast<LEditText>(*iter);
                        }
                    }
                    break;
                }
            }
        }
    }
    return nullptr;
}

std::shared_ptr<LEditText> LEditRow::rowLastText()
{
    std::list<std::shared_ptr<LEditBase> >::const_reverse_iterator iter = m_children.crbegin();
    for (; iter != m_children.crend(); ++iter)
    {
        if((*iter)->isText()){
            return std::dynamic_pointer_cast<LEditText>(*iter);
        }
    }
    return nullptr;
}

std::shared_ptr<LEditText> LEditRow::createText(const wchar_t ch,  std::shared_ptr<LEditText> textRow)
{
    std::wstring t(&ch, 1);
    return createText(t);
}

std::shared_ptr<LEditText> LEditRow::createText(const std::wstring &ch, std::shared_ptr<LEditText> textRow)
{
    if(textRow != nullptr){
        return textRow->create(ch);
    }else{
        std::shared_ptr<LEditText> ret = std::make_shared<LEditText>();
        ret->setText(ch);
        return ret;
    }
}

LK_NAMESPACE_END
