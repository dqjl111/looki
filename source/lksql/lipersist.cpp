#include "lipersist.h"

LK_NAMESPACE_BEGIN

LIPersist::LIPersist()
{
    persistIdPtr = nullptr;
}

LIPersist::LIPersist(LIPersist &persist)
{
    LIPersist::clone(persist);
}


LIPersist& LIPersist::getRef()
{
    return *this;
}

bool LIPersist::saveData(std::shared_ptr<LSqlDatabase> db)
{
    bool ret = false;
    if(!isRaw())
    {
        ret = updateToDB(db);
    }
    else
    {
         ret = insertToDB(db);
    }
    return ret;
}


bool LIPersist::flushData(std::shared_ptr<LSqlDatabase> db)
{
    bool ret = false;
    if(db->transaction()) // 启动事务操作
    {
        ret = saveData(db);

        if(ret && !db->commit())
        {
            lInfo() << db->lastError().toString(); // 提交
             if(!db->rollback())
             {
                  lInfo() << db->lastError().toString(); // 回滚
             }
        }
    }
    return ret;
}

void LIPersist::reSetData()
{
    for(LINamedVar* varPtr : varPtrs)
    {
        varPtr->reSet();
    }
    if(persistIdPtr != nullptr)
    {
        persistIdPtr->reSet();
    }
}

bool LIPersist::insertToDB(std::shared_ptr<LSqlDatabase> db)
{
    bool insertSuccess= false;
    std::string sql;
    if(getInsertSQL(sql, true))
    {
        LSqlQuery query(db);
        query.prepare(sql);
        int validVarNo =0;
        for(LINamedVar* varPtr : varPtrs)
        {
            if(!varPtr->isEmpty())
            {
                query.bindValue(validVarNo, varPtr->getVariant());
                validVarNo ++;
            }
        }

        if(!isRaw())
        {
            query.bindValue(validVarNo, persistIdPtr->getVariant());
            validVarNo ++;
        }

        insertSuccess = query.exec();
        if(!insertSuccess)
        {
            lInfo() <<"Insert data failed!" << query.lastError().toString() << ": " << query.lastQuery();
        }

    }
    return insertSuccess;
}

bool LIPersist::getInsertSQL(std::string &sql, bool usePos)
{    
    std::ostringstream sqlPart0;
    std::ostringstream sqlPart1;
    sqlPart0 << "insert into " << getTableName() << "(";
    sqlPart1 << ") values (";
    bool hasValidData = false;

    for(LINamedVar* varPtr: varPtrs)
    {
        if(!varPtr->isEmpty())
        {
            if(hasValidData)
            {
                sqlPart0 <<  ","  << varPtr->getName();
                if(usePos){
                    sqlPart1 << ",?";
                }else{
                    sqlPart1 << ",:" << varPtr->getName();
                }
            }
            else
            {
                sqlPart0 << varPtr->getName();
                if(usePos){
                    sqlPart1 << "?";
                }else{
                    sqlPart1 << ":" << varPtr->getName();
                }
                hasValidData = true;
            }
        }
    }

    if(!isRaw())
    {
        if(hasValidData)
        {
            sqlPart0 << ","  << persistIdPtr->getName();
            if(usePos){
                sqlPart1 << ",?";
            }else{
                sqlPart1 << ",:" << persistIdPtr->getName();
            }
        }
        else
        {
            sqlPart0 << persistIdPtr->getName();
            if(usePos){
                sqlPart1 << "?";
            }else{
                sqlPart1 << ":" << persistIdPtr->getName();
            }

            hasValidData = true;
        }
    }

    if(hasValidData)
    {
        sql = sqlPart0.str()+sqlPart1.str() +")";
    }
    else
    {
        sql = "";
    }
    return hasValidData;
}

bool LIPersist::updateToDB(std::shared_ptr<LSqlDatabase> db,  bool allowInsert)
{
    if(isRaw())
    {
        lWarn() <<"Invalid persist id for raw data.";
    }
    else
    {
        return updateToDB(*persistIdPtr, db, allowInsert);
    }
    return false;
}

bool LIPersist::updateToDB(const LINamedVar &var, std::shared_ptr<LSqlDatabase> db, bool allowInsert)
{
    bool updateSuccess= false;
    if(!var.isEmpty())
    {
        std::string sql;
        if(getUpdateSQL(var, sql))
        {
            LSqlQuery query(db);
            query.prepare(sql);
            int validVarNo =0;
            for(LINamedVar* varPtr : varPtrs)
            {
                if(!varPtr->isEmpty())
                {
                    query.bindValue(validVarNo, varPtr->getVariant());
                    validVarNo ++;
                }
            }
            query.bindValue(validVarNo, var.getVariant());
            updateSuccess = query.exec();
            int affectedRowNo = query.numRowsAffected();
            if(updateSuccess && (affectedRowNo>0))
            {
            }
            else if(affectedRowNo == 0 && allowInsert)
            {
                updateSuccess = insertToDB(db);
            }
            else if(!updateSuccess)
            {
                lInfo() <<"Update data failed!" << query.lastError().toString() << ": "<< query.lastQuery();
            }
        }else if(allowInsert){
            if((persistIdPtr != nullptr)  && persistIdPtr->getName() == var.getName()){
                updateSuccess = insertToDB(db);
            }
        }
    }
    else
    {
        updateSuccess= false;
        lWarn() <<"Invalid persist var for raw data.";
    }
    return updateSuccess;
}

bool LIPersist::getUpdateSQL(const LINamedVar &var, std::string &sql)
{
    bool hasValidData = false;

    sql = "update ";
    sql += getTableName() + " set ";

    for(LINamedVar* varPtr : varPtrs)
    {
        if(!varPtr->isEmpty())
        {
            if(hasValidData)
            {
                sql += ","  + varPtr->getName() +"=?";
            }
            else
            {
                sql += varPtr->getName() + "=?";
                hasValidData = true;
            }
        }
    }

    if(hasValidData)
    {
        sql += " where "+ var.getName() + "=?";
    }
    else
    {
        sql = "";
    }

    return hasValidData;
}

bool LIPersist::updateToDB(const std::vector<LINamedVar *> &conditionPtrs, std::shared_ptr<LSqlDatabase> db, bool allowInsert)
{
    bool updateSuccess= true;

    std::string sql;
    if(getUpdateSQL(conditionPtrs, sql))
    {
        LSqlQuery query(db);
        query.prepare(sql);
        int validVarNo =0;
        for(LINamedVar* varPtr : varPtrs)
        {
            if(!varPtr->isEmpty())
            {
                query.bindValue(validVarNo, varPtr->getVariant());
                validVarNo ++;
            }
        }
        updateSuccess = query.exec();
        int affectedRowNo = query.numRowsAffected();
        if(updateSuccess && (affectedRowNo>0))
        {
        }
        else if(affectedRowNo == 0 && allowInsert)
        {
            updateSuccess = insertToDB(db);
        }
        else if(!updateSuccess)
        {
            updateSuccess= false;
            lInfo() <<"Update data failed!" << query.lastError().toString() << ": "<< query.lastQuery();
        }
    }

    return updateSuccess;
}

bool LIPersist::getUpdateSQL(const std::vector<LINamedVar *> &conditonPtrs, std::string &sql)
{
    bool hasValidData = false;

    sql = "update ";
    sql += getTableName() + " set ";

    for(LINamedVar* varPtr : varPtrs)
    {
        if(!varPtr->isEmpty())
        {
            if(hasValidData)
            {
                sql += ","  + varPtr->getName() +"=?";
            }
            else
            {
                sql += varPtr->getName() + "=?";
                hasValidData = true;
            }
        }
    }

    if(hasValidData)
    {
        std::string condition;
        if(LIPersist::getVarCondition(condition, conditonPtrs))
        {
            sql += " where "+ condition;
        }
    }
    else
    {
        sql = "";
    }

    return hasValidData;
}

bool LIPersist::deleteFromDB(std::shared_ptr<LSqlDatabase> db, const LINamedVar &var)
{
    bool ret = true;
    if(!var.isEmpty())
    {
        std::string condition = var.toEquationString();
        ret = deleteFromDB(db, condition);
    }
    return ret;
}

bool LIPersist::deleteFromDB(std::shared_ptr<LSqlDatabase> db, const std::vector<LINamedVar *>& varPtrs)
{
    bool ret = true;
    std::string condition;
    if(LIPersist::getVarCondition(condition, varPtrs))
    {
        ret = deleteFromDB(db, condition);
    }
    return ret;
}

bool LIPersist::deleteFromDB(std::shared_ptr<LSqlDatabase> db, std::string &condition)
{
    bool ret = true;
    std::string sql  = "delete from " + getTableName();
    if(!condition.empty())
    {
        sql = sql + " where "+ condition;
    }
    LSqlQuery query(db);
    query.prepare(sql);
    ret = query.exec();
    if(ret)
    {
        //qInfo() <<"Delete data success.";
    }
    else
    {
        lInfo() <<"Delete data failed!" << query.lastQuery();
    }
    return ret;
}

LSqlQuery LIPersist::selectFromDB(std::shared_ptr<LSqlDatabase> db, const std::vector<LINamedVar *>& varPtrs)
{
    return LSqlQuery(getSelectSQL(varPtrs), db);
}


LSqlQuery LIPersist::selectFromDB(std::shared_ptr<LSqlDatabase> db, const LINamedVar& var)
{
    return LSqlQuery(getSelectSQL(var), db);
}

std::string LIPersist::getSelectSQL(const std::vector<LINamedVar *>& varPtrs,  const std::vector<bool>& isEqualConditions)
{
    std::string condition;
    if(getVarCondition(condition, varPtrs, isEqualConditions))
    {
        return getSelectSQL() + " where " + condition;
    }
    else
    {
        return getSelectSQL();
    }
}

std::string LIPersist::getSelectSQL(const LINamedVar &var,  bool isEqualCondition)
{
    if(isEqualCondition)
    {
        return getSelectSQL() + " where " + var.toEquationString();
    }
    else
    {
        return getSelectSQL() + " where " + var.toLikeString();
    }
}


std::string LIPersist::getSelectSQL()
{
    return "select * from " + getTableName();
}

bool LIPersist::fectchKeyByVar(const LINamedVar &var, std::shared_ptr<LSqlDatabase> db)
{
    bool ret = false;
    if(var.isEmpty() ||  persistIdPtr == nullptr)
    {
        //Do nothing
    }
    else
    {
        std::string sql("select "
                    + persistIdPtr->getName()
                    + " from "
                    + getTableName()
                    + " where "
                    + var.toEquationString()
                    + " limit 1");

        LSqlQuery query(sql, db);
        if(query.first())
        {
            persistIdPtr->setVariant(query.value(persistIdPtr->getName()));
            ret = true;
        }

    }
    return ret;
}

bool LIPersist::fetchKeyByVar(const std::vector<LINamedVar *>& conditonPtrs, std::shared_ptr<LSqlDatabase> db)
{
    bool ret = false;
    if(persistIdPtr != nullptr)
    {
        std::string sql("select "
                    + persistIdPtr->getName()
                    + " from "
                    + getTableName()
                    + " where ");
        std::string condition;
        if(LIPersist::getVarCondition(condition, conditonPtrs))
        {
            sql += condition;
            sql += " limit 1";

            LSqlQuery query(sql, db);
            if(query.first())
            {
                persistIdPtr->setVariant(query.value(persistIdPtr->getName()));
                ret = true;
            }

        }
    }

    return ret;
}

bool LIPersist::fectchByQueryFirst(LSqlQuery& query)
{
    bool ret =true;
    if(query.first())
    {
        fectchByQuery(query);
    }
    else
    {
        ret =false;
        //qInfo() << "No valid data fetched!";
    }
    return ret;
}

bool LIPersist::fectchByQuery(LSqlQuery &query, uint32_t pos)
{
    bool ret = true;
    if(query.seek(pos))
    {
        fectchByQuery(query);
    }
    else
    {
        ret = false;
        lInfo() << "No valid data fetched!";
    }
    return ret;
}

void LIPersist::fectchByQuery(LSqlQuery &query)
{
    std::shared_ptr<LSqlRecord> rec = query.record();
    if(rec != nullptr){
        if(persistIdPtr != nullptr)
        {
            int nameCol = rec->indexOf(persistIdPtr->getName()); // index of the field "name"
            if(nameCol>=0)
            {
                persistIdPtr->setVariant(query.value(nameCol));
            }
        }

        for(LINamedVar* varPtr: varPtrs)
        {
            int nameCol = rec->indexOf(varPtr->getName()); // index of the field "name"
            if(nameCol>=0)
            {
                varPtr->setVariant(query.value(nameCol));
            }
        }
    }

}

bool LIPersist::selectFirstFromDB(std::shared_ptr<LSqlDatabase> db, const std::vector<LINamedVar *> &varPtrs)
{
    LSqlQuery query(db);
    query.prepare(getSelectSQL(varPtrs));
    if(query.exec())
    {
        return fectchByQueryFirst(query);
    }
    return false;
}

bool LIPersist::selectFirstFromDB(std::shared_ptr<LSqlDatabase> db, const LINamedVar &var)
{
    LSqlQuery query(db);
    query.prepare(getSelectSQL(var));
    if(query.exec())
    {
        return fectchByQueryFirst(query);
    }
    return false;
}

bool LIPersist::selectFirstFromDB(std::shared_ptr<LSqlDatabase> db, const std::string &sql)
{
    LSqlQuery query(db);
    query.prepare(sql);
    if(query.exec())
    {
        return fectchByQueryFirst(query);
    }
    return false;
}

std::vector<std::shared_ptr<LIPersist>> LIPersist::selectAllFromDB(std::shared_ptr<LSqlDatabase> db, const std::vector<LINamedVar*>& varPtrs,  const std::vector<bool>& isEqualConditions)
{
    std::string sql = getSelectSQL(varPtrs, isEqualConditions);
    return selectAllFromDB(db, sql);
}

std::vector<std::shared_ptr<LIPersist>> LIPersist::selectAllFromDB(std::shared_ptr<LSqlDatabase> db, const LINamedVar& var, bool isEqualCondition)
{
    std::string sql = getSelectSQL(var, isEqualCondition);
    return selectAllFromDB(db, sql);
}

std::vector<std::shared_ptr<LIPersist>> LIPersist::selectAllFromDB(std::shared_ptr<LSqlDatabase> db, const std::string& sql)
{
    std::vector<std::shared_ptr<LIPersist>> ret;
    LSqlQuery query(db);
    query.prepare(sql);
    if(query.exec())
    {
        while(query.next())
        {
            std::shared_ptr<LIPersist> d = createPersist();
            d->fectchByQuery(query);
            ret.push_back(d);
        }
    }
    else
    {
        lWarn() << "Select db data for failed!" << query.lastQuery();
    }
    return ret;
}

bool LIPersist::getVarCondition(std::string &condition, const std::vector<LINamedVar *>& conditonPtrs,  const std::vector<bool>& isEqualConditions)
{
    bool hasValue = false, isAllEqual = (conditonPtrs.size() != isEqualConditions.size());
    uint32_t size = (uint32_t)conditonPtrs.size();
    for(uint32_t i=0; i< size; ++i)
    {
        LINamedVar* varPtr = conditonPtrs[i];
        if(varPtr != nullptr)
        {
            if(hasValue)
            {
                if(isAllEqual || isEqualConditions[i])
                {
                    condition += " and " + varPtr->toEquationString();
                }
                else
                {
                    condition += " and " + varPtr->toLikeString();
                }
            }
            else
            {
                if(isAllEqual || isEqualConditions[i])
                {
                    condition += varPtr->toEquationString();
                }
                else
                {
                    condition += varPtr->toLikeString();
                }
                hasValue = true;
            }
        }
    }
    return hasValue;
}

void LIPersist::clone(LIPersist &persist)
{
    if(typeid(getRef()) == typeid(persist.getRef()))
    {
        /**
         * @For copy between same table
         */
        uint32_t nVar = (uint32_t)varPtrs.size();
        for(uint32_t i=0; i<nVar; ++i)
        {
            (*varPtrs[i]) = persist.varPtrs[i]->getVariant();
        }

        if(persistIdPtr != nullptr && persist.persistIdPtr !=nullptr )
        {
            (*persistIdPtr) =  persist.persistIdPtr->getVariant();
        }
    }
    else
    {
/**
 * @brief For copy between current table and history table, use it carefully.
 * you'd better check parent type to make sure allowed when coding. Pesist id
 * is not allowd copy for their are different table
 */
        uint32_t nVar = (uint32_t)varPtrs.size();
        if(nVar == (uint32_t)persist.varPtrs.size())
        {
            for(int i=0; i<nVar; ++i)
            {
                LINamedVar& var0 = *varPtrs[i];
                LINamedVar& var1 = *(persist.varPtrs[i]);
                if(var0.getName() == var1.getName())
                {
                    (*varPtrs[i]) = persist.varPtrs[i]->getVariant();
                }
            }
        }
    }
}

LIPersist &LIPersist::operator=(LIPersist &persist)
{
    clone(persist);
    return *this;
}

std::shared_ptr<LIPersist> LIPersist::getClone()
{
    std::shared_ptr<LIPersist> ret = createPersist();
    ret->clone(*this);
    return ret;
}

void LIPersist::merge(LIPersist &persist)
{
    if(typeid(getRef()) == typeid(persist.getRef()))
    {
        /**
         * @For merge data between same table
         */
        uint32_t nVar = (uint32_t)varPtrs.size();
        for(uint32_t i=0; i<nVar; ++i)
        {
            if(!persist.varPtrs[i]->isEmpty())
            {
                (*varPtrs[i]) = persist.varPtrs[i]->getVariant();
            }
        }
    }
}

bool LIPersist::isRaw()
{
    if(persistIdPtr == nullptr)
    {
        return true;
    }
    return persistIdPtr->isEmpty();
}

std::unordered_map<std::string, LINamedVar*> LIPersist::getVarMap()
{
    std::unordered_map<std::string, LINamedVar*> retMap;
    if(persistIdPtr != nullptr)
    {
        retMap[persistIdPtr->getName()] = persistIdPtr;
    }

    for(LINamedVar* var: varPtrs)
    {
        retMap[var->getName()]  = var;
    }

    return retMap;
}

std::unordered_map<std::string, LINamedVar *> LIPersist::getValidVarMap()
{
    std::unordered_map<std::string, LINamedVar *> ret;
    std::unordered_map<std::string, LINamedVar *> temp = getVarMap();
    std::unordered_map<std::string, LINamedVar *>::iterator iter = temp.begin();
    for(; iter != temp.end(); ++iter)
    {
        if(iter->second != nullptr && (!iter->second->isEmpty()))
        {
            ret[iter->first] = iter->second;
        }
    }
    return ret;
}

LINamedVar* LIPersist::getKeyVar()
{
    return persistIdPtr;
}

LINamedVar* LIPersist::getVarByName(const std::string& varName)
{
    if(persistIdPtr != nullptr)
    {
        if(persistIdPtr->getName() == varName)
        {
            return persistIdPtr;
        }
    }

    for(LINamedVar* var: varPtrs)
    {
        if(var->getName() == varName)
        {
           return var;
        }
    }

    return nullptr;
}

LINamedVar* LIPersist::getVarByVarMap(const std::string& varKey)
{
    std::unordered_map<std::string, LINamedVar*> varMap = getVarMap();
    std::unordered_map<std::string, LINamedVar*>::iterator iter = varMap.find(varKey);
    if(iter != varMap.end())
    {
        return iter->second;
    }
    return nullptr;
}

void LIPersist::setPrimaryValue(const LVariant& value)
{
    if(persistIdPtr != nullptr)
    {
        persistIdPtr->setVariant(value);
    }
}

void LIPersist::setVarByName(const std::string& varName, const LVariant& value)
{
    LINamedVar* var = getVarByName(varName);
    if(var != nullptr){
        var->setVariant(value);
    }
}


LK_NAMESPACE_END

