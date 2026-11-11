#include "lmultitextlineeditor.h"
#include "lkwidgets/util/leditop.h"
#include "lkgui/text/lfontengine.h"

LK_NAMESPACE_BEGIN

LMultiTextLineEditor::LMultiTextLineEditor() {

}

uint32_t LMultiTextLineEditor::job() const
{
    return uint32_t(LEditJob::TextDoc);
}

std::list<std::shared_ptr<LEditOp> > LMultiTextLineEditor::doInput(const std::wstring &text)
{
    std::list<std::shared_ptr<LEditOp> > ret;
    if(!text.empty()){
        bool prevEnter = false;
        std::wstring tempStr;
        for(wchar_t ch :text){
            if(isInputable(ch)){
                if(ch == '\r'){
                    tempStr.push_back(lchar::LineSeparator);
                    ret.append_range(input(tempStr));
                    tempStr.clear();
                    prevEnter = true;
                }else if(ch == '\n'){
                    if(prevEnter){
                        prevEnter = false;
                    }else{
                        tempStr.push_back(lchar::LineSeparator);
                        ret.append_range(input(tempStr));
                        tempStr.clear();
                    }
                }else{
                    tempStr.push_back(ch);
                }
            }
        }

        if(!tempStr.empty()){
            ret.append_range(input(tempStr));
        }
        update(true, true);
        return ret;
    }
    return std::list<std::shared_ptr<LEditOp> >();
}

bool LMultiTextLineEditor::checkUpdate()
{
    bool ret = m_isViewUpdate | m_isCaretUpdate;
    if(m_isViewUpdate){
        calculateSize();
        m_isViewUpdate = 0;
    }

    if(m_isCaretUpdate){
        calculateCaret();
        m_isCaretUpdate = 0;
    }
    return ret;
}

void LMultiTextLineEditor::setMargins(const LMargins<double> &v)
{
    m_margin = v;
    update(true, true);
}


LRect<double> LMultiTextLineEditor::cursorRect() const
{
    LTextLine line = m_textLayout.lineForTextPosition(m_activePos);
    double cix = line.cursorToX(m_activePos)+m_rect.m_x;;
    double ciy = line.y()+m_rect.m_y;;
    int w = m_cursorWidth;
    double ch = line.height();
    if(lFuzzyIsZero(ch)){
        std::shared_ptr<LFontEngine> e = LIEditor::font()->engine();
        if(e != nullptr){
            ch = e->ascent() + e->descent();
        }else{
            ch =11;
        }
    }
    return LRect<double>(cix-5, ciy, w+9, ch, false);
}

LRect<double> LMultiTextLineEditor::anchorRect() const
{
    if (!hasSelect()){
        return cursorRect();
    }
    return rectForPos(m_partSelectStartId, m_cursorWidth);
}

int LMultiTextLineEditor::contentRowNum() const
{
    int ret = m_textLayout.lineCount();
    if(ret <= 0){
        return 1;
    }
    return  ret;
}

uint32_t LMultiTextLineEditor::contentLength() const
{
    return (uint32_t)m_textLayout.text().length();
}

void LMultiTextLineEditor::updateMaxLength()
{
    if(m_maxLength >= 0){
        uint32_t tLen = contentLength();
        if(tLen > (uint32_t)m_maxLength){
            setText(text().substr(0, m_maxLength));
        }
    }
}




LK_NAMESPACE_END
