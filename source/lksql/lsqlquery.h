#ifndef LSQLQUERY_H
#define LSQLQUERY_H

#include "lksql/lsqlrecord.h"
#include "lksql/lsqlresult.h"
#include "lkcore/lvariant.h"
#include "lksql/lsqldatabase.h"

LK_NAMESPACE_BEGIN

class LKSQLDEF LSqlQuery
{
public:
    LSqlQuery(std::shared_ptr<LSqlResult> result);
    LSqlQuery(std::shared_ptr<LSqlDatabase> db);
    LSqlQuery(const std::string& query, std::shared_ptr<LSqlDatabase> db);
    LSqlQuery(const LSqlQuery& org);
    LSqlQuery(const LSqlQuery&& org);

    std::shared_ptr<LSqlRecord> record() const;
    bool next();
    bool previous();
    bool first();
    bool last();
    void clear();
    bool exec();
    bool exec(const std::string& query);

    bool prepare(const std::string& query);
    void bindValue(const std::string& placeholder, const LVariant& val, LFlags<LSqlIoFlag> type = LSqlIoFlag::In);
    void bindValue(int pos, const LVariant& val, LFlags<LSqlIoFlag> type = LSqlIoFlag::In);
    void addBindValue(const LVariant& val, LFlags<LSqlIoFlag> type = LSqlIoFlag::In);
    const LVariant& boundValue(const std::string& placeholder) const;
    const LVariant& boundValue(int index) const;
    std::list<LVariant> boundValues() const;

    const LVariant& value(int i) const;
    const LVariant& value(const std::string &name) const;

    bool isValid() const;
    bool isActive() const;
    bool isSelect() const;
    bool isForwardOnly() const;
    void setForwardOnly(bool forward);

    int row() const;
    const std::string& lastQuery() const;
    int numRowsAffected() const;

    void setPositionalBindingEnabled(bool enable);
    bool isPositionalBindingEnabled() const;

    bool seek(int i, bool relative = false);
    const LSqlError& lastError() const;


    std::shared_ptr<LSqlDriver> driver() const;
    std::shared_ptr<LSqlResult> result() const;

    inline LSqlQuery& operator=(const LSqlQuery& org){
        m_result = org.m_result; return *this;
    }
    inline LSqlQuery& operator=(const LSqlQuery&& org){
        m_result = org.m_result; return *this;
    }
protected:
    std::shared_ptr<LSqlResult> m_result;
};


LK_NAMESPACE_END

#endif // LSQLQUERY_H
