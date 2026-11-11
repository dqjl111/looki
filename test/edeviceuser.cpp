#include "edeviceuser.h"

namespace  ccon{
const static std::string TABLE_E_DEVICE_USER("E_DEVICE_USER");

const static std::string DEVICE_ID("DEVICE_ID");
const static std::string USER_CODE("USER_CODE");
const static std::string PASSWORD("PASSWORD");
const static std::string AUTO_LOGIN("AUTO_LOGIN");

}


EDeviceUser::EDeviceUser():
BIND_VAR(device_id, ccon::DEVICE_ID),
BIND_VAR(user_code, ccon::USER_CODE),
BIND_VAR(password, ccon::PASSWORD),
BIND_VAR(auto_login, ccon::AUTO_LOGIN)
{
    persistIdPtr = &_device_id;
    varPtrs.push_back(&_user_code);
    varPtrs.push_back(&_password);
    varPtrs.push_back(&_auto_login);

    reSetData();
}

EDeviceUser::~EDeviceUser()
{
}

EDeviceUser &EDeviceUser::getRef()
{
    return *this;
}

const std::string &EDeviceUser::getTableName()
{
    return ccon::TABLE_E_DEVICE_USER;
}

std::string EDeviceUser::getHashKey()
{
    return getTableName() + ":" +  persistIdPtr->toEquationString();
}

std::shared_ptr<_LK::LIPersist> EDeviceUser::createPersist()
{
    return std::make_shared<EDeviceUser>();
}
