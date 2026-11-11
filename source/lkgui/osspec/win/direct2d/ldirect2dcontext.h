#ifndef LDIRECT2DCONTEXT_H
#define LDIRECT2DCONTEXT_H

#include <d3d11_1.h>
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <dxgi1_2.h>
#include <wrl.h>
#include <dwrite.h>
#include "lkgui/lrect.h"
#include "lkgui/osspec/win/lwindows.h"
#include "lkgui/paint/lcolor.h"
#include "lkgui/paint/ltransform2d.h"

struct ID3D11Device;
struct ID2D1Device;
struct ID2D1Factory1;
struct IDXGIFactory2;
struct ID3D11DeviceContext;
struct IDWriteFactory;
struct IDWriteGdiInterop;


LK_NAMESPACE_BEGIN

template<typename T>
extern inline D2D1_RECT_F toD2dRectF(const LRect<T> &rectF){
    return D2D1::RectF(rectF.m_x, rectF.m_y, rectF.x2(), rectF.y2());
}

extern inline D2D1_RECT_U toD2dRectU(const LRect<int> &rect){
    return D2D1::RectU(rect.m_x, rect.m_y, rect.x2(), rect.y2());
}

template<typename T>
inline D2D1_POINT_2F toD2dPoint2F(const LPoint<T> &p)
{
    return D2D1::Point2F(p.m_x, p.m_y);
}


extern inline D2D1::ColorF toD2dColorF(const LColor &c)
{
    return D2D1::ColorF(c.m_red/255.0f, c.m_green/255.0f, c.m_blue/255.0f, c.m_alpha/255.0f);
}


extern inline D2D1_MATRIX_3X2_F toD2dMatrix3x2F(const LTransform2D &transform)
{
    return D2D1::Matrix3x2F(transform.m11(), transform.m12(),
                            transform.m21(), transform.m22(),
                            transform.m31(), transform.m32());
}


class LDirect2DContext
{
public:
    LDirect2DContext();


    bool init();

    static LDirect2DContext *instance();

    ID3D11Device *d3dDevice() const;
    ID2D1Device *d2dDevice() const;
    ID2D1Factory1 *d2dFactory() const;
    IDXGIFactory2 *dxgiFactory() const;
    ID3D11DeviceContext *d3dDeviceContext() const;
    IDWriteFactory *dwriteFactory() const;
    IDWriteGdiInterop *dwriteGdiInterop() const;

private:
    Microsoft::WRL::ComPtr<ID3D11Device>  m_d3dDevice;
    Microsoft::WRL::ComPtr<ID2D1Factory1> m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device>   m_d2dDevice;
    Microsoft::WRL::ComPtr<IDXGIFactory2>  m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dDeviceContext;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_directWriteFactory;
    Microsoft::WRL::ComPtr<IDWriteGdiInterop> m_directWriteGdiInterop;
};

LK_NAMESPACE_END

#endif // LDIRECT2DCONTEXT_H
