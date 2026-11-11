#ifndef LDYNAMICPERSIST_H
#define LDYNAMICPERSIST_H

#include "lksql/lipersist.h"

LK_NAMESPACE_BEGIN

class LKSQLDEF LDynamicPersist : public LIPersist
{
public:
    LDynamicPersist();
    LDynamicPersist(const std::string& _persistTableName);

    virtual LDynamicPersist& getRef() override;
    virtual std::string getHashKey() override;
    virtual std::shared_ptr<LIPersist> createPersist() override;


    bool addField(std::shared_ptr<LINamedVar> namedVar);
    void removeField(const std::string& varName);

    void setPrimaryNamedVar(std::shared_ptr<LINamedVar> namedVar);

    virtual const std::string& getTableName() override;
    void setTableName(const std::string& name);

protected:
    std::unordered_map<std::string, std::shared_ptr<LINamedVar> > varMap;
    std::string persistTableName;
};


LK_NAMESPACE_END
#endif // DYNAMICPERSIST_H
