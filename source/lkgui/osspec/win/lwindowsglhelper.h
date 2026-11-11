#ifndef LWINDOWSGLHELPER_H
#define LWINDOWSGLHELPER_H
#include "lwindows.h"
#include "lkcore/lvariant.h"

LK_NAMESPACE_BEGIN

class LGpuDescription
{

public:
    LGpuDescription();

    static std::shared_ptr<LGpuDescription> detect();
    static std::vector<std::shared_ptr<LGpuDescription> > detectAll();

    uint32_t m_vendorId = 0;
    uint32_t m_deviceId = 0;
    uint32_t m_revision = 0;
    uint32_t m_subSysId = 0;
    std::vector<uint32_t> m_driverVersion;
    std::string m_driverName;
    std::string m_description;
    std::string m_gpuSuitableScreen;
};



//class LWindowsGLHelper
//{
//public:
//    LWindowsGLHelper();
//};

LK_NAMESPACE_END
#endif // LWINDOWSGLHELPER_H
