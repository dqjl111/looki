#include "llineeditor.h"

LK_NAMESPACE_BEGIN

LLineEditor::LLineEditor()
{
}


void LLineEditor::moveToStart()
{
    if(!m_children.empty()){
        std::shared_ptr<LEditBase> child = m_children.front();
        if(child->isText()){
            m_activeChild = child;
            child->moveToStart();
            return;
        }
    }
    return LEditRow::moveToStart();
}

void LLineEditor::moveToEnd()
{
    if(!m_children.empty()){
        std::shared_ptr<LEditBase> child = m_children.back();
        if(child->isText()){
            m_activeChild = child;
            child->moveToEnd();
            return;
        }
    }
    return LEditRow::moveToStart();
}

std::wstring LLineEditor::selectText() const
{
    std::wstring ret;
    for(const std::shared_ptr<LEditBase>& child: m_children){
        if(child->isText()){
            std::shared_ptr<LEditText> textRow =  std::dynamic_pointer_cast<LEditText>(child);
            ret += textRow->selectText();
        }
    }
    return ret;
}

uint32_t LLineEditor::job() const
{
    return uint32_t(LEditJob::Line);
}

std::list<std::shared_ptr<LEditOp> > LLineEditor::doInput(const std::wstring &text)
{
    if(!text.empty()){
        uint32_t tLen = contentLength();
        std::wstring tempStr;
        for(wchar_t ch :text){
            if(m_maxLength < 0 || tLen < (uint32_t)m_maxLength){
                if(isInputable(ch)){
                    tempStr.push_back(ch);
                    tLen += 1;
                }
            }else{
                break;
            }
        }

        if(!tempStr.empty()){
            std::list<std::shared_ptr<LEditOp> > ret = input(tempStr);
            update(true, true);
            return ret;
        }
    }
    return std::list<std::shared_ptr<LEditOp> >();
}

bool LLineEditor::checkUpdate()
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


uint32_t LLineEditor::contentLength() const
{
    uint32_t ret = 0;
    for (const std::shared_ptr<LEditBase>& child : m_children)
    {
        if(child->isText()){
            std::shared_ptr<LEditText> textRow =  std::dynamic_pointer_cast<LEditText>(child);
            ret += (uint32_t)(textRow->text().length());
        }
    }
    return ret;
}

void LLineEditor::setMargins(const LMargins<double> &v)
{
    m_margin = v;
    m_rect.m_x = m_margin.m_left;
    m_rect.m_y = m_margin.m_right;
    update(true, true);
}


double LLineEditor::cursorToX() const
{
    double ret = m_margin.m_left;
    if(m_children.empty()){
        return ret;
    }

    if(m_activeChild == startBase()){
        return ret;
    }

    if(m_activeChild == nullptr){
        ret += m_rect.width();
        return ret;
    }


    for (const std::shared_ptr<LEditBase>& child : m_children)
    {
        if(child != m_activeChild){
            ret += child->fullWidth();
        }else{
            if(child->isText()){
                std::shared_ptr<LEditText> textRow =  std::dynamic_pointer_cast<LEditText>(child);
                ret += textRow->cursorToX();
                return ret;
            }else{
                ret += child->fullWidth();
                return ret;
            }
        }
    }
    return ret;
}


std::wstring LLineEditor::content() const
{
    std::wstring ret;
    for (const std::shared_ptr<LEditBase>& child : m_children)
    {
        if(child->isText()){
            std::shared_ptr<LEditText> textRow =  std::dynamic_pointer_cast<LEditText>(child);
            ret += textRow->text();
        }
    }
    return ret;
}

std::wstring LLineEditor::selectContent() const
{
    std::wstring ret;
    for (const std::shared_ptr<LEditBase>& child : m_children)
    {
        if(child->isText()){
            std::shared_ptr<LEditText> textRow =  std::dynamic_pointer_cast<LEditText>(child);
            ret += textRow->selectText();
        }
    }
    return ret;
}


bool LLineEditor::isInputable(const wchar_t c) const
{
    if(c == '\r' || c == '\n'){
        return false;
    }
    return LTEditor<LEditRow>::isInputable(c);
}

void LLineEditor::updateMaxLength()
{
    if(m_maxLength >= 0){
        uint32_t tLen = contentLength();
        if(tLen > (uint32_t)m_maxLength){
            setContent(content().substr(0, m_maxLength));
        }
    }
}



LK_NAMESPACE_END
