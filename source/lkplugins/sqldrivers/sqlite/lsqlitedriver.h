#ifndef LSQLITEDRIVER_H
#define LSQLITEDRIVER_H

#include "lksql/lsqldriver.h"
#include "lksql/lsqlresult.h"

class sqlite3;
LK_NAMESPACE_BEGIN
class LSqliteDriver : public LSqlDriver
{
public:
    LSqliteDriver();
    virtual ~LSqliteDriver();

    virtual bool open(const std::string &db,
                      const std::string &user = "",
                      const std::string &password = "",
                      const std::string &host = "",
                      int port = -1,
                      const std::string &connOpts = "") override;
    virtual void close() override;
    virtual bool hasFeature(DriverFeature f) const override;
    virtual std::shared_ptr<LSqlResult> createResult() const override;

    void addResult(std::shared_ptr<LSqlResult> r);
    void removeResult(std::shared_ptr<LSqlResult>);

    inline sqlite3 *access(){
        return m_access;
    }

    virtual bool beginTransaction() override;
    virtual bool commitTransaction() override;
    virtual bool rollbackTransaction() override;
    virtual std::list<std::string> tables(LFlags<LSqlTableType> tableType) const override;

private:
    void makeError(const std::string &descr, LSqlError::Type type, int errorCode);

    std::list<std::weak_ptr<LSqlResult> > m_results;
    std::list<std::string> m_notificationId;
    sqlite3 *m_access = nullptr;
};


LK_NAMESPACE_END

#endif //LSQLITEDRIVER_H
