#include "lsqlrecord.h"

LK_NAMESPACE_BEGIN

LSqlRecord::LSqlRecord() {}

std::shared_ptr<LSqlField> LSqlRecord::field(const std::string &fieldName, int *index) const
{
    int i =0;
    for(const std::shared_ptr<LSqlField>& f: m_fields){
        if(LStringUtil::compareIgnoreCase(f->m_name,fieldName) == 0){
            if(index != nullptr){
                *index = i;
            }
            return f;

        }else{
            ++i;
        }
    }
    return nullptr;
}

std::shared_ptr<LSqlField> LSqlRecord::field(const std::string &fieldName, const std::string &tableName, int *index) const
{
    int i =0;
    for(const std::shared_ptr<LSqlField>& f: m_fields){
        if((LStringUtil::compareIgnoreCase(f->m_name,fieldName) == 0)
            && (LStringUtil::compareIgnoreCase(f->m_table,tableName) == 0)){
            if(index != nullptr){
                *index = i;
            }
            return f;
        }else{
            ++i;
        }
    }
    return nullptr;
}

bool LSqlRecord::append(std::shared_ptr<LSqlField> f)
{
    if(f != nullptr){
        m_fields.push_back(f);
        return true;
    }
    return false;
}

bool LSqlRecord::insert(int index, std::shared_ptr<LSqlField> f)
{
    if(f != nullptr){
        if(index >=0 && index <m_fields.size()){
            auto it = m_fields.begin();
            std::advance(it, index);
            m_fields.insert(it, f);
            return true;
        }
    }
    return false;
}

bool LSqlRecord::replace(int index, std::shared_ptr<LSqlField> f)
{
    if(f != nullptr){
        if(index >=0 && index <m_fields.size()){
            auto it = m_fields.begin();
            std::advance(it, index);
            *it = f;
            return true;
        }
    }
    return false;
}

void LSqlRecord::remove(int index)
{
    if(index >=0 && index <m_fields.size()){
        auto it = m_fields.begin();
        std::advance(it, index);
        m_fields.erase(it);
    }
}

void LSqlRecord::clearValues()
{
    for(std::shared_ptr<LSqlField>& f: m_fields){
        f->clear();
    }
}

std::shared_ptr<LSqlField> LSqlRecord::field(int index) const
{
    if(index >=0 && index <m_fields.size()){
        auto it = m_fields.cbegin();
        std::advance(it, index);
        return *it;
    }
    return nullptr;
}
LK_NAMESPACE_END
