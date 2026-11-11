#ifndef LUNDOSTACK_H
#define LUNDOSTACK_H
#include "lkwidgets/util/leditop.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LUndoStack
{
public:
    LUndoStack();

    virtual void undo(std::shared_ptr<LIEditor> editor);
    virtual void redo(std::shared_ptr<LIEditor> editor);

    void setUndoLimit(uint32_t limit);
    int undoLimit() const;
    void push(std::shared_ptr<LEditOp> op);
    int count() const;
    int index() const;
    void clear();

    bool canUndo() const;
    bool canRedo() const;
protected:
    std::list<std::shared_ptr<LEditOp>> m_ops;
    uint32_t m_index = 0;
    uint32_t m_limit = 100;

    void rebuildOps(uint32_t limit);
};


LK_NAMESPACE_END

#endif // LUNDOSTACK_H
