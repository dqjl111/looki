#include "lrccfileinfo.h"

LK_NAMESPACE_BEGIN

LRccFileInfo::LRccFileInfo() {}

std::shared_ptr<LRccFileInfo> LRccFileInfo::findChild(const std::string &node)
{
    for(const std::shared_ptr<LRccFileInfo>& info : m_children){
        if(info->m_name == node){
            return info;
        }
    }
    return nullptr;
}

bool LRccFileInfo::contains(const std::string &node)
{
    for(const std::shared_ptr<LRccFileInfo>& info : m_children){
        if(info->m_name == node){
            return true;
        }
    }
    return false;
}



LK_NAMESPACE_END
