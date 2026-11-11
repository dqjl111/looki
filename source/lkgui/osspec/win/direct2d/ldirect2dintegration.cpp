#include "ldirect2dintegration.h"
#include "ldirect2dwin.h"
#include "ldirect2dpixmap.h"
#include "ldirect2dbackingstore.h"

LK_NAMESPACE_BEGIN

LDirect2DIntegration::LDirect2DIntegration()
{
    m_d2dContext.init();
}

LDirect2DIntegration *LDirect2DIntegration::instance()
{
    return static_cast<LDirect2DIntegration *>(m_instance);
}

std::shared_ptr<LPlatformPixmap> LDirect2DIntegration::createPlatformPixmap() const
{
    return std::make_shared<LDirect2DPixmap>();
}

std::shared_ptr<LPlatformBackingStore> LDirect2DIntegration::createPlatformBackingStore(std::shared_ptr<LWindow> window) const
{
    return std::make_shared<LDirect2DBackingStore>();
}

std::shared_ptr<LWindowsWin> LDirect2DIntegration::createPlatformWindowHelper(std::shared_ptr<LWindow> window, const LWindowsWindowData &data) const
{
    return std::make_shared<LDirect2DWin>(window, data);
}


LK_NAMESPACE_END
