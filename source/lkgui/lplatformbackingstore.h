#ifndef LPLATFORMBACKINGSTORE_H
#define LPLATFORMBACKINGSTORE_H

#include "lkgui/lwindow.h"

LK_NAMESPACE_BEGIN
class LBackingStore;
class LKGUIDEF LPlatformBackingStore
{
public:
    LPlatformBackingStore();
    inline std::shared_ptr<LBackingStore> backingStore(){
        return m_backingStore.lock();
    }
    inline void setBackingStore(std::shared_ptr<LBackingStore>  store){
        m_backingStore = store;
    }

    virtual void resize(const LSize<int> &size, const std::shared_ptr<LRegion> staticContents) = 0;
    virtual void flush(std::shared_ptr<LWindow> window,
                       const std::shared_ptr<LRegion> region,
                       const LPoint<int> &offset) = 0;

    virtual std::shared_ptr<LPaintDevice> paintDevice() = 0;
    virtual void beginPaint(const std::shared_ptr<LRegion> );
    virtual void endPaint();

    std::shared_ptr<LWindow> window();

    virtual bool scroll(const std::shared_ptr<LRegion> area, int dx, int dy);

protected:
    std::weak_ptr<LBackingStore> m_backingStore;
};

LK_NAMESPACE_END

#endif // LPLATFORMBACKINGSTORE_H
