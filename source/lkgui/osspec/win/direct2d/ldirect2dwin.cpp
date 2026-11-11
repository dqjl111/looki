#include "ldirect2dwin.h"
#include "ldirect2dcontext.h"
#include "ldirect2dpixmap.h"
#include "ldirect2dpaintengine.h"
#include <d3d11.h>
#include <d2d1_1.h>
#include <dxgi1_2.h>

LK_NAMESPACE_BEGIN

LDirect2DWin::LDirect2DWin(std::shared_ptr<LWindow> window, const LWindowsWindowData &data)
    :LWindowsWin(window, data)
    , m_directRendering(!(data.flags & LWindowType::FramelessWindowHint && window->format().hasAlpha()))
{
    if (window->type() == LWindowType::Desktop){
        return;
    }

    if (m_directRendering){
        setupSwapChain();
    }

    HRESULT hr = LDirect2DContext::instance()->d2dDevice()->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        m_deviceContext.GetAddressOf());
    if (FAILED(hr)){
        lWarn() << "Couldn't create Direct2D Device context";
    }
}

LDirect2DWin::~LDirect2DWin()
{
//    if(m_pixmap != nullptr){
//        std::shared_ptr<LImage> img = m_pixmap->toImage();
//        img->save("G:\\testDxWin.bmp");
//    }
}

void LDirect2DWin::setWindowFlags(LFlags<LWindowType> flags)
{
    m_directRendering = !(flags & LWindowType::FramelessWindowHint && window()->format().hasAlpha());
    if (!m_directRendering){
        m_swapChain.Reset(); // No need for the swap chain; release from memory
    }else if (!m_swapChain){
        setupSwapChain();
    }

    LWindowsWin::setWindowFlags(flags);
}

std::shared_ptr<LPixmap> LDirect2DWin::pixmap()
{
    setupPixmap();
    return m_pixmap;
}

void LDirect2DWin::flush(std::shared_ptr<LDirect2DPixmap> dxPixmap,
                         const std::shared_ptr<LRegion> region,
                         const LPoint<int> &offset)
{
    if(dxPixmap == nullptr){
        return;
    }

    ID2D1Bitmap1* bitmapIn = dxPixmap->bitmap();
    if(bitmapIn == nullptr){
        return;
    }

    LSize<int> size;
    if (m_directRendering) {
        DXGI_SWAP_CHAIN_DESC1 desc;
        HRESULT hr = m_swapChain->GetDesc1(&desc);
        LRect<int> geom = geometry();

        if (FAILED(hr) || (desc.Width != UINT(geom.width()) || (desc.Height != UINT(geom.height())))) {
            resizeSwapChain(geom.size());
            m_swapChain->GetDesc1(&desc);
        }
        size.setWidth(int(desc.Width));
        size.setHeight(int(desc.Height));
    } else {
        size = geometry().size();
    }

    setupPixmap();
    std::shared_ptr<LDirect2DPixmap> dp = directPixmap();
    if (dp == nullptr){
        return;
    }

    if (dxPixmap != dp) {
        dp->beginPaint();

        ID2D1DeviceContext *dc = dp->deviceContext();
        if (!m_needsFullFlush) {
            std::shared_ptr<LRegion> clipped = std::make_shared<LRegion>(LRect<int>(0,0, size));
            clipped->intersect(region);

            for (const LRect<int> &rect : *clipped) {
                LRect<double> rectF = rect.toRectF();
                dc->DrawBitmap(bitmapIn,
                               toD2dRectF(rectF),
                               1.0,
                               D2D1_INTERPOLATION_MODE_LINEAR,
                               toD2dRectF(rectF.translated(offset.m_x, offset.m_y)));
            }
        } else {
            LRect<double> rectF(0, 0, size.toSizeF());
            dc->DrawBitmap(bitmapIn,
                           toD2dRectF(rectF),
                           1.0,
                           D2D1_INTERPOLATION_MODE_LINEAR,
                           toD2dRectF(rectF.translated(offset.m_x, offset.m_y)));
            m_needsFullFlush = false;
        }

        dp->endPaint();
    }
}

void LDirect2DWin::present(const std::shared_ptr<LRegion> region)
{
    std::shared_ptr<LDirect2DPixmap> dp = directPixmap();
    if (dp == nullptr){
        return;
    }

    ID2D1Bitmap1* bitmap = dp->bitmap();
    if(bitmap == nullptr){
        return;
    }

    if (m_directRendering) {
        m_swapChain->Present(0, 0);
        return;
    }

    Microsoft::WRL::ComPtr<IDXGISurface> bitmapSurface;
    HRESULT hr = bitmap->GetSurface(&bitmapSurface);

    Microsoft::WRL::ComPtr<IDXGISurface1> dxgiSurface;
    hr = bitmapSurface.As(&dxgiSurface);

    HDC hdc;
    hr = dxgiSurface->GetDC(FALSE, &hdc);
    if (FAILED(hr)) {
        lFatal() << "Failed to get DC for presenting the surface";
        return;
    }

    const LRect<int> bounds = window()->geometry();
    const SIZE size = { bounds.width(), bounds.height() };
    const POINT ptDst = { bounds.m_x, bounds.m_y };
    const POINT ptSrc = { 0, 0 };
    const BLENDFUNCTION blend = { AC_SRC_OVER, 0, BYTE(255.0 * opacity()), AC_SRC_ALPHA };
    const LRect<int> r = region->boundingRect();
    const RECT dirty = { r.m_x, r.m_y, r.x2(), r.y2() };
    UPDATELAYEREDWINDOWINFO info = { sizeof(UPDATELAYEREDWINDOWINFO), nullptr,
                                    &ptDst, &size, hdc, &ptSrc, 0, &blend, ULW_ALPHA, &dirty };
    if (!UpdateLayeredWindowIndirect(handle(), &info)){
        lFatal() <<  "Failed to update the layered window";
    }

    hr = dxgiSurface->ReleaseDC(nullptr);
    if (FAILED(hr)){
        lFatal() << "Failed to release the DC for presentation";
    }
}

void LDirect2DWin::setupSwapChain()
{
    DXGI_SWAP_CHAIN_DESC1 desc = {};

    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1;
    desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

    HRESULT hr = LDirect2DContext::instance()->dxgiFactory()->CreateSwapChainForHwnd(
        LDirect2DContext::instance()->d3dDevice(), // [in]   IUnknown *pDevice
        handle(),                                         // [in]   HWND hWnd
        &desc,                                            // [in]   const DXGI_SWAP_CHAIN_DESC1 *pDesc
        nullptr,                                          // [in]   const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc
        nullptr,                                          // [in]   IDXGIOutput *pRestrictToOutput
        m_swapChain.ReleaseAndGetAddressOf());            // [out]  IDXGISwapChain1 **ppSwapChain

    if (FAILED(hr)){
        lWarn() << "Could not create swap chain";
    }
    m_needsFullFlush = true;
}

void LDirect2DWin::resizeSwapChain(const LSize<int> &size)
{
    m_pixmap = nullptr;
    m_deviceContext->SetTarget(nullptr);
    m_needsFullFlush = true;

    if (!m_swapChain){
        return;
    }

    HRESULT hr = m_swapChain->ResizeBuffers(0,
                                            UINT(size.width()), UINT(size.height()),
                                            DXGI_FORMAT_UNKNOWN,
                                            0);
    if (FAILED(hr)){
        lWarn() << "Could not resize swap chain size:" << size.width() << "," << size.height();
    }
}


std::shared_ptr<LDirect2DPixmap> LDirect2DWin::copyBackBuffer() const
{
    std::shared_ptr<LDirect2DPixmap> org = directPixmap();
    if (org == nullptr){
        return nullptr;
    }
    ID2D1Bitmap1* bitmap = org->bitmap();
    if(bitmap == nullptr){
        return nullptr;
    }

    D2D1_PIXEL_FORMAT format = bitmap->GetPixelFormat();
    D2D1_SIZE_U size = bitmap->GetPixelSize();

    FLOAT dpiX, dpiY;
    bitmap->GetDpi(&dpiX, &dpiY);

    D2D1_BITMAP_PROPERTIES1 properties = {
        format,                     // D2D1_PIXEL_FORMAT pixelFormat;
        dpiX,                       // FLOAT dpiX;
        dpiY,                       // FLOAT dpiY;
        D2D1_BITMAP_OPTIONS_TARGET, // D2D1_BITMAP_OPTIONS bitmapOptions;
        nullptr                   // _Field_size_opt_(1) ID2D1ColorContext *colorContext;
    };
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> copy;
    HRESULT hr = m_deviceContext->CreateBitmap(size, nullptr, 0, properties, &copy);

    if (FAILED(hr)) {
        lWarn() << "Could not create staging bitmap";
        return nullptr;
    }

    hr = copy->CopyFromBitmap(nullptr, bitmap, nullptr);
    if (FAILED(hr)) {
        lWarn() << "Could not copy from bitmap";
        return nullptr;
    }

    std::shared_ptr<LDirect2DPixmap> ret = std::make_shared<LDirect2DPixmap>(copy.Get(), nullptr);
    return ret;
}

std::shared_ptr<LDirect2DPixmap> LDirect2DWin::directPixmap() const
{
    if(m_pixmap != nullptr){
        return std::dynamic_pointer_cast<LDirect2DPixmap>(m_pixmap->handle());
    }
    return nullptr;
}

void LDirect2DWin::setupPixmap()
{
    if(m_pixmap != nullptr){
        return;
    }

    if (m_deviceContext == nullptr){
        return;
    }

    if (m_directRendering && (m_swapChain == nullptr)){
        return;
    }

    HRESULT hr;
    Microsoft::WRL::ComPtr<IDXGISurface1> backBufferSurface;
    if (m_directRendering) {
        hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBufferSurface));
        if (FAILED(hr)) {
            lWarn() << "Could not query backbuffer for DXGI Surface";
            return;
        }
    } else {
        const LRect<int> rect = geometry();
        CD3D11_TEXTURE2D_DESC backBufferDesc(DXGI_FORMAT_B8G8R8A8_UNORM, UINT(rect.width()), UINT(rect.height()), 1, 1);
        backBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
        backBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBufferTexture;
        HRESULT hr = LDirect2DContext::instance()->d3dDevice()->CreateTexture2D(&backBufferDesc, nullptr, &backBufferTexture);
        if (FAILED(hr)) {
            lWarn() << "Failed to create backing texture for indirect rendering";
            return;
        }

        hr = backBufferTexture.As(&backBufferSurface);
        if (FAILED(hr)) {
            lWarn() << "Failed to cast back buffer surface to DXGI surface";
            return;
        }
    }

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> backBufferBitmap;
    hr = m_deviceContext->CreateBitmapFromDxgiSurface(backBufferSurface.Get(), nullptr, backBufferBitmap.GetAddressOf());
    if (FAILED(hr)) {
        lWarn() << "Could not create Direct2D Bitmap from DXGI Surface";
        return;
    }

    uint32_t flags = LDirect2DPaintEngine::NoFlag;
    if (!m_directRendering){
        flags |= LDirect2DPaintEngine::TranslucentTopLevelWindow;
    }

    std::shared_ptr<LDirect2DPixmap> dp = std::make_shared<LDirect2DPixmap>(backBufferBitmap.Get(),
                                                                            m_deviceContext.Get(),
                                                                            flags);
    m_pixmap = std::make_shared<LPixmap>(dp);
}

LK_NAMESPACE_END
