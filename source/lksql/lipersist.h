#ifndef LIPERSIST_H
#define LIPERSIST_H

#include <concepts>
#include <memory>
#include "lkcore/io/lnamedvar.h"
#include "lkcore/lvariant.h"
#include "lksql/lsqlquery.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN

class LIPersist;

template<typename T>
concept TPersist = std::derived_from<T, LIPersist> || std::same_as<T, LIPersist>;

class LKSQLDEF LIPersist : public LIVar
{
public:
    LIPersist();
    LIPersist(LIPersist& persist);

    virtual LIPersist& getRef();

    virtual bool saveData(std::shared_ptr<LSqlDatabase> db);//存储实时数据
    virtual bool flushData(std::shared_ptr<LSqlDatabase> db);  //存储实时数据
    virtual void reSetData();  //clear data to empty

    virtual bool insertToDB(std::shared_ptr<LSqlDatabase> db);
    virtual bool getInsertSQL(std::string& sql, bool usePos = true);
    virtual bool updateToDB(std::shared_ptr<LSqlDatabase> db, bool allowInsert = true);
    virtual bool updateToDB(const LINamedVar& var, std::shared_ptr<LSqlDatabase> db, bool allowInsert = true);
    virtual bool getUpdateSQL(const LINamedVar& var, std::string& sql);

    virtual bool updateToDB(const std::vector<LINamedVar*>& conditonPtrs, std::shared_ptr<LSqlDatabase> db, bool allowInsert = true);
    virtual bool getUpdateSQL(const std::vector<LINamedVar*>& conditonPtrs, std::string& sql);

    virtual bool deleteFromDB(std::shared_ptr<LSqlDatabase> db, const LINamedVar& var);
    virtual bool deleteFromDB(std::shared_ptr<LSqlDatabase> db, const std::vector<LINamedVar*>& varPtrs);
    virtual bool deleteFromDB(std::shared_ptr<LSqlDatabase> db, std::string& condition);
    /**
     * @brief selectFromDB
     * @param query result of selection
     * @param db
     * @param varPtrs And conditons
     * @return
     */
    virtual LSqlQuery selectFromDB(std::shared_ptr<LSqlDatabase> db, const std::vector<LINamedVar*>& varPtrs);
    virtual LSqlQuery selectFromDB(std::shared_ptr<LSqlDatabase> db, const LINamedVar& var);
    virtual std::string getSelectSQL(const std::vector<LINamedVar*>& varPtrs, const std::vector<bool>& isEqualConditions = std::vector<bool>());
    virtual std::string getSelectSQL(const LINamedVar& var, bool isEqualCondition = true);
    virtual std::string getSelectSQL();

    /**
     * Use with selectFromDB() to fetch data
     * @brief fectchByQueryFirst
     * @param query
     */
    virtual bool fectchByQueryFirst(LSqlQuery& query);
    virtual bool fectchByQuery(LSqlQuery& query, uint32_t pos);
    virtual void fectchByQuery(LSqlQuery& query);

    virtual bool selectFirstFromDB(std::shared_ptr<LSqlDatabase> db, const std::vector<LINamedVar*>& varPtrs);
    virtual bool selectFirstFromDB(std::shared_ptr<LSqlDatabase> db, const LINamedVar& var);
    virtual bool selectFirstFromDB(std::shared_ptr<LSqlDatabase> db, const std::string& sql);

    virtual std::vector<std::shared_ptr<LIPersist>> selectAllFromDB(std::shared_ptr<LSqlDatabase> db, const std::vector<LINamedVar*>& varPtrs, const std::vector<bool>& isEqualConditions = std::vector<bool>());
    virtual std::vector<std::shared_ptr<LIPersist>> selectAllFromDB(std::shared_ptr<LSqlDatabase> db, const LINamedVar& var, bool isEqualCondition = true);
    virtual std::vector<std::shared_ptr<LIPersist>> selectAllFromDB(std::shared_ptr<LSqlDatabase> db, const std::string& sql);

    template<TPersist T>
    std::vector<std::shared_ptr<T>> tlSelectAllFromDB(std::shared_ptr<LSqlDatabase> db, const std::vector<LINamedVar*>& varPtrs, const std::vector<bool>& isEqualConditions = std::vector<bool>());
    template<TPersist T>
    std::vector<std::shared_ptr<T>> tlSelectAllFromDB(std::shared_ptr<LSqlDatabase> db, const LINamedVar& var, bool isEqualCondition = true);
    template<TPersist T>
    std::vector<std::shared_ptr<T>> tlSelectAllFromDB(std::shared_ptr<LSqlDatabase> db, const std::string& sql);

    virtual bool fectchKeyByVar(const LINamedVar& var, std::shared_ptr<LSqlDatabase> db);
    virtual bool fetchKeyByVar(const std::vector<LINamedVar*>& conditonPtrs, std::shared_ptr<LSqlDatabase> db);
    bool isRaw();

    static bool getVarCondition(std::string& condition, const std::vector<LINamedVar*>& conditonPtrs, const std::vector<bool>& isEqualConditions = std::vector<bool>());

    virtual void clone(LIPersist& persist);
    LIPersist& operator=(LIPersist& persist);
    virtual std::shared_ptr<LIPersist> getClone();
    virtual void merge(LIPersist& persist);

    virtual std::string getHashKey() =0;
    virtual std::shared_ptr<LIPersist> createPersist() = 0;

    virtual std::unordered_map<std::string, LINamedVar*> getVarMap();
    virtual std::unordered_map<std::string, LINamedVar*> getValidVarMap();

    virtual LINamedVar* getKeyVar();

    virtual LINamedVar* getVarByName(const std::string& varName);
    virtual LINamedVar* getVarByVarMap(const std::string& varKey);

    virtual void setPrimaryValue(const LVariant& value);
    virtual void setVarByName(const std::string& varName, const LVariant& value);
    virtual const std::string& getTableName() = 0;
protected:
    std::vector<LINamedVar*> varPtrs;
    LINamedVar* persistIdPtr;
};



template<TPersist T>
std::vector<std::shared_ptr<T>> LIPersist::tlSelectAllFromDB(std::shared_ptr<LSqlDatabase> db, const std::vector<LINamedVar*>& varPtrs,  const std::vector<bool>& isEqualConditions)
{
    std::string sql = getSelectSQL(varPtrs, isEqualConditions);
    return tlSelectAllFromDB<T>(db, sql);
}

template<TPersist T>
std::vector<std::shared_ptr<T>> LIPersist::tlSelectAllFromDB(std::shared_ptr<LSqlDatabase> db, const LINamedVar& var, bool isEqualCondition)
{
    std::string sql = getSelectSQL(var, isEqualCondition);
    return tlSelectAllFromDB<T>(db, sql);
}

template<TPersist T>
std::vector<std::shared_ptr<T>> LIPersist::tlSelectAllFromDB(std::shared_ptr<LSqlDatabase> db, const std::string& sql)
{
    std::vector<std::shared_ptr<T>> ret;
    LSqlQuery query(db);
    query.prepare(sql);
    if(query.exec())
    {
        while(query.next())
        {
            std::shared_ptr<LIPersist> d = createPersist();
            d->fectchByQuery(query);
            ret.push_back(std::dynamic_pointer_cast<T>(d));
        }
    }
    else
    {
        lWarn() << "Select db data for failed!" << query.lastQuery();
    }
    return ret;
}


template<TPersist T>
std::vector<std::shared_ptr<T>> tVec(const std::vector<std::shared_ptr<LIPersist>>& orgVec){
    std::vector<std::shared_ptr<T>> ret;
    for(const std::shared_ptr<LIPersist>& a: orgVec){
        std::shared_ptr<T> t= std::dynamic_pointer_cast<T>(a);
        ret.push_back(t);
    }
    return ret;
}

template<TPersist T>
std::vector<std::shared_ptr<T>> tVec(const std::list<std::shared_ptr<LIPersist>>& orgVec){
    std::vector<std::shared_ptr<T>> ret;
    for(const std::shared_ptr<LIPersist>& a: orgVec){
        std::shared_ptr<T> t= std::dynamic_pointer_cast<T>(a);
        ret.push_back(t);
    }
    return ret;
}

template<TPersist T>
std::list<std::shared_ptr<T>> tList(const std::vector<std::shared_ptr<LIPersist>>& orgVec){
    std::list<std::shared_ptr<T>> ret;
    for(const std::shared_ptr<LIPersist>& a: orgVec){
        std::shared_ptr<T> t= std::dynamic_pointer_cast<T>(a);
        ret.push_back(t);
    }
    return ret;
}

template<TPersist T>
std::list<std::shared_ptr<T>> tList(const std::list<std::shared_ptr<LIPersist>>& orgVec){
    std::list<std::shared_ptr<T>> ret;
    for(const std::shared_ptr<LIPersist>& a: orgVec){
        std::shared_ptr<T> t= std::dynamic_pointer_cast<T>(a);
        ret.push_back(t);
    }
    return ret;
}

template<TPersist T>
std::vector<std::shared_ptr<T>> tVec(std::vector<std::shared_ptr<LIPersist>>&& orgVec){
    std::vector<std::shared_ptr<T>> ret;
    for(const std::shared_ptr<LIPersist>& a: orgVec){
        std::shared_ptr<T> t= std::dynamic_pointer_cast<T>(a);
        ret.push_back(t);
    }
    return ret;
}

template<TPersist T>
std::vector<std::shared_ptr<T>> tVec(std::list<std::shared_ptr<LIPersist>>&& orgVec){
    std::vector<std::shared_ptr<T>> ret;
    for(const std::shared_ptr<LIPersist>& a: orgVec){
        std::shared_ptr<T> t= std::dynamic_pointer_cast<T>(a);
        ret.push_back(t);
    }
    return ret;
}

template<TPersist T>
std::list<std::shared_ptr<T>> tList(std::vector<std::shared_ptr<LIPersist>>&& orgVec){
    std::list<std::shared_ptr<TPersist>> ret;
    for(const std::shared_ptr<LIPersist>& a: orgVec){
        std::shared_ptr<T> t= std::dynamic_pointer_cast<T>(a);
        ret.push_back(t);
    }
    return ret;
}

template<TPersist T>
std::list<std::shared_ptr<T>> tList(std::list<std::shared_ptr<LIPersist>>&& orgVec){
    std::list<std::shared_ptr<T>> ret;
    for(const std::shared_ptr<LIPersist>& a: orgVec){
        std::shared_ptr<T> t= std::dynamic_pointer_cast<T>(a);
        ret.push_back(t);
    }
    return ret;
}


template<class TKey, TPersist T, TPersist P = LIPersist>
std::shared_ptr<T> getPersistInMap(const TKey& key, std::unordered_map<TKey, std::shared_ptr<P>>& peristMap)
{
    std::shared_ptr<T> persistPtr = nullptr;

    typename std::unordered_map<TKey, std::shared_ptr<P>>::iterator iter = peristMap.find(key);
    if(iter != peristMap.end())
    {
        persistPtr = std::dynamic_pointer_cast<T>(iter->second);
    }
    else
    {
        persistPtr =  std::make_shared<T>();
        peristMap[key] = persistPtr;
    }

    return persistPtr;
}


template<typename TKey, TPersist T, TPersist P = LIPersist>
std::shared_ptr<T> getPersistInMap(const TKey& key, std::map<TKey, std::shared_ptr<P>>& peristMap)
{
    std::shared_ptr<T> persistPtr = nullptr;

    typename std::map<TKey, std::shared_ptr<P>>::iterator iter = peristMap.find(key);
    if(iter != peristMap.end())
    {
        persistPtr = std::dynamic_pointer_cast<T>(iter->second);
    }
    else
    {
        persistPtr =  std::make_shared<T>();
        peristMap[key] = persistPtr;
    }

    return persistPtr;
}

LK_NAMESPACE_END

#endif // IPERSISTDEVCE_H
