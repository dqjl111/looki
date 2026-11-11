#include "lplatformbackingstore.h"
#include "lbackingstore.h"

LK_NAMESPACE_BEGIN

LPlatformBackingStore::LPlatformBackingStore()
{

}

void LPlatformBackingStore::beginPaint(const std::shared_ptr<LRegion>)
{

}

void LPlatformBackingStore::endPaint()
{

}

std::shared_ptr<LWindow> LPlatformBackingStore::window()
{
    std::shared_ptr<LBackingStore> s = m_backingStore.lock();
    if(s != nullptr){
        return s->window();
    }
    return nullptr;
}

bool LPlatformBackingStore::scroll(const std::shared_ptr<LRegion> area, int dx, int dy)
{
    return false;
}

LK_NAMESPACE_END
