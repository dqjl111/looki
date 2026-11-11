#include "ldirect2dcontext.h"
#include <d3d11_1.h>
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <dxgi1_2.h>
#include <wrl.h>
#include <dwrite.h>
#include "lkcore/ldebug.h"
#include "ldirect2dintegration.h"

LK_NAMESPACE_BEGIN

LDirect2DContext::LDirect2DContext()
{

}

bool LDirect2DContext::init()
{
    HRESULT hr;

    D3D_FEATURE_LEVEL level;

    D3D_DRIVER_TYPE typeAttempts[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP
    };
    const int ntypes = int(sizeof(typeAttempts) / sizeof(typeAttempts[0]));

    for (int i = 0; i < ntypes; i++) {
        hr = D3D11CreateDevice(nullptr,
                               typeAttempts[i],
                               nullptr,
                               D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                               nullptr,
                               0,
                               D3D11_SDK_VERSION,
                               &m_d3dDevice,
                               &level,
                               &m_d3dDeviceContext);

        if (SUCCEEDED(hr))
            break;
    }

    if (FAILED(hr)) {
        lWarn() << "Could not create Direct3D Device";
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice;
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;

    hr = m_d3dDevice.As(&dxgiDevice);
    if (FAILED(hr)) {
        lWarn() << "DXGI Device interface query failed on D3D Device";
        return false;
    }

    // Ensure that DXGI doesn't queue more than one frame at a time.
    dxgiDevice->SetMaximumFrameLatency(1);

    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) {
        lWarn() <<"Failed to probe DXGI Device for parent DXGI Adapter";
        return false;
    }

    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&m_dxgiFactory));
    if (FAILED(hr)) {
        lWarn() << "Failed to probe DXGI Adapter for parent DXGI Factory";
        return false;
    }

    D2D1_FACTORY_OPTIONS options = {};


    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, m_d2dFactory.GetAddressOf());
    if (FAILED(hr)) {
        lWarn() << "Could not create Direct2D Factory";
        return false;
    }

    hr = m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice);
    if (FAILED(hr)) {
        lWarn() << "Could not create D2D Device";
        return false;
    }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             static_cast<IUnknown **>(&m_directWriteFactory));
    if (FAILED(hr)) {
        lWarn() << "Could not create DirectWrite factory";
        return false;
    }

    hr = m_directWriteFactory->GetGdiInterop(&m_directWriteGdiInterop);
    if (FAILED(hr)) {
        lWarn() << "Could not create DirectWrite GDI Interop";
        return false;
    }

    return true;
}

LDirect2DContext *LDirect2DContext::instance()
{
    return LDirect2DIntegration::instance()->direct2DContext();;
}

ID3D11Device *LDirect2DContext::d3dDevice() const
{
    return m_d3dDevice.Get();
}

ID2D1Device *LDirect2DContext::d2dDevice() const
{
    return m_d2dDevice.Get();
}

ID2D1Factory1 *LDirect2DContext::d2dFactory() const
{
    return m_d2dFactory.Get();
}

IDXGIFactory2 *LDirect2DContext::dxgiFactory() const
{
    return m_dxgiFactory.Get();
}

ID3D11DeviceContext *LDirect2DContext::d3dDeviceContext() const
{
    return m_d3dDeviceContext.Get();
}

IDWriteFactory *LDirect2DContext::dwriteFactory() const
{
    return m_directWriteFactory.Get();
}

IDWriteGdiInterop *LDirect2DContext::dwriteGdiInterop() const
{
    return m_directWriteGdiInterop.Get();
}

LK_NAMESPACE_END
