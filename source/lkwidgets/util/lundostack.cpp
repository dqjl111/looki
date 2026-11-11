#include "lundostack.h"
#include <iterator>
#include "lmultilineeditor.h"
LK_NAMESPACE_BEGIN


LUndoStack::LUndoStack()
{

}

void LUndoStack::undo(std::shared_ptr<LIEditor> editor)
{
    if(m_ops.empty()){
        return;
    }

    if(m_index >= m_ops.size()){
        std::shared_ptr<LEditOp> op = m_ops.back();
        op->undo(editor);
        m_index = m_ops.size()-1;
    }else if(m_index >0){
        std::list<std::shared_ptr<LEditOp>>::iterator iter = m_ops.begin();
        std::advance(iter, m_index-1);
        std::shared_ptr<LEditOp> op = *iter;
        op->undo(editor);
        m_index --;
    }
    editor->update(true, true);
}

void LUndoStack::redo(std::shared_ptr<LIEditor> editor)
{
    if(m_ops.empty()){
        return;
    }

    if(m_index <m_ops.size()){
        std::list<std::shared_ptr<LEditOp>>::iterator iter = m_ops.begin();
        std::advance(iter, m_index);
        std::shared_ptr<LEditOp> op = *iter;
        op->redo(editor);
        m_index++;
    }
    editor->update(true, true);
}

void LUndoStack::setUndoLimit(uint32_t limit)
{
    m_limit = limit;
    rebuildOps(m_limit);
}

int LUndoStack::undoLimit() const
{
    return m_limit;
}

void LUndoStack::push(std::shared_ptr<LEditOp> op)
{
    rebuildOps(m_limit-1);
    m_ops.push_back(op);
    m_index = m_ops.size();
}

int LUndoStack::count() const
{
    return m_ops.size();
}

int LUndoStack::index() const
{
    return m_index;
}

void LUndoStack::clear()
{
    m_ops.clear();
    m_index = 0;
}

bool LUndoStack::canUndo() const
{
    if (!m_ops.empty()){
        return false;
    }
    return m_index > 0;
}

bool LUndoStack::canRedo() const
{
    if (!m_ops.empty()){
        return false;
    }
    return m_index  < m_ops.size();;
}

void LUndoStack::rebuildOps(uint32_t limit)
{
    if(m_index < m_ops.size()){
        std::list<std::shared_ptr<LEditOp>>::iterator iter = m_ops.begin();
        std::advance(iter, m_index);
        m_ops.erase(iter, m_ops.end());
    }

    if(m_ops.size() > limit){
        std::list<std::shared_ptr<LEditOp>>::iterator iter = m_ops.begin();
        std::advance(iter, m_ops.size()-limit);
        m_ops.erase(m_ops.begin(), iter);
    }
}


LK_NAMESPACE_END
