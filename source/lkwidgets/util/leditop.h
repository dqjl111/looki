#ifndef LEDITOP_H
#define LEDITOP_H

#include "lkcore/io/lcbor.h"
#include <list>

LK_NAMESPACE_BEGIN
class LIEditor;
class LKWIDGETSDEF LEditOp
{
public:
    LEditOp();
    LEditOp(uint8_t type);
    enum Type{TextMerge, TextSplit,
              TextInput, TextDelete,
              ItemInput, ItemDelete,
              RowInput, RowDelete,
              MultiOp, UserOp};
    virtual void undo(std::shared_ptr<LIEditor> editor)=0;
    virtual void redo(std::shared_ptr<LIEditor> editor)=0;

    inline uint8_t type() const{
        return m_type;
    }
protected:
    uint8_t m_type = TextMerge;
};

class LKWIDGETSDEF LTextMergeSplit : public LEditOp{
public:
    LTextMergeSplit(bool isMerge = true){
        if(isMerge){
            m_type = TextMerge;
        }else{
            m_type = TextSplit;
        }
    }

    std::vector<int> m_mergeItemIndex;
    std::wstring m_text;
    bool m_hasSplit = false;

    virtual void undo(std::shared_ptr<LIEditor> editor) override;
    virtual void redo(std::shared_ptr<LIEditor> editor) override;
protected:
    void split(std::shared_ptr<LIEditor> editor);
    void merge(std::shared_ptr<LIEditor> editor);
};

class LTextInputDelete: public LEditOp{
public:
    LTextInputDelete(bool isInput = true){
        if(isInput){
            m_type = TextInput;
        }else{
            m_type = TextDelete;
        }
    }

    std::vector<int> m_editItemIndex;
    std::wstring m_text;

    virtual void undo(std::shared_ptr<LIEditor> editor) override;
    virtual void redo(std::shared_ptr<LIEditor> editor) override;
protected:
    void input(std::shared_ptr<LIEditor> editor);
    void del(std::shared_ptr<LIEditor> editor);
};

class LItemInputDelete : public LEditOp{
public:
    LItemInputDelete(bool isInput = true){
        if(isInput){
            m_type = ItemInput;
        }else{
            m_type = ItemDelete;
        }
    }

    std::vector<int> m_editContainerIndex;
    int m_pos=0;
    int m_validNum=1;
    LCbor::array m_inputCbor;

    virtual void undo(std::shared_ptr<LIEditor> editor) override;
    virtual void redo(std::shared_ptr<LIEditor> editor) override;
protected:
    void input(std::shared_ptr<LIEditor> editor);
    void del(std::shared_ptr<LIEditor> editor);
};

class LRowInputDelete : public LEditOp{
public:
    LRowInputDelete(bool isInput = true){
        if(isInput){
            m_type = RowInput;
        }else{
            m_type = RowDelete;
        }
    }

    std::vector<int> m_editRowIndex;
    virtual void undo(std::shared_ptr<LIEditor> editor) override;
    virtual void redo(std::shared_ptr<LIEditor> editor) override;
protected:
    void input(std::shared_ptr<LIEditor> editor);
    void del(std::shared_ptr<LIEditor> editor);
};

class LMultiEditOp : public LEditOp{
public:
    LMultiEditOp(){
        m_type = MultiOp;
    }

    LMultiEditOp(const std::vector<int>& beginIndex,
                 const std::vector<int>& endIndex,
                 const std::list<std::shared_ptr<LEditOp>>& ops,
                 bool isDone = true);

    virtual void undo(std::shared_ptr<LIEditor> editor) override;
    virtual void redo(std::shared_ptr<LIEditor> editor) override;

    std::vector<int> m_beginIndexes;
    std::vector<int> m_endIndexes;

    std::list<std::shared_ptr<LEditOp>> m_ops;
    bool m_isDone = true;
};


LK_NAMESPACE_END
#endif // LEDITOP_H
