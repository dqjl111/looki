#ifndef LI18N_H
#define LI18N_H

#include <unordered_map>
#include "lkcore/lstringutil.h"

LK_NAMESPACE_BEGIN

class LI18n
{
public:
    LI18n();

    LKCOREDEF static void load(const std::string & path, bool clearOrg = false);
    LKCOREDEF static void clear();
    LKCOREDEF static const std::wstring& tr(const std::wstring& key, const std::wstring& defaultValue = LStringUtil::wempty);
    LKCOREDEF static const inline std::string trx(const std::wstring& key, const std::wstring& defaultValue = LStringUtil::wempty){
        return LStringUtil::wideToUtf8(tr(key, defaultValue));
    }
protected:
    static std::unordered_map<std::wstring, std::wstring> m_dict; //程序界面一般使用wchar
};

LK_NAMESPACE_END

#endif // LI18N_H
