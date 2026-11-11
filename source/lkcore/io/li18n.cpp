#include "li18n.h"
#include "lkcore/io/ljson.h"
#include "lkcore/ldebug.h"
LK_NAMESPACE_BEGIN

std::unordered_map<std::wstring, std::wstring> LI18n::m_dict;

LI18n::LI18n() {}

void LI18n::load(const std::string &path, bool clearOrg)
{
    if(clearOrg){
        m_dict.clear();
    }
    LJson tJ = LJson::parseFile(path);
    if(tJ.isObject()){
        LJson::map obj = tJ.toMap();
        LJson::map::const_iterator iter = obj.cbegin();
        for(; iter != obj.cend(); ++iter){
            LJson s = iter->second;
            if(s.isString()){
                std::wstring t = LStringUtil::utf8ToWide(s.toString());
                if(!t.empty()){
                    m_dict[LStringUtil::utf8ToWide(iter->first)] = t;
                }
            }
        }
    }else{
        lWarn() << "Invalid type:" << tJ.type();
    }
}

void LI18n::clear()
{
    m_dict.clear();
}

const std::wstring &LI18n::tr(const std::wstring &key, const std::wstring &defaultValue)
{
    std::unordered_map<std::wstring, std::wstring>::const_iterator iter = m_dict.find(key);
    if(iter != m_dict.cend()){
        return iter->second;
    }
    if(defaultValue.empty()){
        return key;
    }
    return defaultValue;
}


LK_NAMESPACE_END
