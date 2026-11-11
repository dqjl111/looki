#include "lsqlfield.h"

LK_NAMESPACE_BEGIN

LSqlField::LSqlField() {}

LSqlField::LSqlField(const std::string &colName, const std::string &tableName):m_name(colName), m_table(tableName)
{

}

void LSqlField::setValue(const LVariant &value)
{
    if(m_readOnly == false){
        m_value = value;
    }
}

void LSqlField::clear()
{
    if(m_readOnly == false){
        m_value.clear();
    }
}


LK_NAMESPACE_END
