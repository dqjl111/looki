#ifndef EDEVICEUSER_H
#define EDEVICEUSER_H

#include "lksql/lipersist.h"

class EDeviceUser : public _LK::LIPersist
{
public:
    EDeviceUser();
    virtual ~EDeviceUser();
    virtual EDeviceUser& getRef() override;
    virtual std::string getHashKey() override;
    virtual std::shared_ptr<_LK::LIPersist> createPersist() override;
    DECLARE_REFVAR(std::string, device_id)
    DECLARE_REFNOEMPTY(std::string, user_code)
    DECLARE_REFNOEMPTY(std::string, password)
    DECLARE_REFVAR(uint8_t, auto_login)

    virtual const std::string& getTableName() override;
};

#endif
