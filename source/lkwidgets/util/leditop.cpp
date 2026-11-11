#include "leditop.h"
#include "lmultilineeditor.h"
#include "ledittext.h"
#include "leditdoc.h"
LK_NAMESPACE_BEGIN

LEditOp::LEditOp(){
}

LEditOp::LEditOp(uint8_t type):m_type(type)
{
}

void LTextMergeSplit::undo(std::shared_ptr<LIEditor> editor)
{
    if(m_type == TextMerge){
        split(editor);
    }else{
        merge(editor);
    }
}

void LTextMergeSplit::redo(std::shared_ptr<LIEditor> editor)
{
    if(m_type == TextMerge){
        merge(editor);
    }else{
        split(editor);
    }
}

void LTextMergeSplit::split(std::shared_ptr<LIEditor> editor)
{
    if(editor != nullptr){
        LIndexItem item = editor->indexItem(m_mergeItemIndex);
        if(item.baseItem != nullptr){
            editor->setActiveIndexes(m_mergeItemIndex);
            if(item.textPos>=0){
                std::shared_ptr<LEditText> textItem = std::dynamic_pointer_cast<LEditText>(item.baseItem);
                if(textItem != nullptr){
                    std::wstring prev = textItem->text().substr(0, textItem->activePos());
                    m_text = textItem->text().substr(textItem->activePos());
                    if(prev.empty()){
                        m_hasSplit = false;
                        if(m_type == TextMerge){
                            textItem->clear();
                            textItem->setActivePos(0);
                        }else{
                            m_text.clear();
                            textItem->setActivePos(0);
                        }
                    }else{
                        if(!m_text.empty()){
                            textItem->setText(prev);
                            textItem->setActivePos((int)textItem->text().size());
                            textItem = std::make_shared<LEditText>();
                            textItem->setText(m_text);

                            std::shared_ptr<LEditContainer> parent = item.baseItem->parent();
                            if(parent != nullptr){
                                if(parent->isRow()){
                                    std::shared_ptr<LEditRow> row = std::dynamic_pointer_cast<LEditRow>(parent);
                                    int pos = row->childIndex(item.baseItem);
                                    if(pos>=0){
                                        row->insert(textItem, pos+1);
                                    }
                                    m_hasSplit = true;
                                }
                            }
                        }else{
                            m_hasSplit = false;
                        }
                    }
                }
            }
        }
    }
}

void LTextMergeSplit::merge(std::shared_ptr<LIEditor> editor)
{
    if(editor != nullptr){
        LIndexItem item = editor->indexItem(m_mergeItemIndex);
        if(item.baseItem != nullptr){
            std::shared_ptr<LEditText> textItem = std::dynamic_pointer_cast<LEditText>(item.baseItem);
            if(textItem != nullptr){
                editor->setActiveIndexes(m_mergeItemIndex);
                if(item.textPos>=0){
                    if(m_hasSplit){
                        std::vector<int> splitItemIndex;
                        for(int i=0; i<item.validLevel-2; ++i){
                            splitItemIndex.push_back(m_mergeItemIndex[i]);
                        }

                        if(item.validLevel>1){
                            splitItemIndex.push_back(m_mergeItemIndex[item.validLevel-1]+1);
                        }

                        LIndexItem splitItem = editor->indexItem(splitItemIndex);
                        if(splitItem.baseItem != nullptr){
                            if(splitItem.baseItem->isText()){
                                std::shared_ptr<LEditText> tItem = std::dynamic_pointer_cast<LEditText>(splitItem.baseItem);
                                m_text = tItem->text();
                                std::shared_ptr<LEditContainer> parent = tItem->parent();
                                if(parent != nullptr){
                                    parent->deleteChild(item.baseItem);
                                }else{
                                    lWarn() << "Unexpect no parent item here!";
                                }
                            }
                        }
                    }

                    textItem->setActivePos(item.textPos);
                    textItem->inputText(m_text);
                }
            }
        }
    }
}

void LTextInputDelete::undo(std::shared_ptr<LIEditor> editor)
{
    if(m_type == TextInput){
        del(editor);
    }else{
        input(editor);
    }
}

void LTextInputDelete::redo(std::shared_ptr<LIEditor> editor)
{
    if(m_type == TextInput){
        input(editor);
    }else{
        del(editor);
    }
}

void LTextInputDelete::input(std::shared_ptr<LIEditor> editor)
{
    if(editor != nullptr){
        LIndexItem item = editor->indexItem(m_editItemIndex);
        if(item.baseItem!= nullptr){
            editor->setActiveIndexes(m_editItemIndex);
            if(item.textPos>=0){
                std::shared_ptr<LEditText> textItem = std::dynamic_pointer_cast<LEditText>(item.baseItem);
                if(textItem != nullptr){
                    textItem->setActivePos(item.textPos);
                    textItem->inputText(m_text);
                }
            }
        }
    }
}

void LTextInputDelete::del(std::shared_ptr<LIEditor> editor)
{
    if(editor != nullptr){
        LIndexItem item = editor->indexItem(m_editItemIndex);
        if(item.baseItem!= nullptr){
            editor->setActiveIndexes(m_editItemIndex);
            if(item.textPos>=0){
                std::shared_ptr<LEditText> textItem = std::dynamic_pointer_cast<LEditText>(item.baseItem);
                if(textItem != nullptr){
                    textItem->deletePos(item.textPos-(int)m_text.size(), (int)m_text.size());
                }
            }
        }
    }
}

void LItemInputDelete::undo(std::shared_ptr<LIEditor> editor)
{
    if(m_type == ItemInput){
        del(editor);
    }else{
        input(editor);
    }
}

void LItemInputDelete::redo(std::shared_ptr<LIEditor> editor)
{
    if(m_type == ItemInput){
        input(editor);
    }else{
        del(editor);
    }
}

void LItemInputDelete::input(std::shared_ptr<LIEditor> editor)
{
    if(editor != nullptr){
        LIndexItem item = editor->indexItem(m_editContainerIndex);
        if(item.baseItem != nullptr){
            editor->setActiveIndexes(m_editContainerIndex);
            if(item.baseItem->isContainer()){
                std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(item.baseItem);
                LCbor::array::iterator iter = m_inputCbor.begin();

                int tPos = m_pos;
                std::shared_ptr<LEditBase> child = nullptr;
                for(; iter != m_inputCbor.end(); ++iter){
                    std::shared_ptr<LEditBase> child = LEditDoc::createBase(*iter, true, editor->doc());
                    if(child != nullptr){
                        c->insert(child, tPos++);
                    }
                }
                if(child){
                    c->setActiveChild(child);
                    child->moveCursor(LMoveOper::EndOfBlock);
                }
                m_validNum = tPos-m_pos;
            }
        }
    }
}

void LItemInputDelete::del(std::shared_ptr<LIEditor> editor)
{
    if(editor != nullptr){
        LIndexItem item = editor->indexItem(m_editContainerIndex);
        if(item.baseItem != nullptr){
            editor->setActiveIndexes(m_editContainerIndex);
            if(item.baseItem->isContainer()){
                std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(item.baseItem);
                if(m_inputCbor.empty()){
                    m_inputCbor = c->childrenCbor(m_pos, m_validNum);
                }
                c->del(m_pos-m_validNum, m_validNum);
            }
        }
    }
}

void LRowInputDelete::undo(std::shared_ptr<LIEditor> editor)
{
    if(m_type == RowInput){
        del(editor);
    }else{
        input(editor);
    }
}

void LRowInputDelete::redo(std::shared_ptr<LIEditor> editor)
{
    if(m_type == RowInput){
        input(editor);
    }else{
        del(editor);
    }
}

void LRowInputDelete::input(std::shared_ptr<LIEditor> editor)
{
    std::shared_ptr<LMultiLineEditor> editorX = std::dynamic_pointer_cast<LMultiLineEditor>(editor);
    if(editorX != nullptr){
        if(m_editRowIndex.size()>0){
            if(m_editRowIndex[0]>=0){
                editorX->newRow(m_editRowIndex[0]);
            }else{
                editorX->newRow(0);
            }
        }else{
            editorX->newRow(0);
        }
    }
}

void LRowInputDelete::del(std::shared_ptr<LIEditor> editor)
{
    std::shared_ptr<LMultiLineEditor> editorX = std::dynamic_pointer_cast<LMultiLineEditor>(editor);
    if(editorX != nullptr){
        editorX->doDelete(m_editRowIndex);
    }
}


LMultiEditOp::LMultiEditOp(const std::vector<int> &beginIndex,
                           const std::vector<int> &endIndex,
                           const std::list<std::shared_ptr<LEditOp> > &ops,
                           bool isDone)
    : LEditOp(MultiOp)
    , m_beginIndexes(beginIndex)
    , m_endIndexes(endIndex)
    , m_ops(ops)
    , m_isDone(isDone)
{

}

void LMultiEditOp::undo(std::shared_ptr<LIEditor> editor)
{
    if(editor != nullptr){
        if(m_isDone){
            editor->setActiveIndexes(m_endIndexes);
            std::list<std::shared_ptr<LEditOp>>::reverse_iterator iter = m_ops.rbegin();
            for(; iter != m_ops.rend(); ++iter){
                (*iter)->undo(editor);
            }
            editor->setActiveIndexes(m_beginIndexes);
            m_isDone = false;
        }
    }
}

void LMultiEditOp::redo(std::shared_ptr<LIEditor> editor)
{
    std::list<std::shared_ptr<LEditOp>>::iterator iter = m_ops.begin();
    for(; iter != m_ops.end(); ++iter){
        (*iter)->undo(editor);
    }

    if(editor != nullptr){
        if(!m_isDone){
            editor->setActiveIndexes(m_beginIndexes);
            for(std::shared_ptr<LEditOp> op: m_ops){
                op->redo(editor);
            }
            editor->setActiveIndexes(m_endIndexes);
            m_isDone = true;
        }
    }
}



LK_NAMESPACE_END


