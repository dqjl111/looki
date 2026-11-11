#include "leditrows.h"
#include "leditdoc.h"
#include "lieditor.h"

LK_NAMESPACE_BEGIN

LEditRows::LEditRows()
{

}

uint32_t LEditRows::type() const
{
    return uint32_t(LEditType::Rows);
}

bool LEditRows::isDynChild() const
{
    return true;
}

bool LEditRows::append(std::shared_ptr<LEditRow> child)
{
    return insert(child, (int)m_children.size());
}

bool LEditRows::prepend(std::shared_ptr<LEditRow> child)
{
    return insert(child, 0);
}

bool LEditRows::insert(std::shared_ptr<LEditBase> child, int pos)
{
    if(child->isRow()){
        LEditContainer::insert(child, pos);
    }
    return false;
}

bool LEditRows::setToRowChild(std::shared_ptr<LEditBase> t, int index)
{
    if(m_children.size() > index){
        return LEditContainer::setToRowChild(t, index);
    }else if(m_children.size() == index){
        if(t->isRow()){
            std::shared_ptr<LEditRow> r = std::dynamic_pointer_cast<LEditRow>(t);
            return insert(r, index);
        }else{
            std::shared_ptr<LEditRow> tRow = std::make_shared<LEditRow>();
            tRow->append(t);
            return insert(tRow, index);
        }
    }else{
        return false;
    }
}

std::shared_ptr<LEditRow> LEditRows::currentRow()
{
    return std::dynamic_pointer_cast<LEditRow>(m_activeChild);
}

std::shared_ptr<LEditRow> LEditRows::firstRow()
{
    return std::dynamic_pointer_cast<LEditRow>(firstBlock());
}

std::shared_ptr<LEditRow> LEditRows::nextRow()
{
    return std::dynamic_pointer_cast<LEditRow>(nextBlock());
}

std::shared_ptr<LEditRow> LEditRows::previousRow()
{
    return std::dynamic_pointer_cast<LEditRow>(previousBlock());
}

std::shared_ptr<LEditRow> LEditRows::lastRow()
{
    return std::dynamic_pointer_cast<LEditRow>(lastBlock());
}

std::shared_ptr<LEditRow> LEditRows::createRow(uint32_t type)
{
    if(type == (uint32_t)LEditType::Unknown){
        return std::make_shared<LEditRow>();
    }else{
        std::shared_ptr<LIEditor> e = editor();
        std::shared_ptr<LEditBase> b = nullptr;
        if(e != nullptr){
            std::shared_ptr<LEditDoc> pDoc = e->doc();
            if(pDoc != nullptr){
                b = pDoc->createBase(type, true);
            }else{
                b = LEditDoc::createGlobalBase(type);
            }
        }else{
            b = LEditDoc::createGlobalBase(type);
        }

        std::shared_ptr<LEditRow> ret;
        if(b->isRow()){
            ret = std::dynamic_pointer_cast<LEditRow>(b);
        }else{
            ret = std::make_shared<LEditRow>();
            ret->append(b);
        }
        return ret;
    }
}

std::shared_ptr<LEditRow> LEditRows::newRow(int pos, uint32_t type)
{
    std::shared_ptr<LEditRow> row = createRow(type);
    insert(row, pos);
    return row;
}

void LEditRows::calculateSize()
{
    if(m_isHiding){
        m_rect.reset();
        return;
    }

    LEditContainer::calculateSize();

    double height = 0;
    double maxWidth = 0;
    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        height += eb->fullHeight();
        maxWidth = std::max<double>(maxWidth, eb->fullWidth());
    }
    m_rect.setWidth(maxWidth);
    m_rect.setHeight(height);
}

void LEditRows::calculatePos()
{
    double y = m_rect.m_y;
    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        eb->m_rect.moveLeft(m_rect.m_x + eb->marginLeft());
        y += eb->marginTop();
        eb->m_rect.moveTop(y);
        y += eb->m_rect.height() + eb->marginBottom();
    }
    for (const std::shared_ptr<LEditBase>& eb : m_children)
    {
        if(eb->isContainer()){
            std::shared_ptr<LEditContainer> c =std::dynamic_pointer_cast<LEditContainer>(eb);
            c->calculatePos();
        }
    }
}

bool LEditRows::setToActivePoint(const LPoint<double> &p)
{
    if(bounds().contains(p)){
        return setToActivePointIgnoreWidth(p);
    }
    return false;
}

bool LEditRows::moveCursor(LMoveOper op)
{
    switch(op){
    case LMoveOper::Up:
    case LMoveOper::Down:{
        if(m_children.empty()){
            return false;
        }
        if(isValid(m_activeChild)){
            if(m_activeChild->moveCursor(op)){
                return true;
            }
        }

        std::shared_ptr<LEditBase> t;
        if(op == LMoveOper::Up){
            t = previousBlock();
        }else{
            t= nextBlock();
        }


        if(isValid(t) && t != m_activeChild){
            LPoint<double> pos = activePoint();
            m_activeChild = t;
            if(op == LMoveOper::Up){
                pos.m_y = m_activeChild->rect().y2();
            }else{
                pos.m_y = m_activeChild->rect().m_y;
            }
            m_activeChild->setToActivePoint(pos);

        }
        return true;
    }
    default:{
        break;
    }
    }
    LEditContainer::moveCursor(op);

    if(isAtEnd()){
        std::shared_ptr<LEditBase> t = lastBlock();
        if(isValid(t)){
            m_activeChild = t;
            m_activeChild->moveToEnd();
        }
    }else if(isAtStart()){
        std::shared_ptr<LEditBase> t = firstBlock();
        if(isValid(t)){
            m_activeChild = t;
            m_activeChild->moveToStart();
        }
    }
    return true;
}

bool LEditRows::setToActivePointIgnoreWidth(const LPoint<double> &p)
{
    m_activeChild = nullptr;
    double s = m_rect.m_y;
    for (const std::shared_ptr<LEditBase>& eb : m_children){
        double e = eb->m_rect.y2() + eb->marginBottom();
        if ((p.m_y >= s) && (p.m_y <= e)){
            m_activeChild = eb;
            break;
        }
        s = e;
    }

    if(isValid(m_activeChild)){
        return m_activeChild->setToActivePoint(p);
    }else{
        if(p.m_y >= midY()){
            std::shared_ptr<LEditBase> t = lastBlock();
            if(isValid(t)){
                m_activeChild = t;
                m_activeChild->setToActivePoint(p);
                return true;
            }
        }else{
            std::shared_ptr<LEditBase> t = firstBlock();
            if(isValid(t)){
                m_activeChild = t;
                m_activeChild->setToActivePoint(p);
                return true;
            }
        }
    }
    return false;
}


LK_NAMESPACE_END
