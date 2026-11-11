#ifndef LSURFACE_H
#define LSURFACE_H
#include "lkcore/lglobal.h"
#include "lkcore/lflags.h"
#include "lsize.h"
#include <atomic>

LK_NAMESPACE_BEGIN

enum class LSurfaceFmtOpt {
    StereoBuffers = 0x0001,
    DebugContext = 0x0002,
    DeprecatedFunctions = 0x0004,
    ResetNotification = 0x0008
};
L_EN_FLAGS(LSurfaceFmtOpt)


class LKGUIDEF LSurfaceFormat{
public:
    enum SwapBehavior {
        DefaultSwapBehavior,
        SingleBuffer,
        DoubleBuffer,
        TripleBuffer
    };

    enum ColorSpace {
        DefaultColorSpace,
        sRGBColorSpace
    };

    LFlags<LSurfaceFmtOpt> m_opts = 0;
    int m_redBufferSize =-1;
    int m_greenBufferSize =-1;
    int m_blueBufferSize =-1;
    int m_alphaBufferSize =-1;
    int m_depthSize =-1;
    int m_stencilSize =-1;
    SwapBehavior m_swapBehavior =DefaultSwapBehavior;
    int m_numSamples =-1;

    int m_swapInterval =1;
    ColorSpace m_colorSpace = DefaultColorSpace;

    LSurfaceFormat(LFlags<LSurfaceFmtOpt> options = 0);
    bool hasAlpha() const;
};


class LPlatformSurface;
class LKGUIDEF LSurface
{
public:
    enum SurfaceClass {
        Window,
        Offscreen
    };

    enum SurfaceType {
        RasterSurface,
        OpenGLSurface,
        RasterGLSurface,
        OpenVGSurface,
        VulkanSurface,
        MetalSurface
    };

    LSurface();
    explicit LSurface(SurfaceClass type);

    inline SurfaceClass surfaceClass() const {
        return m_type;
    }

    virtual LSurfaceFormat format() const = 0;
    virtual std::shared_ptr<LPlatformSurface>  surfaceHandle() const = 0;

    virtual SurfaceType surfaceType() const = 0; 
    virtual LSize<int> size() const = 0;

protected:
    SurfaceClass m_type = Window;
};


LK_NAMESPACE_END
#endif // LSURFACE_H
