#include "lplatformdialoghelper.h"

LK_NAMESPACE_BEGIN

LPlatformDialogHelper::LPlatformDialogHelper()
{

}

std::wstring LFileDialogOptions::suffixFromFilter(const std::wstring &filter)
{
    size_t suffixPos = filter.find(L"*.");
    if (suffixPos < 0){
        return LStringUtil::wempty;
    }
    suffixPos += 2;
    size_t endPos = filter.find(wchar_t(' '), suffixPos + 1);
    if (endPos < 0){
        endPos = filter.find(wchar_t(';'), suffixPos + 1);
    }
    if (endPos < 0){
        endPos = filter.find(wchar_t(')'), suffixPos + 1);
    }
    if (endPos < 0){
        endPos = filter.size();
    }
    return filter.substr(suffixPos, endPos - suffixPos);
}

LK_NAMESPACE_END
