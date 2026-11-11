#ifndef LPLATFORMSURFACE_H
#define LPLATFORMSURFACE_H
#include "lsurface.h"
#include <memory>

LK_NAMESPACE_BEGIN

class LPlatformScreen;
class LKGUIDEF LPlatformSurface  : public std::enable_shared_from_this<LPlatformSurface>
{
public:
    LPlatformSurface(std::shared_ptr<LSurface> surface = nullptr);

    virtual ~LPlatformSurface();
    virtual LSurfaceFormat format() const;

    std::shared_ptr<LSurface> surface() const{
        return m_surface.lock();
    }
    virtual std::shared_ptr<LPlatformScreen> screen() const;

    inline std::shared_ptr<LPlatformSurface> getPtr() const{
        return const_cast<LPlatformSurface*>(this)->shared_from_this();
    }

    template<class T>
    std::shared_ptr<T> getPtrT() const{
        std::shared_ptr<LPlatformSurface> t = const_cast<LPlatformSurface*>(this)->shared_from_this();
        return std::dynamic_pointer_cast<T>(t);
    }
protected:
    std::weak_ptr<LSurface> m_surface;
};



LK_NAMESPACE_END
#endif // LPLATFORMSURFACE_H
