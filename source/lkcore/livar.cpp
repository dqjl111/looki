#include "livar.h"
#include "lkcore/lstringutil.h"
#include "lkcore/ldebug.h"
LK_NAMESPACE_BEGIN

LIVar::LIVar()
{

}

LIVar::~LIVar()
{

}

std::string LIVar::typeName() const
{
    return NAMEOF_SHORT_TYPE_RTTI(*this);
}


LIVarTag::LIVarTag()
{

}

//

LIVarTag *LIVarTag::tag(const std::string &tagName)
{
    const std::unordered_map<std::string, LIVarTag* >& m_tags = tags();
    std::unordered_map<std::string, LIVarTag* >::const_iterator
        iter = m_tags.find(tagName);
    if( iter != m_tags.end()){
        return iter->second;
    }
    return nullptr;
}


bool LIVarTag::isMatchBefore(const std::string &type1, const std::string &type2)
{
    const std::unordered_map<std::string, LIVarTag* >& m_tags = tags();
    std::unordered_map<std::string, LIVarTag* >::const_iterator
        iter1 = m_tags.find(type1);
    if( iter1 != m_tags.end()){
        std::unordered_map<std::string, LIVarTag* >::const_iterator
            iter2 = m_tags.find(type2);
        if(iter2 != m_tags.end()){
            return isMatchBefore(iter1->second, iter2->second);
        }else{
            return true;
        }
    }else{
        std::unordered_map<std::string, LIVarTag* >::const_iterator
            iter2 = m_tags.find(type2);
        if(iter2 != m_tags.end()){
            return false;
        }
        return type1 < type2;
    }
}

bool LIVarTag::isMatchAfter(const std::string &type1, const std::string &type2)
{
    const std::unordered_map<std::string, LIVarTag* >& m_tags = tags();
    std::unordered_map<std::string, LIVarTag* >::const_iterator
        iter1 = m_tags.find(type1);
    if( iter1 != m_tags.end()){
        std::unordered_map<std::string, LIVarTag* >::const_iterator
            iter2 = m_tags.find(type2);
        if(iter2 != m_tags.end()){
            return isMatchAfter(iter1->second, iter2->second);
        }else{
            return false;
        }
    }else{
        std::unordered_map<std::string, LIVarTag* >::const_iterator
            iter2 = m_tags.find(type2);
        if(iter2 != m_tags.end()){
            return true;
        }
        return type1 > type2;
    }
}

bool LIVarTag::isMatchBefore(const LIVarTag *tag1, const LIVarTag *tag2)
{
    if(tag1 != nullptr){
        if(tag2 != nullptr){
            if(tag1 == tag2){
                return false;
            }

            if(tag1->isMatch(tag2)){ //父类在前,能转成父类,2能转1,1为父类
                return true;
            }

            if(tag2->isMatch(tag1)){ //父类在前,能转成父类,1能转2,2为父类
                return false;
            }
            return tag1->typeIndex() < tag2->typeIndex();
        }else{
            return true;
        }
    }else{
        return false;
    }
}

bool LIVarTag::isMatchAfter(const LIVarTag *tag1, const LIVarTag *tag2)
{
    if(tag1 != nullptr){
        if(tag2 != nullptr){
            if(tag1 == tag2){
                return false;
            }

            if(tag1->isMatch(tag2)){ //父类在前,能转成父类,2能转1,1为父类
                return false;
            }

            if(tag2->isMatch(tag1)){ //父类在前,能转成父类,1能转2,2为父类
                return true;
            }
            return tag1->typeIndex() < tag2->typeIndex();
        }else{
            return false;
        }
    }else{
        if(tag2 != nullptr){
            return true;
        }
        return false;
    }
}
LK_NAMESPACE_END
