#ifndef LDIRECT2DBACKINGSTORE_H
#define LDIRECT2DBACKINGSTORE_H
#include "lkgui/lplatformbackingstore.h"


LK_NAMESPACE_BEGIN
class LDirect2DBackingStore : public LPlatformBackingStore
{
public:
    LDirect2DBackingStore();

    void beginPaint(const std::shared_ptr<LRegion> ) override;
    void endPaint() override;

    virtual std::shared_ptr<LPaintDevice> paintDevice() override;
    virtual void flush(std::shared_ptr<LWindow> window,
                       const std::shared_ptr<LRegion> region,
                       const LPoint<int> &offset) override;

    void resize(const LSize<int> &size, const std::shared_ptr<LRegion> region) override;
};

LK_NAMESPACE_END
#endif // LDIRECT2DBACKINGSTORE_H
