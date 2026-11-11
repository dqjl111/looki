#ifndef LOBJECTDATA_H
#define LOBJECTDATA_H

#include <atomic>
#include <memory>
#include <string>
#include <list>
#include "lkcore/lookiconfig.h"

LK_NAMESPACE_BEGIN

class LObject;
class LIThread;
class LIThreadData;
class LKCOREDEF LObjectData {
public:
    LObjectData();
    struct ExtraData
    {
        ExtraData() {}
        std::list<std::weak_ptr<LObject> > eventFilters;
        std::string objectName;
    };

    std::weak_ptr<LIThreadData> m_threadData;
    std::atomic<LObject*> m_Obj;
    std::shared_ptr<ExtraData> m_extraData = nullptr;

    LIThread* thread();
    std::shared_ptr<LIThreadData> threadData();
};

LK_NAMESPACE_END

#endif // LOBJECTDATA_H
