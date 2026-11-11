#include "lkgui/lbackingstore.h"
#include "lkgui/lguiapplication.h"
#include "lkgui/lhighdpiscaling.h"

LK_NAMESPACE_BEGIN

static bool isRasterSurface(std::shared_ptr<LWindow> window)
{
    switch (window->surfaceType()) {
    case LSurface::RasterSurface:
    case LSurface::RasterGLSurface:{
        return true;
    }
    default:{
        return false;
    }
    };
}

LBackingStore::LBackingStore(std::shared_ptr<LWindow> win): m_window(win)
{

}

void LBackingStore::resize(const LSize<int> &size)
{
    m_size = size;
    handle()->resize(LHighDpi::toNativePixels(size, m_window.get()), m_staticContents);
}

std::shared_ptr<LPlatformBackingStore> LBackingStore::handle() const
{
    if (m_platformBackingStore == nullptr) {
        const_cast<LBackingStore*>(this)->m_platformBackingStore =
            LGuiApplication::platformIntegration()->createPlatformBackingStore(m_window);
        std::shared_ptr<LBackingStore> q = getPtr();
        m_platformBackingStore->setBackingStore(q);
    }
    return m_platformBackingStore;
}

void LBackingStore::flush(const std::shared_ptr<LRegion> region,
                          std::shared_ptr<LWindow> window,
                          const LPoint<int> &offset)
{
    if (window == nullptr){
        window = m_window;
    }

    if(window ==nullptr){
        return;
    }

    if (!isRasterSurface(window)) {
        return;
    }

    std::shared_ptr<LPlatformWindow> pWin = window->handle();
    if (pWin ==nullptr) {
        return;
    }

    std::shared_ptr<LPlatformBackingStore> h = handle();
    h->flush(window,
             LHighDpi::toNativeLocalRegion(region, window.get()),
             LHighDpi::toNativeLocalPosition(offset, window.get()));
}

std::shared_ptr<LPaintDevice> LBackingStore::paintDevice()
{
    std::shared_ptr<LPaintDevice> device = handle()->paintDevice();
    if (LHighDpiScaling::isActive() && device->devType() == LPaintDevice::PDtImage){
        return m_highDpiBackingstore;
    }
    return device;
}

void LBackingStore::beginPaint(const std::shared_ptr<LRegion> region)
{
    if(m_highDpiBackingstore != nullptr){
        if (m_highDpiBackingstore->devicePixelRatio() != m_window->devicePixelRatio()){
            resize(size());
        }
    }

    std::shared_ptr<LPlatformBackingStore> platformBackingStore = handle();
    platformBackingStore->beginPaint(LHighDpi::toNativeLocalRegion(region, m_window.get()));

    // When Gui is applying a high-dpi scale factor the backing store
    // creates a "large" backing store image. This image needs to be
    // painted on as a high-dpi image, which is done by setting
    // devicePixelRatio. Do this on a separate image instance that shares
    // the image data to avoid having the new devicePixelRatio be propagated
    // back to the platform plugin.
    std::shared_ptr<LPaintDevice> device = platformBackingStore->paintDevice();
    if (LHighDpiScaling::isActive() && device->devType() == LPaintDevice::PDtImage) {
        std::shared_ptr<LImage> source =
            std::dynamic_pointer_cast<LImage>(device);
        if(source != nullptr){
            if (source != m_highDpiBackingstore) {
                m_highDpiBackingstore = source;
                double targetDevicePixelRatio = m_window->devicePixelRatio();
                m_highDpiBackingstore->setDevicePixelRatio(targetDevicePixelRatio);
            }
        }
    }
}

void LBackingStore::endPaint()
{
    handle()->endPaint();
}

bool LBackingStore::scroll(const LRect<int> area, int dx, int dy)
{
    const std::shared_ptr<LRegion> r = std::make_shared<LRegion>(area);
    return scroll(r, dx, dy);
}

bool LBackingStore::scroll(const std::shared_ptr<LRegion> area, int dx, int dy)
{
    const double nativeDx = LHighDpi::toNativePixels(double(dx), m_window.get());
    const double nativeDy = LHighDpi::toNativePixels(double(dy), m_window.get());
    if (std::floor(nativeDx) != nativeDx || std::floor(nativeDy) != nativeDy){
        return false;
    }

    std::shared_ptr<LPlatformBackingStore> h = handle();
    return h->scroll(LHighDpi::toNativeLocalRegion(area, m_window.get()), nativeDx, nativeDy);
}


LK_NAMESPACE_END
