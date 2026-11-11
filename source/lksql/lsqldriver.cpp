#include "lsqldriver.h"
LK_NAMESPACE_BEGIN


LSqlError::LSqlError()
{

}

void LSqlError::buildError(Type type,
                           const std::string &driverError,
                           const std::string &databaseError,
                           const std::string &errorCode)
{
    m_type = type;
    m_driverError = driverError;
    m_databaseError = databaseError;
    m_errorCode = errorCode;
}

void LSqlError::clear()
{
    m_type = No;
    m_driverError.clear();
    m_databaseError.clear();
    m_errorCode.clear();
}

std::string LSqlError::toString() const
{
    if(m_type == No){
        return "";
    }
    std::ostringstream result;
    result << "LSqlError(" << m_errorCode << ", " << m_driverError  << ", " << m_databaseError << ')';
    return result.str();
}

const LSqlError &LSqlError::staticNoError()
{
    const static LSqlError noError;
    return noError;
}


LSqlDriver::LSqlDriver()
{

}

LSqlDriver::~LSqlDriver()
{

}

std::string LSqlDriver::formatValue(const std::shared_ptr<LSqlField> field, bool trimStrings) const
{
    const std::string null("NULL");
    if(field == nullptr){
        return null;
    }
    if(field->value().isNull()){
        return null;
    }
    return "";
}

bool LSqlDriver::beginTransaction()
{
    return false;
}

bool LSqlDriver::commitTransaction()
{
    return false;
}

bool LSqlDriver::rollbackTransaction()
{
    return false;
}

std::list<std::string> LSqlDriver::tables(LFlags<LSqlTableType> tableType) const
{
    return std::list<std::string>();
}



LK_NAMESPACE_END
