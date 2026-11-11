#ifndef LBACKINGSTORE_H
#define LBACKINGSTORE_H
#include "lkgui/paint/lregion.h"
#include "lkgui/lwindow.h"
#include "lkgui/lplatformbackingstore.h"
#include "lkgui/image/limage.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LBackingStore : public std::enable_shared_from_this<LBackingStore>
{
public:
    LBackingStore(std::shared_ptr<LWindow> win);

    void resize(const LSize<int> &size);
    inline LSize<int> size() const{
        return m_size;
    }


    inline bool hasStaticContents() const{
        if(m_staticContents != nullptr){
            return !m_staticContents->isEmpty();
        }
        return false;
    }

    std::shared_ptr<LPlatformBackingStore> handle() const;

    inline void setStaticContents(const std::shared_ptr<LRegion> region){
        m_staticContents = region;
    }
    inline std::shared_ptr<LRegion> staticContents() const{
        return m_staticContents;
    }

    inline std::shared_ptr<LBackingStore> getPtr() const{
        return const_cast<LBackingStore*>(this)->shared_from_this();
    }

    void flush(const std::shared_ptr<LRegion> region,
               std::shared_ptr<LWindow> window = nullptr,
               const LPoint<int> &offset = LPoint<int>());

    std::shared_ptr<LPaintDevice> paintDevice();

    void beginPaint(const std::shared_ptr<LRegion> region);
    void endPaint();

    inline std::shared_ptr<LWindow> window(){
        return m_window;
    }

    bool scroll(const LRect<int> area, int dx, int dy);
    bool scroll(const std::shared_ptr<LRegion> area, int dx, int dy);
protected:
    std::shared_ptr<LWindow> m_window;
    std::shared_ptr<LPlatformBackingStore> m_platformBackingStore;
    LSize<int> m_size;
    std::shared_ptr<LRegion> m_staticContents;
    std::shared_ptr<LImage> m_highDpiBackingstore;

};

LK_NAMESPACE_END

#endif // LBACKINGSTORE_H
