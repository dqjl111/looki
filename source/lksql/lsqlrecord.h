#ifndef LSQLRECORD_H
#define LSQLRECORD_H


#include "lksql/lsqlfield.h"

LK_NAMESPACE_BEGIN

class LKSQLDEF LSqlRecord : public LIVar
{
public:
    LSqlRecord();


    inline int indexOf(const std::string& fieldName) const{
        int ret=-1;
        field(fieldName, &ret);
        return ret;
    }

    inline int indexOf(const std::string& fieldName, const std::string& tableName) const{
        int ret=-1;
        field(fieldName, tableName, &ret);
        return ret;
    }

    std::shared_ptr<LSqlField> field(int index) const;
    std::shared_ptr<LSqlField> field(const std::string& fieldName, int* index) const;
    std::shared_ptr<LSqlField> field(const std::string& fieldName, const std::string& tableName, int* index) const;

    inline const std::list<std::shared_ptr<LSqlField> >& fields(){
        return m_fields;
    }

    inline bool isEmpty() const{
        return m_fields.empty();
    }

    bool append(std::shared_ptr<LSqlField> f);
    bool insert(int index, std::shared_ptr<LSqlField> f);
    bool replace(int index, std::shared_ptr<LSqlField> f);
    void remove(int index);

    inline void clear(){
        m_fields.clear();
    }

    inline int count() const{
        return m_fields.size();
    }

    void clearValues();
protected:
    std::list<std::shared_ptr<LSqlField> > m_fields;
};


LK_NAMESPACE_END


#endif // LSQLRECORD_H
