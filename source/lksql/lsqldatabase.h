#ifndef LSQLDATABASE_H
#define LSQLDATABASE_H

#include "lsqldriver.h"
#include "lkcore/plugin/ldynamiclibrary.h"

LK_NAMESPACE_BEGIN

class LKSQLDEF LSqlDatabase
{
public:
    LSqlDatabase();
    LSqlDatabase(const std::string &driverName,
                 uint8_t loadPathFlags = LDynamicLibrary::AppPath|LDynamicLibrary::LookiPath);

    inline std::shared_ptr<LSqlDriver> driver() const{
        return m_driver;
    }

    inline const std::string& driverName() const{
        return m_driverName;
    }

    std::string m_dbName;
    std::string m_userName;
    std::string m_password;
    std::string m_hostName;

    int m_port=-1;
    std::string m_connectionOptions;
    std::string m_connectionName;
    LSqlPrecisionPolicy m_precisionPolicy = LSqlPrecisionPolicy::LowDouble;

    void init(const std::string& driverName, uint8_t loadPathFlagss);
    inline bool isValid() const{
        return m_driver != nullptr;
    }

    bool transaction();
    bool commit();
    bool rollback();

    const LSqlError& lastError() const;

    bool open();
    bool open(const std::string& user, const std::string& password);
    void close();
    bool isOpen() const;
    bool isOpenError() const;
    std::list<std::string> tables(LFlags<LSqlTableType> tableType = LSqlTableType::Tables) const;
private:
    std::string m_driverName;
    std::shared_ptr<LSqlDriver> m_driver;


};

LK_NAMESPACE_END

#endif // LSQLDATABASE_H
