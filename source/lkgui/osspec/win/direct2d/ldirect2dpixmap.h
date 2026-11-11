#ifndef LDIRECT2DPIXMAP_H
#define LDIRECT2DPIXMAP_H
#include "lkgui/image/lplatformpixmap.h"
#include <wrl.h>
#include <d2d1_1.h>

class ID2D1Bitmap1;
class ID2D1DeviceContext;

LK_NAMESPACE_BEGIN
class LDirect2DPaintEngine;
class LDirect2DPixmap : public LPlatformPixmap
{
public:
    LDirect2DPixmap(uint32_t dxEngineFlags =0);
    LDirect2DPixmap(ID2D1Bitmap1 *bitmap, ID2D1DeviceContext *dc, uint32_t dxEngineFlags = 0);

    virtual std::shared_ptr<LPaintEngine> paintEngine() const override;
    virtual bool resize(int width, int height) override;

    ID2D1DeviceContext* deviceContext();
    ID2D1Bitmap1* bitmap();

    void beginPaint();
    void endPaint();
    void suspendPaint();
    void resumePaint();

    virtual bool hasAlphaChannel() const override;
    virtual double devicePixelRatio() const override;
    virtual void setDevicePixelRatio(double scaleFactor) override;

    virtual int metric(LPaintDevice::Metric m) const override;
    virtual void fromImage(const std::shared_ptr<LImage> image,
                           LFlags<LImageConvertFlag> flags) override;

    virtual std::shared_ptr<LImage> toImage() const override;
    virtual void copy(const std::shared_ptr<LPlatformPixmap> other, const LRect<int> &rect) override;
    virtual bool isNull()  override;
    void copy(const std::shared_ptr<LPlatformPixmap> other, std::shared_ptr<LRegion> r);
private:
    bool resizeHelper(int width, int height, const void *data = nullptr, int pitch = 0);

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_bitmap;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_deviceContext;

    std::shared_ptr<LDirect2DPaintEngine>  m_paintEngine;
    int m_paintCount =0;
    uint32_t m_engineInitFlags = 0;
    double m_devicePixelRatio = 1.0;
};

LK_NAMESPACE_END

#endif // LDIRECT2DBITMAP_H
