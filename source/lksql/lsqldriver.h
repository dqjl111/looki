#ifndef LSQLDRIVER_H
#define LSQLDRIVER_H

#include "lkcore/livar.h"
#include "lkcore/lflags.h"
#include "lksql/lsqlfield.h"

LK_NAMESPACE_BEGIN


enum class LSqlLocation
{
    BeforeFirstRow = -1,
    AfterLastRow = -2
};

enum class LSqlIoFlag
{
    In = 0x00000001,
    Out = 0x00000002,
    InOut = In | Out,
    Binary = 0x00000004
};

enum class LSqlTableType
{
    Tables = 0x01,
    System = 0x02,
    Views = 0x04,
    All = 0xff
};

enum class LSqlPrecisionPolicy
{
    LowInt32    = 0x01,
    LowInt64    = 0x02,
    LowDouble   = 0x04,
    High        = 0
};

L_EN_FLAGS(LSqlTableType)
L_EN_FLAGS(LSqlIoFlag)

class LKSQLDEF LSqlError{
public:
    LSqlError();

    LSqlError(LSqlError &other) = default;
    LSqlError(LSqlError &&other) noexcept = default;
    LSqlError& operator=(const LSqlError &other) = default;

    enum Type{
        No,
        Connection,
        Statement,
        Transaction,
        Unknown
    };

    Type m_type = No;
    std::string m_driverError;
    std::string m_databaseError;
    std::string m_errorCode;

    void buildError(Type type,
                    const std::string& driverError = "",
                    const std::string& databaseError = "",
                    const std::string& errorCode = "");

    void clear();
    inline bool isValid() const{
        return m_type != No;
    }

    std::string toString() const;
    static const LSqlError& staticNoError();
};

class LSqlResult;
class LKSQLDEF LSqlDriver : public LIVar
{
public:
    LSqlDriver();
    virtual ~LSqlDriver();

    enum DbmsType {
        UnknownDbms,
        MSSqlServer,
        MySqlServer,
        PostgreSQL,
        Oracle,
        Sybase,
        SQLite,
        Interbase,
        DB2,
        MimerSQL,
    };

    enum DriverFeature { Transactions, QuerySize, BLOB, Unicode, PreparedQueries,
                         NamedPlaceholders, PositionalPlaceholders, LastInsertId,
                         BatchOperations, SimpleLocking, LowPrecisionNumbers,
                         EventNotifications, FinishQuery, MultipleResultSets, CancelQuery };

    virtual bool open(const std::string &db,
                      const std::string &user = "",
                      const std::string &password = "",
                      const std::string &host = "",
                      int port = -1,
                      const std::string &connOpts = "") = 0;

    inline bool isOpen() const{
        return m_isOpen;
    }

    inline bool isOpenError() const{
        return m_isOpenError;
    }

    virtual void close() = 0;
    virtual bool hasFeature(DriverFeature f) const = 0;
    virtual std::shared_ptr<LSqlResult> createResult() const = 0;

    const LSqlError& lastError() const{
        return m_error;
    }

    virtual std::string formatValue(const std::shared_ptr<LSqlField> field, bool trimStrings = false) const;

    inline void setNumericalPrecisionPolicy(LSqlPrecisionPolicy precisionPolicy){
        m_precisionPolicy = precisionPolicy;
    }

    inline LSqlPrecisionPolicy numericalPrecisionPolicy() const{
        return m_precisionPolicy;
    }

    inline DbmsType dbmsType() const{
        return m_dbmsType;
    }


    virtual bool beginTransaction();
    virtual bool commitTransaction();
    virtual bool rollbackTransaction();
    virtual std::list<std::string> tables(LFlags<LSqlTableType> tableType) const;
protected:
    std::string m_connectionName;
    LSqlError m_error;
    LSqlPrecisionPolicy m_precisionPolicy = LSqlPrecisionPolicy::LowDouble;
    DbmsType m_dbmsType = UnknownDbms;
    bool m_isOpen = false;
    bool m_isOpenError = false;
};


LK_NAMESPACE_END

#endif //LSQLDRIVER_H
