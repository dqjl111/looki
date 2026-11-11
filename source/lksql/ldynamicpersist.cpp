#include "ldynamicpersist.h"
#include <algorithm>

LK_NAMESPACE_BEGIN

LDynamicPersist::LDynamicPersist()
{
    persistIdPtr = nullptr;
}

LDynamicPersist::LDynamicPersist(const std::string &_persistTableName)
{
    persistIdPtr = nullptr;
    persistTableName = _persistTableName;
}

LDynamicPersist &LDynamicPersist::getRef()
{
    return *this;
}

std::string LDynamicPersist::getHashKey()
{
    if(persistIdPtr != nullptr){
        return getTableName() + ":" +  persistIdPtr->toEquationString();
    }else{
        return getTableName();
    }
}

std::shared_ptr<LIPersist> LDynamicPersist::createPersist()
{
    std::shared_ptr<LDynamicPersist> tempPtr = std::make_shared<LDynamicPersist>(persistTableName);
    if(persistIdPtr != nullptr){
        std::shared_ptr<LINamedVar> var = persistIdPtr->newVar();
        tempPtr->setPrimaryNamedVar(var);
    }

    for(LINamedVar* var : varPtrs){
        std::shared_ptr<LINamedVar> newVar = var->newVar();
        tempPtr->addField(newVar);
    }
    std::shared_ptr<LIPersist> retPtr = tempPtr;
    return retPtr;
}

bool LDynamicPersist::addField(std::shared_ptr<LINamedVar> namedVar)
{
    if(namedVar != nullptr){
        auto it =  varMap.find(namedVar->getName());
        if (it == varMap.end()) {
            varMap[namedVar->getName()] = namedVar;
            varPtrs.push_back(namedVar.get());
            return true;
        }
    }
    return false;
}

void LDynamicPersist::removeField(const std::string& varName)
{
    auto it =  varMap.find(varName);
    if(it != varMap.end()){
        std::shared_ptr<LINamedVar> namedVar = it->second;
        varMap.erase(it);
        if(persistIdPtr != nullptr){
            if(persistIdPtr->getName() == varName){
                persistIdPtr = nullptr;
            }
        }

        varPtrs.erase(
            std::remove_if(varPtrs.begin(), varPtrs.end(),
                           [namedVar](LINamedVar * n) { return n== namedVar.get(); }

                           ),
            varPtrs.end()
            );
    }
}


void LDynamicPersist::setPrimaryNamedVar(std::shared_ptr<LINamedVar> namedVar)
{
    if(persistIdPtr != namedVar.get()){
        if(persistIdPtr != nullptr){
            auto it =  varMap.find(persistIdPtr->getName());
            if(it != varMap.end()){
                varMap.erase(it);
            }
        }

        if(namedVar != nullptr){
            varMap[namedVar->getName()] = namedVar;
        }
        persistIdPtr = namedVar.get();
    }
}

const std::string &LDynamicPersist::getTableName()
{
    return  persistTableName;
}

void LDynamicPersist::setTableName(const std::string &name)
{
    persistTableName = name;
}

LK_NAMESPACE_END
