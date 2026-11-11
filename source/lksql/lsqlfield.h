#ifndef LSQLFIELD_H
#define LSQLFIELD_H

#include "lkcore/livar.h"
#include "lkcore/lvariant.h"
LK_NAMESPACE_BEGIN

class LKSQLDEF LSqlField : public LIVar
{
public:
    LSqlField();
    LSqlField(const std::string& colName, const std::string& tableName);

    enum RequiredStatus { Unknown = 0, Optional = 1, Required = 2 };

    void setValue(const LVariant& value);
    inline const LVariant& value() const
    { return m_value; }

    void clear();

    std::string m_table;
    std::string m_name;
    LVariant m_defaultValue;
    uint32_t m_precision = 0;
    uint32_t m_length = 0;
    union{
        uint8_t m_fieldFlags = 0;
        struct{
            uint8_t m_requiredStatus : 2;
            uint8_t m_readOnly : 1;
            uint8_t m_generated: 1;
            uint8_t m_autoValue : 1;
        };
    };
protected:
    LVariant m_value;

};


LK_NAMESPACE_END

#endif // LSQLFIELD_H
