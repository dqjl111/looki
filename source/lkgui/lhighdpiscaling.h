#ifndef LHIGHDPISCALING_H
#define LHIGHDPISCALING_H
#include "lrect.h"
#include "lwindow.h"
#include <unordered_map>

LK_NAMESPACE_BEGIN

class LScreen;
class LPlatformScreen;
class LKGUIDEF LHighDpiScaling
{
public:
    LHighDpiScaling() = delete;
    ~LHighDpiScaling() = delete;

    LHighDpiScaling(const LHighDpiScaling &) = delete;
    LHighDpiScaling &operator=(const LHighDpiScaling &) = delete;
    LHighDpiScaling(LHighDpiScaling &&) = delete;
    LHighDpiScaling &operator=(LHighDpiScaling &&) = delete;

    enum class DpiAdjustmentPolicy {
        Unset,
        Enabled,
        Disabled,
        UpOnly
    };


    enum class HighDpiScaleRoundPolicy {
        Unset,
        Round,
        Ceil,
        Floor,
        RoundPreferFloor,
        PassThrough
    };

    static void initHighDpiScaling();
    static void updateHighDpiScaling();
    static void setGlobalFactor(double factor);
    static void setScreenFactor(LScreen *screen, double factor);

    static bool isActive() { return m_active; }

    struct ScaleAndOrigin
    {
        double factor;
        LPoint<int> origin;
    };
    static ScaleAndOrigin scaleAndOrigin(const LPlatformScreen *platformScreen, LPoint<int> *nativePosition = nullptr);
    static ScaleAndOrigin scaleAndOrigin(const LScreen *screen, LPoint<int> *nativePosition = nullptr);
    static ScaleAndOrigin scaleAndOrigin(const LWindow *window, LPoint<int> *nativePosition = nullptr);

    template<typename C>
    static double factor(C *context, LPoint<int> *nativePosition = nullptr) {
        return scaleAndOrigin(context, nativePosition).factor;
    }

    static LPoint<int> mapPositionFromNative(const LPoint<int> &pos, const LPlatformScreen *platformScreen);
    static LPoint<int> mapPositionToNative(const LPoint<int> &pos, const LPlatformScreen *platformScreen);
    static LPoint<int> mapPositionToGlobal(const LPoint<int> &pos, const LPoint<int> &windowGlobalPosition, const LWindow *window);
    static LPoint<int> mapPositionFromGlobal(const LPoint<int> &pos, const LPoint<int> &windowGlobalPosition, const LWindow *window);
    static std::pair<double, double> logicalDpi(const LScreen *screen);

    inline static void setUsePhysicalDpi(bool policy){
        m_usePhysicalDpi = policy;
    }
    inline static bool usePhysicalDpi(){
        return m_usePhysicalDpi;
    }

    inline static void setHighDpiScaleRoundPolicy(HighDpiScaleRoundPolicy policy){
        m_highDpiScaleRoundPolicy = policy;
    }
    inline static HighDpiScaleRoundPolicy highDpiScaleRoundPolicy(){
        return m_highDpiScaleRoundPolicy;
    }

    inline static void setDpiAdjustmentPolicy(DpiAdjustmentPolicy policy){
        m_dpiAdjustmentPolicy = policy;
    }
    inline static DpiAdjustmentPolicy dpiAdjustmentPolicy(){
        return m_dpiAdjustmentPolicy;
    }
private:
    static double rawScaleFactor(const LPlatformScreen *screen);
    static double roundScaleFactor(double rawFactor);
    static std::pair<double, double> effectiveLogicalDpi(const LPlatformScreen *screen, double rawFactor, double roundedFactor);
    static double screenSubfactor(const LPlatformScreen *screen);

    static double m_factor;
    static bool m_active;
    static bool m_usePixelDensity;
    static bool m_globalScalingActive;
    static bool m_pixelDensityScalingActive;
    static bool m_screenFactorSet;
    static std::pair<double, double> m_logicalDpi;
    static bool m_usePhysicalDpi;
    static HighDpiScaleRoundPolicy m_highDpiScaleRoundPolicy;
    static DpiAdjustmentPolicy m_dpiAdjustmentPolicy;
};

namespace LHighDpi {
template<typename T=int, typename C=int>
inline double scale(double value, double scaleFactor, LPoint<C> /* origin */ = LPoint<C>(0, 0))
{
    return value * scaleFactor;
}

template<typename T=int, typename C=int>
inline LPoint<T> scale(const LSize<T> &value, double scaleFactor, LPoint<C> /* origin */ = LPoint<C>(0, 0))
{
    return value * scaleFactor;
}

template<typename T=int, typename C=int>
inline LPoint<T> scale(const LPoint<T> &pos, double scaleFactor, LPoint<C> origin = LPoint<C>(0, 0))
{
    return LPoint<T>((pos.m_x-origin.m_x)*scaleFactor+origin.m_x,
                     (pos.m_y-origin.m_y)*scaleFactor+origin.m_y);
}

template<typename C=int>
inline LPoint<int> scale(const LPoint<int> &pos, double scaleFactor, LPoint<C> origin = LPoint<C>(0, 0))
{
    return LPoint<int>(std::lround((pos.m_x-origin.m_x)*scaleFactor+origin.m_x),
                       std::lround((pos.m_y-origin.m_y)*scaleFactor+origin.m_y));
}

template<typename T=int, typename C=int>
inline LRect<T> scale(const LRect<T> &rect, double scaleFactor, LPoint<C> origin = LPoint<C>(0, 0))
{
    LPoint<T> topLeft = scale(rect.topLeft(), scaleFactor, origin);
    return LRect<T>(topLeft, rect.m_width*scaleFactor, rect.m_height*scaleFactor);
}


template<typename T=int, typename C=int>
inline LMargins<T> scale(const LMargins<T> &margins, double scaleFactor, LPoint<C> /*origin*/ = LPoint<C>(0, 0))
{
    return LMargins<T>((T)std::lround(margins.m_left * scaleFactor),
                       (T)std::lround(margins.m_top * scaleFactor),
                       (T)std::lround(margins.m_right * scaleFactor),
                       (T)std::lround(margins.m_bottom * scaleFactor));
}

inline std::shared_ptr<LRegion> scale(std::shared_ptr<LRegion> region,
                                      double scaleFactor,
                                      LPoint<int> origin = LPoint<int>(0, 0))
{
    if (!LHighDpiScaling::isActive()){
        return region;
    }

    std::shared_ptr<LRegion> scaled = std::make_shared<LRegion>();
    for (const LRect<int> &rect : *region){
        scaled->unite(scale(rect.toRectF(), scaleFactor, origin.toPointF()).toRect());
    }
    return scaled;
}


template <typename N, typename T=int>
inline N fromNative(const N &value, double scaleFactor, LPoint<T> origin = LPoint<T>(0, 0))
{
    return scale(value, double(1) / scaleFactor, origin);
}

template <typename N, typename T=int>
inline N toNative(const N &value, double scaleFactor, LPoint<T> origin = LPoint<T>(0, 0))
{
    return scale(value, scaleFactor, origin);
}

inline LRect<int> fromNative(const LRect<int> &rect, const LScreen *screen, const LPoint<int> &screenOrigin)
{
    return scale(rect, double(1) / LHighDpiScaling::factor(screen), screenOrigin);
}

inline LRect<int> fromNativeScreenGeometry(const LRect<int> &nativeScreenGeometry, const LScreen *screen)
{
    return LRect<int>(nativeScreenGeometry.topLeft(),
                 scale(nativeScreenGeometry.size(), double(1) / LHighDpiScaling::factor(screen)));
}

template <typename T>
inline LPoint<int> position(T) { return LPoint<int>(); }
inline LPoint<int> position(LPoint<int> point) { return point; }
inline LPoint<int> position(LPoint<double> point) { return point.toPoint(); }
inline LPoint<int> position(LRect<int> rect) { return rect.center(); }
inline LPoint<int> position(LRect<double> rect) { return rect.center().toPoint(); }


template <typename T, typename C>
T fromNativePixels(const T &value, const C *context)
{
    LPoint<int> nativePosition = position(value);
    LHighDpiScaling::ScaleAndOrigin so = LHighDpiScaling::scaleAndOrigin(context, &nativePosition);
    return scale(value, double(1) / so.factor, so.origin);
}

template <typename T, typename C>
T toNativePixels(const T &value, const C *context)
{
    LHighDpiScaling::ScaleAndOrigin so = LHighDpiScaling::scaleAndOrigin(context);
    return scale(value, so.factor, so.origin);
}


template <typename T, typename C>
T fromNativeLocalPosition(const T &value, const C *context)
{
    return scale(value, double(1) / LHighDpiScaling::factor(context));
}

template <typename T, typename C>
T toNativeLocalPosition(const T &value, const C *context)
{
    return scale(value, LHighDpiScaling::factor(context));
}


inline std::shared_ptr<LRegion> toNativeLocalRegion(std::shared_ptr<LRegion> pointRegion, const LWindow *window)
{
    return scale(pointRegion, LHighDpiScaling::factor(window));
}


inline std::shared_ptr<LRegion> fromNativeLocalExposedRegion(const std::shared_ptr<LRegion> pixelRegion,
                                                             const LWindow *window)
{
    if (!LHighDpiScaling::isActive()){
        return pixelRegion;
    }

    const double scaleFactor = LHighDpiScaling::factor(window);
    std::shared_ptr<LRegion> pointRegion = std::make_shared<LRegion>();
    for (const LRect<int>& rect: *pixelRegion) {
        LRect<double> t= rect.toRectF();
        const LPoint<double> topLeftP = t.topLeft() / scaleFactor;
        const LSize<double> sizeP = t.size() / scaleFactor;
        LPoint<int> tl(std::floor(topLeftP.m_x), std::floor(topLeftP.m_y));
        LPoint<int> br(std::ceil(topLeftP.m_x + sizeP.width()  - 1.0),
                       std::ceil(topLeftP.m_y + sizeP.height() - 1.0));

        pointRegion->unite(LRect<int>(tl, br));
    }
    return pointRegion;
}

}

LK_NAMESPACE_END
#endif // LHIGHDPISCALING_H
