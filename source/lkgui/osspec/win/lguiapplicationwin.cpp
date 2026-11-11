#include "lkgui/lguiapplication.h"
#include "lkgui/osspec/win/direct2d/ldirect2dintegration.h"
LK_NAMESPACE_BEGIN

LPlatformIntegration* LGuiApplication::platformIntegration()
{
    if(LWindowsIntegration::instance() == nullptr){
        static LDirect2DIntegration t;
        //static LWindowsIntegration t;
    }
    return LWindowsIntegration::instance();
}

LK_NAMESPACE_END
