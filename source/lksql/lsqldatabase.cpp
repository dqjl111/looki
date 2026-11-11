#include "lsqldatabase.h"
#include "lkcore/plugin/ldynamiclibrary.h"
#include "lksql/lsqldriverplugin.h"

LK_NAMESPACE_BEGIN

LSqlDatabase::LSqlDatabase() {}

LSqlDatabase::LSqlDatabase(const std::string &driverName, uint8_t loadPathFlagss)
{
    init(driverName, loadPathFlagss);
}

void LSqlDatabase::init(const std::string &driverName, uint8_t loadPathFlagss)
{
    if((m_driverName == driverName) && m_driver != nullptr){
        return;
    }    
    m_driverName = driverName;
#ifdef _WIN32
    std::shared_ptr<LSqlDriverPlugin> p =
        LDynamicLibrary::loadPluginT<LSqlDriverPlugin>(std::string("plugins/sqldrivers/") + m_driverName, loadPathFlagss);
#else
    std::shared_ptr<LSqlDriverPlugin> p =
        LDynamicLibrary::loadPluginT<LSqlDriverPlugin>(std::string("plugins/sqldrivers/lib") + m_driverName, loadPathFlagss);
#endif
    if(p != nullptr){
        m_driver = p->create();
    }
}

bool LSqlDatabase::transaction()
{
    if(m_driver != nullptr){
        if (m_driver->hasFeature(LSqlDriver::Transactions)){
            return m_driver->beginTransaction();
        }
    }
    return false;
}

bool LSqlDatabase::commit()
{
    if(m_driver != nullptr){
        if (m_driver->hasFeature(LSqlDriver::Transactions)){
            return m_driver->commitTransaction();
        }
    }
    return false;
}

bool LSqlDatabase::rollback()
{
    if(m_driver != nullptr){
        if (m_driver->hasFeature(LSqlDriver::Transactions)){
            return m_driver->rollbackTransaction();
        }
    }
    return false;
}

const LSqlError &LSqlDatabase::lastError() const
{
    if(m_driver != nullptr){
        return m_driver->lastError();
    }
    return LSqlError::staticNoError();
}

bool LSqlDatabase::open()
{
    if(m_driver != nullptr){
        return m_driver->open(m_dbName, m_userName, m_password, m_hostName, m_port, m_connectionOptions);
    }
    return false;
}

bool LSqlDatabase::open(const std::string &user, const std::string &password)
{
    m_userName = user;
    if(m_driver != nullptr){
        return m_driver->open(m_dbName, user, password, m_hostName, m_port, m_connectionOptions);
    }
    return false;
}

void LSqlDatabase::close()
{
    if(m_driver != nullptr){
        return m_driver->close();
    }
}

bool LSqlDatabase::isOpen() const
{
    if(m_driver != nullptr){
        return m_driver->isOpen();
    }
    return false;
}

bool LSqlDatabase::isOpenError() const
{
    if(m_driver != nullptr){
        return m_driver->isOpenError();
    }
    return true;
}

std::list<std::string> LSqlDatabase::tables(LFlags<LSqlTableType> tableType) const
{
    if(m_driver != nullptr){
        return m_driver->tables(tableType);
    }
    return std::list<std::string>();
}

LK_NAMESPACE_END
