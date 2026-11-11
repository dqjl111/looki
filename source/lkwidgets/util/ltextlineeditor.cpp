#include "ltextlineeditor.h"

LK_NAMESPACE_BEGIN

LTextLineEditor::LTextLineEditor() {

}

bool LTextLineEditor::isInputable(const wchar_t c) const
{
    if(c == '\r' || c == '\n'){
        return false;
    }
    return LMultiTextLineEditor::isInputable(c);
}

LK_NAMESPACE_END
