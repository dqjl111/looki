#ifndef LSQLRESULT_H
#define LSQLRESULT_H

#include "lksql/lsqlrecord.h"
#include "lksql/lsqldriver.h"
#include "lkcore/io/lnamedvar.h"

LK_NAMESPACE_BEGIN

class LKSQLDEF LSqlResult  : public LIVar
{
public:
    LSqlResult(std::shared_ptr<LSqlDriver> driver);

    enum BindingSyntax {
        PositionalBinding,
        NamedBinding
    };

    const virtual LVariant& data(int col) const = 0;
    virtual bool isNull(int col) const = 0;
    virtual bool fetch(int row) = 0;
    virtual bool fetchNext();
    virtual bool fetchPrevious();
    virtual bool fetchFirst() = 0;
    virtual bool fetchLast() = 0;

    virtual bool reset(const std::string& sqlquery) = 0;
    virtual int size() = 0;
    virtual int numRowsAffected() = 0;
    virtual std::shared_ptr<LSqlRecord> record() const = 0;
    virtual bool exec();

    virtual void setRow(int row);
    virtual void setActive(bool a);

    virtual bool prepare(const std::string& query);
    virtual bool savePrepare(const std::string& query);
    virtual void detachFromResultSet();
    virtual void setPrecisionPolicy(LSqlPrecisionPolicy policy);
    virtual void setQuery(const std::string& query);
    virtual LVariant lastInsertId() const;
    virtual void setForwardOnly(bool forward);


    void bindValue(const std::string& placeholder, const LVariant& val, LFlags<LSqlIoFlag> type = LSqlIoFlag::In);
    void bindValue(int index, const LVariant& val, LFlags<LSqlIoFlag> type = LSqlIoFlag::In);


    void addBindValue(const LVariant& val, LFlags<LSqlIoFlag> type = LSqlIoFlag::In);
    const LVariant& boundValue(const std::string& placeholder) const;
    const LVariant& boundValue(int index) const;
    inline const std::list<LVariant>& boundValues() const{
        return m_boundValues;
    }

    inline bool isActive() const{
        return m_active;
    }
    inline bool isSelect() const{
        return m_isSel;
    }
    inline bool isForwardOnly() const{
        return m_forwardOnly;
    }

    inline int row() const{
        return m_row;
    }

    inline LSqlPrecisionPolicy precisionPolicy() const{
        return m_precisionPolicy;
    }

    bool isValid() const;

    void clear();
    void resetBindCount();
    void setPositionalBindingEnabled(bool enable){
        m_positionalBindingEnabled = enable;
    }
    inline bool isPositionalBindingEnabled() const{
        return m_positionalBindingEnabled;
    }

    inline const std::string& executedQuery() const{
        return m_executedQuery;
    }
    std::list<std::string> boundValueNames() const;
    std::string boundValueName(int index) const;
    bool hasOutValues() const;

    inline const std::string& lastQuery() const{
        return m_sql;
    }

    inline const LSqlError& lastError() const{
        return m_error;
    }

    inline void clearLastError(){
        m_error.clear();
    }

    inline std::shared_ptr<LSqlDriver> driver() const{
        return m_driver;
    }
protected:
    std::string namedToPositionalBinding(const std::string &query);
    std::string positionalToNamedBinding(const std::string &query) const;
    void clearValues();
    void clearIndex();

    std::shared_ptr<LSqlDriver> m_driver;
    std::string m_sql;
    std::string m_executedQuery;
    LSqlError m_error;
    bool m_forwardOnly = false;
    bool m_positionalBindingEnabled = true;

    BindingSyntax m_binds = PositionalBinding;
    std::unordered_map<int, LFlags<LSqlIoFlag> > m_types;
    std::list<LVariant> m_boundValues;
    using IndexMap = std::unordered_map<std::string, std::list<int>>;
    IndexMap m_indexes;

    int m_row = (int)LSqlLocation::BeforeFirstRow;
    int m_bindCount = 0;
    bool m_active = false;
    bool m_isSel = false;
    LSqlPrecisionPolicy m_precisionPolicy = LSqlPrecisionPolicy::LowDouble;

    using Holder = LNamedVar<uint32_t, LIDynNamedVar>;
    std::list<Holder> m_holders;
};


LK_NAMESPACE_END
#endif // LSQLRESULT_H
