#ifndef LDIRECT2DWIN_H
#define LDIRECT2DWIN_H
#include "lkgui/osspec/win/lwindowswin.h"
#include <wrl.h>

struct IDXGISwapChain1;
struct ID2D1DeviceContext;

LK_NAMESPACE_BEGIN

class LDirect2DPixmap;
class LDirect2DWin : public LWindowsWin
{
public:
    LDirect2DWin(std::shared_ptr<LWindow> window, const LWindowsWindowData &data);
    virtual ~LDirect2DWin();

    virtual void setWindowFlags(LFlags<LWindowType> flags) override;

    std::shared_ptr<LPixmap> pixmap();
    void flush(std::shared_ptr<LDirect2DPixmap> dxPixmap,
               const std::shared_ptr<LRegion> region,
               const LPoint<int> &offset);
    void present(const std::shared_ptr<LRegion> region);
    void setupSwapChain();
    void resizeSwapChain(const LSize<int> &size);

    std::shared_ptr<LDirect2DPixmap> copyBackBuffer() const;
    std::shared_ptr<LDirect2DPixmap> directPixmap() const;
private:
    void setupPixmap();


    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_deviceContext;
    std::shared_ptr<LPixmap> m_pixmap;
    bool m_needsFullFlush = true;
    bool m_directRendering = false;
};

LK_NAMESPACE_END

#endif // LDIRECT2DWIN_H
