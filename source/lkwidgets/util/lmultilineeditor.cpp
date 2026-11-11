#include "lmultilineeditor.h"
#include "lkwidgets/core/lapplication.h"
#include <iterator>
#include "leditdoc.h"
LK_NAMESPACE_BEGIN


LMultiLineEditor::LMultiLineEditor()
{

}

uint32_t LMultiLineEditor::job() const
{
    return uint32_t(LEditJob::Doc);
}


std::list<std::shared_ptr<LEditOp> > LMultiLineEditor::doInput(std::shared_ptr<LEditBase> item)
{
    if(item != nullptr){
        std::shared_ptr<LEditRow> curRow = activeRow(true);
        std::list<std::shared_ptr<LEditOp> > ret= curRow->putToActive(item, true, LEditRow::Merge);
        update(true, true);
        return ret;
    }
    return std::list<std::shared_ptr<LEditOp> >();
}


std::list<std::shared_ptr<LEditOp> > LMultiLineEditor::doInput(const std::wstring &text)
{
    std::list<std::shared_ptr<LEditOp> > ret;
    if(!text.empty()){
        bool prevEnter = false;
        std::wstring tempStr;
        for(wchar_t ch :text){
            if(isInputable(ch)){
                if(ch == '\r'){
                    if(!tempStr.empty()){
                        std::shared_ptr<LEditRow> curRow = activeRow(true);
                        ret.append_range(curRow->input(tempStr));
                        tempStr.clear();
                    }

                    prevEnter = true;
                    ret.append_range(doEnterNewItem());
                }else if(ch == '\n'){
                    if(prevEnter){
                        prevEnter = false;
                    }else{
                        if(!tempStr.empty()){
                            std::shared_ptr<LEditRow> curRow = activeRow(true);
                            ret.append_range(curRow->input(tempStr));
                            tempStr.clear();
                        }

                        ret.append_range(doEnterNewItem());
                    }
                }else{
                    tempStr.push_back(ch);
                }
            }
        }

        if(!tempStr.empty()){
            std::shared_ptr<LEditRow> curRow = activeRow(true);
            ret.append_range(curRow->input(tempStr));
        }
        update(true, true);
    }
    return ret;
}


std::list<std::shared_ptr<LEditOp>> LMultiLineEditor::doEnterNewItem()
{
    std::list<std::shared_ptr<LEditOp>> ret;

    if(!isValid(m_activeChild)){
        if(!m_children.empty()){
            if(m_activeChild == nullptr){
                m_activeChild = m_children.back();
                m_activeChild->moveToEnd();
            }else{
                m_activeChild = m_children.front();
                m_activeChild->moveToStart();
            }

        }
    }

    if(isValid(m_activeChild)){
        int row = lItemIndex(m_children, m_activeChild);
        m_activeChild = newRow(row+1);
    }else{
        m_activeChild = newRow(0);
    }

    std::shared_ptr<LRowInputDelete> op  = std::make_shared<LRowInputDelete>(true);
    m_activeChild->buildIndexes(op->m_editRowIndex);
    ret.push_back(op);
    return ret;
}


std::shared_ptr<LEditRow> LMultiLineEditor::activeRow(bool create)
{
    std::shared_ptr<LEditRow> curRow = nullptr;
    if(isValid(m_activeChild)){
        curRow = std::dynamic_pointer_cast<LEditRow>(m_activeChild);
    }
    if(curRow != nullptr){
        std::shared_ptr<LEditBase> t = curRow->activeChild();
        while(isValid(t)){
            if(t->isRow()){
                curRow = std::dynamic_pointer_cast<LEditRow>(t);
                t = curRow->activeChild();
            }else if(t->isContainer()){
                std::shared_ptr<LEditContainer> ec = std::dynamic_pointer_cast<LEditContainer>(t);
                t =ec->activeChild();
            }else{
                return curRow;
            }
        }
    }else{
        if(create){
            if(m_children.empty()){
                m_activeChild = newRow(0);
            }else{
                m_activeChild = m_children.front();
            }
            curRow = std::dynamic_pointer_cast<LEditRow>(m_activeChild);
        }
    }
    return curRow;
}

std::shared_ptr<LEditText> LMultiLineEditor::activeText()
{
    std::shared_ptr<LEditRow> curRow = activeRow();
    if(curRow != nullptr){
        return curRow->rowCurrentText();
    }
    return nullptr;
}

std::shared_ptr<LEditBase> LMultiLineEditor::activeBlock(bool create)
{
    std::shared_ptr<LEditContainer> c = nullptr;
    if(isValid(m_activeChild)){
        c = std::dynamic_pointer_cast<LEditContainer>(m_activeChild);
    }

    if(c != nullptr){
        std::shared_ptr<LEditBase> t = c->activeChild();
        while(isValid(t)){
            if(t->isContainer()){
                c = std::dynamic_pointer_cast<LEditContainer>(t);
                t = c->activeChild();
            }else{
                return t;
            }
        }
        return c;
    }else{
        if(create){
            if(m_children.empty()){
                m_activeChild = newRow(0);
            }
            c = std::dynamic_pointer_cast<LEditContainer>(m_activeChild);
        }
    }
    return c;
}

bool LMultiLineEditor::doMouseClick(const LPoint<double> &p)
{
    deSelect();
    LRect<double> r = bounds();
    if((r.m_y <= p.m_y) && (r.y2() >= p.m_y)){
        LEditRows::mouseClick(p);
    }else{
        moveToEnd();
    }
    update(false, true);
    return true;
}

bool LMultiLineEditor::checkUpdate()
{
    bool ret = m_isViewUpdate | m_isCaretUpdate;
    if(m_isViewUpdate){
        calculateRect();
        m_isViewUpdate = 0;
    }

    if(m_isCaretUpdate){
        calculateCaret();
        m_isCaretUpdate = 0;
    }
    return ret;
}

int LMultiLineEditor::contentRowNum() const
{
    if(m_children.empty()){
        return 1;
    }
    return m_children.size();
}


LK_NAMESPACE_END
