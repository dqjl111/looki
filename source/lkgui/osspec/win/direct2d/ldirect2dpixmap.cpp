#include "ldirect2dpixmap.h"
#include "lkcore/ldebug.h"
#include "ldirect2dcontext.h"
#include "ldirect2dpaintengine.h"

LK_NAMESPACE_BEGIN

D2D1_BITMAP_PROPERTIES1 bitmapProperties()
{
    FLOAT dpiX, dpiY;
    LDirect2DContext::instance()->d2dFactory()->GetDesktopDpi(&dpiX, &dpiY);

    return D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                          D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpiX, dpiY);

}


LDirect2DPixmap::LDirect2DPixmap(uint32_t dxEngineFlags)
{
    HRESULT hr = LDirect2DContext::instance()->d2dDevice()->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        &m_deviceContext);
    if (L_UNLIKELY(FAILED(hr))){
        lFatal() << "Couldn't create Direct2D Device Context";
    }

    m_engineInitFlags = dxEngineFlags;
    m_depth = 32;
}

LDirect2DPixmap::LDirect2DPixmap(ID2D1Bitmap1 *bitmap, ID2D1DeviceContext *dc, uint32_t dxEngineFlags)
    :m_bitmap(bitmap), m_deviceContext(dc)
{
    if (dc == nullptr) {
        HRESULT hr = LDirect2DContext::instance()->d2dDevice()->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
            &m_deviceContext);
        if (L_UNLIKELY(FAILED(hr))){
            lFatal() << "Couldn't create Direct2D Device Context";
        }
    }
    m_deviceContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
    m_engineInitFlags = dxEngineFlags;
    m_depth = 32;

    if(m_bitmap != nullptr){
        D2D1_SIZE_U size = m_bitmap->GetPixelSize();
        m_width = size.width;
        m_height = size.height;
        m_deviceContext->SetTarget(m_bitmap.Get());
    }
}


std::shared_ptr<LPaintEngine> LDirect2DPixmap::paintEngine() const
{
    if(m_paintEngine == nullptr){
        const_cast<LDirect2DPixmap*>(this)->m_paintEngine =
            std::make_shared<LDirect2DPaintEngine>(m_engineInitFlags);
    }
    return m_paintEngine;
}

bool LDirect2DPixmap::resize(int width, int height)
{
    if(resizeHelper(width, height)){
        m_width = width;
        m_height = height;
        return true;
    }else{
        m_width = 0;
        m_height = 0;
        return false;
    }
}

ID2D1DeviceContext *LDirect2DPixmap::deviceContext()
{
    return m_deviceContext.Get();
}

ID2D1Bitmap1 *LDirect2DPixmap::bitmap()
{
    return m_bitmap.Get();
}

void LDirect2DPixmap::beginPaint()
{
    if(m_deviceContext != nullptr){
        if(m_paintCount <= 0){
            m_deviceContext->BeginDraw();
            //lWarn() << " Begin LDirect2DPixmap draw";
            m_paintCount = 1;
        }else{
            m_paintCount++;
        }
    }
}

void LDirect2DPixmap::endPaint()
{
    if(m_deviceContext != nullptr){
        if(m_paintCount >0){
            m_paintCount--;
            if(m_paintCount ==0){
                m_deviceContext->EndDraw();
            }
        }
    }
}

void LDirect2DPixmap::suspendPaint()
{
    if(m_deviceContext != nullptr){
        if(m_paintCount >0){
            m_deviceContext->EndDraw();
        }
    }
}

void LDirect2DPixmap::resumePaint()
{
    if(m_deviceContext != nullptr){
        if(m_paintCount >0){
            m_deviceContext->BeginDraw();
        }
    }
}

bool LDirect2DPixmap::hasAlphaChannel() const
{
    return true;
}

double LDirect2DPixmap::devicePixelRatio() const
{
    return m_devicePixelRatio;
}

void LDirect2DPixmap::setDevicePixelRatio(double scaleFactor)
{
    m_devicePixelRatio = scaleFactor;
}


int LDirect2DPixmap::metric(LPaintDevice::Metric metric) const
{
    switch (metric) {
    case LPaintDevice::PdmWidth:{
        return m_width;
    }
    case LPaintDevice::PdmHeight:{
        return m_height;
    }
    case LPaintDevice::PdmNumColors:{
        return INT_MAX;
    }
    case LPaintDevice::PdmDepth:{
        return 32;
    }
    case LPaintDevice::PdmDpiX:
    case LPaintDevice::PdmPhysicalDpiX:
    {
        FLOAT x, y;
        LDirect2DContext::instance()->d2dFactory()->GetDesktopDpi(&x, &y);
        return std::lround(x);
    }
    case LPaintDevice::PdmDpiY:
    case LPaintDevice::PdmPhysicalDpiY:
    {
        FLOAT x, y;
        LDirect2DContext::instance()->d2dFactory()->GetDesktopDpi(&x, &y);
        return std::lround(y);
    }
    case LPaintDevice::PdmDevicePixelRatio:
        return 1;
    case LPaintDevice::PdmDevicePixelRatioScaled:{
        return std::lround(LPaintDevice::devicePixelRatioFScale());
    }
    case LPaintDevice::PdmWidthMM:
    case LPaintDevice::PdmHeightMM:{
        break;
    }
    default:{
        break;
    }
    }

    return -1;
}

void LDirect2DPixmap::fromImage(const std::shared_ptr<LImage> image, LFlags<LImageConvertFlag> flags)
{
    std::shared_ptr<LImage> converted =
        image->convertToFormat(LImage::Format::ARGB32Premultiplied, flags, true);

    if(resizeHelper(converted->width(), converted->height(), converted->bits(), converted->bytesPerLine())){
        m_width = converted->width();
        m_height = converted->height();
        m_devicePixelRatio = image->devicePixelRatio();
    }else{
        m_width = 0;
        m_height = 0;
        m_devicePixelRatio = 1.0;
    }
}

std::shared_ptr<LImage> LDirect2DPixmap::toImage() const
{
    if (m_bitmap == nullptr){
        return std::make_shared<LImage>();
    }

    LDirect2DPaintEngineSuspender suspender(m_paintEngine);

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> mappingCopy;

    HRESULT hr = S_OK;
    D2D1_SIZE_U size = m_bitmap->GetPixelSize();

    D2D1_BITMAP_PROPERTIES1 properties = bitmapProperties();
    properties.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ;

    hr = m_deviceContext->CreateBitmap(size, nullptr, 0, properties, &mappingCopy);
    if (FAILED(hr)) {
        lWarn() << "Could not create bitmap";
        return std::make_shared<LImage>();
    }

    hr = mappingCopy->CopyFromBitmap(nullptr, m_bitmap.Get(), nullptr);
    if (FAILED(hr)) {
        lWarn() << "Could not copy from bitmap";
        return std::make_shared<LImage>();
    }

    D2D1_MAPPED_RECT mappedRect;
    hr = mappingCopy->Map(D2D1_MAP_OPTIONS_READ, &mappedRect);
    if (FAILED(hr)) {
        lWarn() << "Could not map bitmap";
        return std::make_shared<LImage>();
    }
    std::shared_ptr<LImage>  ret =  std::make_shared<LImage>(mappedRect.bits,
                                        int(size.width),
                                        int(size.height),
                                        int(mappedRect.pitch),
                                        LImage::Format::ARGB32Premultiplied,
                                        false);
    ret->setDevicePixelRatio(m_devicePixelRatio);
    return ret;
}

void LDirect2DPixmap::copy(const std::shared_ptr<LPlatformPixmap> oldPixmap, const LRect<int> &rect)
{
    if(oldPixmap != nullptr && rect.isValid() ){
        if(m_bitmap == nullptr){
            resize(oldPixmap->width(), oldPixmap->height());
        }

        ID2D1Bitmap1* t = m_bitmap.Get();
        if(t != nullptr){
            std::shared_ptr<LDirect2DPixmap> oldDp = std::dynamic_pointer_cast<LDirect2DPixmap>(oldPixmap);
            if (oldDp != nullptr ) {
                ID2D1Bitmap1* pOld = oldDp->m_bitmap.Get();
                if(pOld != nullptr){
                    D2D1_RECT_U src = toD2dRectU(rect);
                    HRESULT hr = t->CopyFromBitmap(nullptr, pOld, &src);
                    if (FAILED(hr)) {
                        lWarn() << "Could not copy from direct2d bitmap";
                        return;
                    }
                }
            }else{
                LPlatformPixmap::copy(oldPixmap, rect);
            }
        }
    }
}

bool LDirect2DPixmap::isNull()
{
    return m_bitmap == nullptr;
}

void LDirect2DPixmap::copy(std::shared_ptr<LPlatformPixmap> oldPixmap, std::shared_ptr<LRegion> region)
{
    if(oldPixmap != nullptr && region != nullptr ){
        if(m_bitmap == nullptr){
            resize(oldPixmap->width(), oldPixmap->height());
        }

        ID2D1Bitmap1* t = m_bitmap.Get();
        if(t != nullptr){
            std::shared_ptr<LDirect2DPixmap> oldDp = std::dynamic_pointer_cast<LDirect2DPixmap>(oldPixmap);
            if (oldDp != nullptr ) {
                ID2D1Bitmap1* pOld = oldDp->m_bitmap.Get();
                if(pOld != nullptr){
                    for (const LRect<int> &rect : *region){
                        D2D1_RECT_U src = toD2dRectU(rect);
                        t->CopyFromBitmap(nullptr, pOld, &src);
                    }
                }
            }else{
                std::shared_ptr<LImage> image = oldPixmap->toImage();
                if(image != nullptr){
                    for (const LRect<int> &rect : *region){
                        std::shared_ptr<LImage> t = image->copy(rect);
                        fromImage(t, LImageConvertFlag::NoOpaqueDetection);
                    }
                }
            }
        }
    }
}

bool LDirect2DPixmap::resizeHelper(int width, int height, const void *data, int pitch)
{
    m_deviceContext->SetTarget(nullptr);
    m_bitmap.Reset();

    D2D1_SIZE_U size = {
        UINT32(width), UINT32(height)
    };

    HRESULT hr = m_deviceContext->CreateBitmap(size, data, UINT32(pitch),
                                               bitmapProperties(),
                                               m_bitmap.ReleaseAndGetAddressOf());
    if (SUCCEEDED(hr)){
        m_deviceContext->SetTarget(m_bitmap.Get());
        return true;
    }else{
        lWarn() << "Could not create LDirect2DPixmap";
        return false;
    }

}

LK_NAMESPACE_END
