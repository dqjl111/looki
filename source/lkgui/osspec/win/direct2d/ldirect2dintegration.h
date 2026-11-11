#ifndef LDIRECT2DINTEGRATION_H
#define LDIRECT2DINTEGRATION_H
#include "lkgui/osspec/win/lwindowsintegration.h"
#include "ldirect2dcontext.h"

LK_NAMESPACE_BEGIN

class LDirect2DIntegration : public LWindowsIntegration
{
public:
    LDirect2DIntegration();

    static LDirect2DIntegration *instance();
    inline LDirect2DContext *direct2DContext() const{
        return &(const_cast<LDirect2DIntegration*>(this)->m_d2dContext);
    }

    virtual std::shared_ptr<LPlatformPixmap> createPlatformPixmap() const override;
    virtual std::shared_ptr<LPlatformBackingStore> createPlatformBackingStore(std::shared_ptr<LWindow> window) const override;

protected:
    LDirect2DContext m_d2dContext;
    virtual std::shared_ptr<LWindowsWin> createPlatformWindowHelper(std::shared_ptr<LWindow> window, const LWindowsWindowData &data) const override;
};


LK_NAMESPACE_END
#endif // LDIRECT2DINTEGRATION_H
